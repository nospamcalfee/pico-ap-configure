/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"

#include "tcp_json.h"
#include "picow_tcp.h"
#include "json_handler.h"

//fixme - security and robustness requires using external data with verification!

#if 0
static void dump_bytes(const uint8_t *bptr, uint32_t len) {
    unsigned int i = 0;

    printf("dump_bytes %d", len);
    for (i = 0; i < len;) {
        if ((i & 0x0f) == 0) {
            printf("\n");
        } else if ((i & 0x07) == 0) {
            printf(" ");
        }
        printf("%02x ", bptr[i++]);
    }
    printf("\n");
}
#define DUMP_BYTES dump_bytes
#else
#define DUMP_BYTES(A,B)
#endif
static void json_dump_hdr(struct tcp_json_header *hdr) {
    DEBUG_printf("%s proto_ver = 0x%x, data_ver=0x%x size=%d\n", __func__,
        hdr->protocol_version, hdr->data_version, hdr->size);
}

/*
 * These functions are protocol specific clients, called from generic client routines
 *
 * The following should be moved into a new file for server tests.
 */

//sample original userspace functions.
static err_t tcp_server_json_send(void *arg, struct tcp_pcb *tpcb)
{
    struct server_per_client *per_client = (struct server_per_client *)arg;

    per_client->sent_len = 0;
    DEBUG_printf("%s writing %ld\n", __func__, per_client->send_size);
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    err_t err = tcp_write(tpcb, per_client->per_client_s_buffer, per_client->send_size, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        DEBUG_printf("%s Failed to write data %d\n", __func__, err);
        per_client->status = err;
        if (err == ERR_MEM) {
            return ERR_OK; //wait for memory, will be called again by poll
        }
        //real errors exit here
        return tcp_server_result(per_client, err);
    }
    return ERR_OK;
}
// local adjustments after the server accepts. If protocol demands, start a send.
static err_t tcp_server_json_accept(void *arg, struct tcp_pcb *tpcb)
{
    (void)tpcb; //ignore arg
    struct server_per_client *per_client = (struct server_per_client *)arg;
#if 0
    //example code where server sends on connect
    // app specific send on connnect
    return per_client->parent->user_send(per_client, per_client->client_pcb);
#else
    per_client->recv_size = MAX_JSON_BUF_SIZE; // set maximum recv
    per_client->recv_len = 0; //accumulated receive size

    per_client->send_size = 0;
    per_client->recv_flag = 0;  //clear header recieved flag
    DEBUG_printf("%s\n", __func__);
    return ERR_OK;
#endif
}
static err_t tcp_server_json_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct server_per_client *per_client = (struct server_per_client*) arg;
    DEBUG_printf("%s %u\n", __func__, len);
    per_client->sent_len += len;

    return ERR_OK;
}

/*
 * After client sends his json, the server will return his version info
 * followed (if the version is higher than the sending client) by the server
 * json. If the server is behind the client version he will update his json
 * and return just the binary version info.
 */
static err_t tcp_server_json_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    struct server_per_client *per_client = (struct server_per_client *)arg;
    DEBUG_printf("%s\n",__func__);
    if ((err == ERR_OK || err == ERR_ABRT) && p == NULL) {
        // //remote client closed the connections, free up client stuff
        return tcp_server_result(per_client, err);
    }

    if ((err == ERR_OK || err == ERR_ABRT) && p != NULL) {
        //this callback means we have some data from the client
        struct tcp_json_header *json_binary = (struct tcp_json_header *)per_client->per_client_r_buffer;

        // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
        // can use this method to cause an assertion in debug mode, if this method is called when
        // cyw43_arch_lwip_begin IS needed
        cyw43_arch_lwip_check();
        if (p->tot_len > 0) {
            DEBUG_printf("%s %d/%d err %d\n", __func__, p->tot_len, per_client->recv_len, err);

            // Receive the buffer
            const uint16_t buffer_left = MAX_JSON_BUF_SIZE - per_client->recv_len;
            //fixme buffer_recv and buffer_send should be external buffers with external sizes.
            int this_len = pbuf_copy_partial(p, per_client->per_client_r_buffer + per_client->recv_len,
                                                 p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);
            per_client->recv_len += this_len;
            if (per_client->recv_flag == 0 &&
                per_client->recv_len > sizeof(struct tcp_json_header)) {
                //only process this once, after have a full header
                per_client->recv_flag = 1; //flag I have the header
                json_dump_hdr(json_binary);
                DEBUG_printf("%s totlen=%d recv_len=%d recv_size=%d binary->size=%d\n", __func__,
                             p->tot_len, per_client->recv_len, per_client->recv_size, json_binary->size);
                //fixme verify this is correct json version, etc
                per_client->recv_size = json_binary->size;
            }
            tcp_recved(tpcb, p->tot_len);
        }
        pbuf_free(p);

        // Have we have received the whole buffer
        DEBUG_printf("%s 2 totlen=%d recv_len=%d recv_size=%d binary->size=%d\n", __func__,
                     p->tot_len, per_client->recv_len, per_client->recv_size, json_binary->size);
        if (per_client->recv_len >= per_client->recv_size) {
            // when we have the clients data if it is older return my entire json
            DEBUG_printf("%s buffer ok\n", __func__);

            per_client->send_size = tcp_server_json_check_freshness(json_binary);
            if (per_client->send_size < 0) {
                //some data error, abort connection
                return tcp_server_result(per_client, ERR_USER);
            }
            if (per_client->send_size > sizeof(*json_binary)) {
                //my local json is fresher, send it back to client
                struct tcp_json_header *json_sbinary = (struct tcp_json_header *)per_client->per_client_s_buffer;
                if (json_prep_get_counter_value(json_sbinary) > 0) {
                    // I now have a prepped ready to send buffer.
                    per_client->send_size = json_sbinary->size; //extract actual total send length
                    json_dump_hdr(json_sbinary);
                } else {
                    //something is wrong with the mirror json
                    return tcp_server_result(per_client, ERR_USER);
                }
            }
            per_client->parent->user_send(per_client, per_client->client_pcb);
            return ERR_OK;
        }
    } else {
        DEBUG_printf("%s some funny error condition, still free the pbuf\n", __func__);
        pbuf_free(p);
    }
    return ERR_OK;
}
/* every client protocol will have a custom open call.
   The mainloop only knows to start it up, every so often.

   For this test, malloc the server, but never free it. frees cause
   fragmentation and stale pointers. Embedded wants predictable.
*/
#if 1
/*
 * These functions are protocol specific clients, called from generic client routines
 */
/*
 * This little function tries to start a send. It may fail mainly due to
 * ERR_MEM memory pressure. In that case, it will be retried by the
 * tcp_client_poll function.
 */
static err_t tcp_client_json_sending(void *arg, struct tcp_pcb *pcb) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    struct tcp_json_header *json_priv = state->priv;
    //fixme first I send data, then receive it. below is wrong
    if (state->sent_len <= json_priv->size) {
        DEBUG_printf("%s %d bytes to server\n", __func__,
                     json_priv->size);
        json_dump_hdr(json_priv);
        err_t err = tcp_write(pcb, state->buffer, json_priv->size, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            state->status = err;
            DEBUG_printf("%s Failed to write data %d\n", __func__, err);
            if (err == ERR_MEM) {
                return ERR_OK; //wait for memory, will be called again by poll
            }
            return tcp_client_result(state, err); //some write failure, abort
        }
    }
    return ERR_OK;
}

static err_t tcp_json_poll(void *arg, struct tcp_pcb *tpcb) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    DEBUG_printf("%s stat=%d\n", __func__, state->status);
    if (state->status == ERR_MEM) {
        state->user_sending(state, tpcb); //retry send
    }
    return ERR_OK;
}

err_t tcp_client_json_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    struct tcp_json_header *json_priv = state->priv;
    DEBUG_printf("%s %u\n", __func__, len);
    state->sent_len += len;

    if (state->sent_len >= json_priv->size) {

        state->count++;
        // We should receive a new buffer from the server
        state->recv_len = 0;
        state->sent_len = 0;
        DEBUG_printf("%s Waiting for json from server\n", __func__);
        json_priv->size = MAX_JSON_BUF_SIZE;
    }

    return ERR_OK;
}
/*
 * Here the amount to receive is in the header from the server
 */
err_t tcp_client_json_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    struct tcp_json_header *json_priv = state->priv;
    printf("%s %p size %d\n", __func__, p, json_priv->size);
    if (!p) {
        return tcp_client_result(arg, ERR_USER);
    }
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    if (p->tot_len > 0) {
        DEBUG_printf("%s %d err %d\n", __func__, p->tot_len, err);
        // Receive the buffer
        const uint16_t buffer_left = json_priv->size - state->recv_len;
        int this_len = pbuf_copy_partial(p, state->buffer + state->recv_len,
                   p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);
        //on the first chunk, verify the  protocol, extract total size expected.
        //assumes entire header 10 bytes will be in the first receive
        if (state->recv_len == 0 && this_len >= sizeof(*json_priv)) {
            struct tcp_json_header *buf_priv = ( struct tcp_json_header *)state->buffer;
            //replace private expected size with server sent size
            json_priv->size = buf_priv->size;
            DEBUG_printf("%s data len=%d expected=%d p->tot_len=%d\n", __func__,
                        this_len, json_priv->size, p->tot_len);
        }
        state->recv_len += this_len;
        tcp_recved(tpcb, p->tot_len);
    }
    pbuf_free(p);

    if (state->recv_len >= json_priv->size) {
        //we have the entire send, update my json if necessary
        tcp_client_json_handle_reply(json_priv->size, state->buffer);
        return tcp_client_result(arg, ERR_OK); //we got it all, stop the client
    }

    return ERR_OK;
}
//if I want client to start protocol, modify the connected routine here.
static err_t json_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (err != ERR_OK) {
        printf("connect failed %d\n", err);
        return tcp_client_result(arg, err);
    }
    DEBUG_printf("%s initiate sending to server\n", __func__);
    state->recv_len = 0; //init state variables
    state->sent_len = 0;
    return state->user_sending(state, tpcb);
}

#endif
// If I use client priv pointer, I must have a complete callback to cleanup
// only the priv area can be safely accessed
// must clean up after init, freeing memory.
static void json_client_complete(void *arg, int status) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    (void) status; //ignore status
    // DEBUG_printf("json_client_complete\n");

    //fixme embedded guys hate free - it causes fragmentation. ALternative is
    //to reuse the allocated memory.
    free(state); //so multiple init/open cycles can work.
    //so far the priv data is not malloc'ed
}
/* every client protocol will have a custom open call.
   The mainloop only knows to start it up, every so often.
*/

uint8_t json_buffer[MAX_JSON_BUF_SIZE]; //fixme yet another buffer, isn't there one in state?
bool tcp_client_json_init_open(const char *hostname, uint16_t port, struct tcp_json_header *mypriv) {
    static int fail_count;
    static TCP_CLIENT_T *json_state; //need permanent storage, but not reentrant!
    printf("%s connnect to server %s\n", __func__, hostname);
    json_state = tcp_client_init(mypriv);

    if (json_state->busy) {
        if (fail_count++ > 10) {
            //if something happened,
            printf("clear the busy flag to restart\n");
            json_state->busy = false;
        } else {
            return false; //could not open, last operation is in progress
        }
    }
    fail_count = 0; //reset working counter
    return tcp_client_open(json_state, hostname, port,
                            json_buffer,
                            tcp_client_json_recv,
                            tcp_client_json_sent,
                            tcp_client_json_sending,
                            tcp_json_poll,
                            json_client_connected,
                            json_client_complete);
}

err_t tcp_server_json_init_open(uint16_t port,
                               complete_callback completed_callback,
                               struct tcp_json_header *spriv) {
    TCP_SERVER_T *tcp_serv = tcp_server_init(spriv, MAX_JSON_CONNECTIONS);

    err_t err = tcp_server_open(tcp_serv, port,
                        json_buffer,    //only used during xfer or receive
                        json_buffer,    //only used during xfer or receive
                        tcp_server_json_recv,
                        tcp_server_json_sent,
                        tcp_server_json_send,
                        tcp_server_json_accept,
                        completed_callback);
    return err;
}
