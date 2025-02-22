/**
 * @file
 * HTTPD example for simple POST
 */
 
 /*
 * Copyright (c) 2017 Simon Goldschmidt
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Simon Goldschmidt <goldsimon@gmx.de>
 *
 */

#include "lwip/opt.h"

#include "lwip/apps/httpd.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "post_handler.h"
#include <stdio.h>
#include <string.h>
#include <ssi.h>
#include <flash_io.h>

/** define LWIP_HTTPD_EXAMPLE_GENERATEDFILES to 1 to enable this file system ?? what*/

#if !LWIP_HTTPD_SUPPORT_POST
#error This needs LWIP_HTTPD_SUPPORT_POST
#endif

#undef C
#define C(x) #x,
const char * const post_html_tags[] = { POST_NAMES };
#undef C

char wifi_ssid[LWIP_POST_BUFSIZE];
char wifi_password[LWIP_POST_BUFSIZE];
char json1[LWIP_POST_BUFSIZE];
int config_changed; //set to zero, watch for non-zero from POST handler

//fixme this strange racy connection stuff needs to be figured out for the ap
static void *current_connection;
static void *valid_connection;
static void *valid_json;
static void *valid_config;

//this function  checks for a legal form action name too
err_t
httpd_post_begin(void *connection, const char *uri, const char *http_request,
                 u16_t http_request_len, int content_len, char *response_uri,
                 u16_t response_uri_len, u8_t *post_auto_wnd)
{
  LWIP_UNUSED_ARG(connection);
  LWIP_UNUSED_ARG(http_request);
  LWIP_UNUSED_ARG(http_request_len);
  LWIP_UNUSED_ARG(content_len);
  LWIP_UNUSED_ARG(post_auto_wnd);
  for (int i = 0; i < POST_NAMES_TOP; i++) {
    if (!memcmp(uri + 1, post_html_tags[i], strlen(post_html_tags[i]))) {
        if (current_connection != connection) {
            current_connection = connection;
            valid_connection = NULL;
            printf("POST handler post name=%s\n", post_html_tags[i]);
            /* default page */
            snprintf(response_uri, response_uri_len, "/page2.shtml");
            /* e.g. for large uploads to slow flash over a fast connection, you should
               manually update the rx window. That way, a sender can only send a full
               tcp window at a time. If this is required, set 'post_aut_wnd' to 0.
               We do not need to throttle upload speed here, so: */
            *post_auto_wnd = 1;
            return ERR_OK;
        }
    }
  }
  return ERR_VAL;
}
//function to convert escaped hex chars in ascii string
void squash_hex_string(char* input, char* output)
{
    int loop = 0;
    int i = 0;
    char substr[3];
    while (input[loop] != '\0')
    {
        if (input[loop] == '%') {
            //got an escaped char
            loop++;
            strncpy(substr, &input[loop], 2);
            substr[3] = '\0';
            printf("substr = %s\n", substr); //debug
            int s = strtol(substr, NULL, 16);
            printf("s= %x\n", s); //debug
            loop += 2;
            sprintf(output, "%c", s);
            output++;
        } else {
            memcpy(output, &input[loop], 1); //copy unescaped chars
            loop++;
            output++;
        }
    }
    //insert NULL at the end of the output string
    output[i++] = '\0';
}
//move post string to a buffer, convert funny chars to ascii
static char *get_post_string(struct pbuf *p, char *destmem, uint16_t destlen, uint16_t srclen, uint16_t src_offset) {
     // char *w_json1 = (char *)pbuf_get_contiguous(p, tjson, sizeof(tjson), len, t);
    char *w_ptr = (char *)pbuf_get_contiguous(p, destmem, destlen, srclen, src_offset);
    if (w_ptr) {
        //we have now fetched the string
        memmove(destmem, w_ptr, destlen); //preserve the new json
        destmem[srclen] = '\0';   //terminate c string
        squash_hex_string(destmem, destmem); //squash in-place
        printf("New POST handler destmem=%s\n", destmem);
        w_ptr = destmem;
    }
    return w_ptr;
}
//return offset to field and length of value field
static u16_t check_field(struct pbuf *p, const char *mem, u16_t *len_val) {
    char *mptr = strstr(mem, "=");
    u16_t name_offset = 0xFFFF;
    u16_t len_str = 0xFFFF;
    if (mptr) {
        u16_t mem_len = mptr + 1 - mem; //offset to target string, skip "="
        u16_t value;
        name_offset = pbuf_memfind(p, mem, mem_len, 0); //find post str name
        if (name_offset != 0xFFFF) {
            //we found the post string name
            value = name_offset + mem_len;
            u16_t tmp = pbuf_memfind(p, "&", 1, value); //find delimiter
            if (tmp != 0xFFFF) {
                len_str = tmp - value;
            } else {
                len_str = p->tot_len - value; //last string in post
            }
            if ((len_str > 0) && (len_str < LWIP_POST_BUFSIZE)) {
                name_offset = value;
            }
        }
    }
    *len_val = len_str;
    return name_offset;
}
err_t
httpd_post_receive_data(void *connection, struct pbuf *p)
{
    err_t ret;

    LWIP_ASSERT("NULL pbuf", p != NULL);

    if (current_connection == connection) {
        u16_t len_ssid;
        u16_t len_pass;
        u16_t len_host;
        u16_t offset_ssid = check_field(p, "ssid=", &len_ssid);
        u16_t offset_pass = check_field(p, "pass=", &len_pass);
        u16_t offset_host = check_field(p, "host=", &len_host);
        if ((len_ssid > 0) && (len_ssid < LWIP_POST_BUFSIZE) &&
           (len_pass > 0) && (len_pass < LWIP_POST_BUFSIZE) &&
           (len_host > 0) && (len_host < LWIP_POST_BUFSIZE))
        {
            char *w_ssid = (char *)get_post_string(p, wifi_ssid, sizeof(wifi_ssid), len_ssid, offset_ssid);
            char *w_pass = (char *)get_post_string(p, wifi_password, sizeof(wifi_password), len_pass, offset_pass);
            char *w_host = (char *)get_post_string(p, local_host_name, sizeof(local_host_name), len_host, offset_host);
            if (w_ssid && w_pass && w_host) {
                memcpy(wifi_ssid, w_ssid, len_ssid); //preserve the new ssid
                wifi_ssid[len_ssid] = 0;
                memcpy(wifi_password, w_pass, len_pass);
                wifi_password[len_pass] = 0;
                printf("POST handler wifi_ssid=%s wifi_password=%s\n",wifi_ssid, wifi_password);
                flash_io_write_ssid(wifi_ssid, wifi_password);
                memcpy(local_host_name, w_host, len_host); //preserve the new ssid
                local_host_name[len_host] = 0;   //terminate c string
                printf("POST handler hostname=%s\n", local_host_name);
                flash_io_write_hostname(local_host_name, len_host);
                set_host_name(local_host_name);
                /* ssid and password and hostname are correct, create a "session" */
                valid_connection = connection;
            }

        } else {
            u16_t len;
            u16_t offset = check_field(p, "json1=", &len);
            if (offset != 0xffff) {
                char tjson[LWIP_POST_BUFSIZE];
                char *ts = get_post_string(p, tjson, sizeof(tjson), len, offset);
                if (ts != NULL) {
                    /* json is correct, set flag for post_finished*/
                    printf("json =%s\n", ts);
                    valid_json = connection;
                }
            } else {
                u16_t len;
                u16_t offset = check_field(p, "config=", &len);
                if (offset != 0xffff) {
                    char *ts = get_post_string(p, local_host_name, sizeof(local_host_name), len, offset);
                    if (ts != NULL) {
                        /* name is correct, set flag for post_finished*/
                        printf("POST handler hostname=%s\n", local_host_name);
                        //record for future use in flash, include '\0'
                        flash_io_write_hostname(local_host_name, len + 1);
                        /* set flag for post_finished*/
                        valid_config = connection;
                        set_host_name(local_host_name);
                    }
                }
            }
        }
        /* not returning ERR_OK aborts the connection, so return ERR_OK unless the
           connection is unknown */
        ret = ERR_OK;
    } else {
        ret = ERR_VAL;
    }

    /* this function must ALWAYS free the pbuf it is passed or it will leak memory */
    pbuf_free(p);

    return ret;
}

void
httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len)
{
  /* default page  for failure*/
  snprintf(response_uri, response_uri_len, "/page2.shtml");
  if (current_connection == connection) {
    if (valid_connection == connection) {
        /* login succeeded */
        config_changed = 1; //signal to app that the config has changed, use it
        snprintf(response_uri, response_uri_len, "/index.shtml");
    } else {
        if (current_connection == valid_json) {
            //we got json
            snprintf(response_uri, response_uri_len, "/index.shtml");
        } else {
            if (current_connection == valid_config) {
                //we got config setup
                snprintf(response_uri, response_uri_len, "/config.shtml");
            }
        }
    }
    current_connection = NULL;
    valid_connection = NULL;
  }
}
