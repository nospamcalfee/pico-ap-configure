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
    // if (!memcmp(uri, "/configure", 10)) {
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

err_t
httpd_post_receive_data(void *connection, struct pbuf *p)
{
    err_t ret;

    LWIP_ASSERT("NULL pbuf", p != NULL);

    if (current_connection == connection) {
        u16_t token_user = pbuf_memfind(p, "ssid=", 5, 0);
        u16_t token_pass = pbuf_memfind(p, "pass=", 5, 0);
        if ((token_user != 0xFFFF) && (token_pass != 0xFFFF)) {
            u16_t value_user = token_user + 5;
            u16_t value_pass = token_pass + 5;
            u16_t len_user = 0;
            u16_t len_pass = 0;
            u16_t tmp;
            /* find ssid len */
            tmp = pbuf_memfind(p, "&", 1, value_user);
            if (tmp != 0xFFFF) {
            len_user = tmp - value_user;
            } else {
            len_user = p->tot_len - value_user;
            }
            /* find pass len */
            tmp = pbuf_memfind(p, "&", 1, value_pass);
            if (tmp != 0xFFFF) {
            len_pass = tmp - value_pass;
            } else {
            len_pass = p->tot_len - value_pass;
            }
            if ((len_user > 0) && (len_user < LWIP_POST_BUFSIZE) &&
               (len_pass > 0) && (len_pass < LWIP_POST_BUFSIZE)) {
                /* provide contiguous storage if p is a chained pbuf */
                // char buf_user[LWIP_POST_BUFSIZE];
                // char buf_pass[LWIP_POST_BUFSIZE];
                char *w_ssid = (char *)pbuf_get_contiguous(p, wifi_ssid, sizeof(wifi_ssid), len_user, value_user);
                char *w_pass = (char *)pbuf_get_contiguous(p, wifi_password, sizeof(wifi_password), len_pass, value_pass);
                if (w_ssid && w_pass) {
                    memcpy(wifi_ssid, w_ssid, len_user); //preserve the new ssid
                    wifi_ssid[len_user] = 0;
                    memcpy(wifi_password, w_pass, len_pass);
                    wifi_password[len_pass] = 0;
                    printf("POST handler wifi_ssid=%s wifi_password=%s\n",wifi_ssid, wifi_password);
                    /* ssid and password are correct, create a "session" */
                    valid_connection = connection;
                }
            }
        } else {
            u16_t token_json1 = pbuf_memfind(p, "json1=", 6, 0);
            if (token_json1 != 0xFFFF) {
                u16_t value_json1 = token_json1 + 6;
                u16_t len_json1 = 0;
                u16_t tmp;
                tmp = pbuf_memfind(p, "&", 1, value_json1);
                if (tmp != 0xFFFF) {
                    len_json1 = tmp - value_json1;
                } else {
                    len_json1 = p->tot_len - value_json1;
                }
                if ((len_json1 > 0) && (len_json1 < LWIP_POST_BUFSIZE)) {
                    char *w_json1 = (char *)pbuf_get_contiguous(p, json1, sizeof(json1), len_json1, value_json1);
                    if (w_json1) {
                        memcpy(json1, w_json1, len_json1); //preserve the new ssid
                        json1[len_json1] = 0;   //terminate c string
                        printf("POST handler json=%s\n", json1);
                        /* json is correct, set flag for post_finished*/
                        valid_json = connection;
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
        }
    }
    current_connection = NULL;
    valid_connection = NULL;
  }
}
