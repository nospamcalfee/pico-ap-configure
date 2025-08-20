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
void tcp_client_json_dump_hdr(struct tcp_json_header *hdr) {
    DEBUG_printf("json hdr proto_ver = 0x%x, data_ver=0x%x size=0x%x\n",
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
    // struct tcp_json_priv *json_priv = per_client->priv;

    per_client->sent_len = 0;
    DEBUG_printf("tcp_server_json_send writing %ld\n", /*json_priv->size*/ BUF_SIZE);
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    err_t err = tcp_write(tpcb, per_client->buffer_sent, /*json_priv->size*/ BUF_SIZE, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        DEBUG_printf("tcp_server_json_send Failed to write data %d\n", err);
        per_client->status = err;
        if (err == ERR_MEM) {
            return ERR_OK; //wait for memory, will be called again by poll
        }
        //real errors exit here
        return tcp_server_result(per_client, err);
    }
    return ERR_OK;
}
static err_t tcp_server_json_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct server_per_client *per_client = (struct server_per_client*) arg;
    DEBUG_printf("tcp_server_json_sent %u\n", len);
    per_client->sent_len += len;

    if (per_client->sent_len >= BUF_SIZE) {

        // We should get the data back from the client
        per_client->recv_len = 0;
        DEBUG_printf("tcp_server_json_sent Waiting for buffer\n");
    }

    return ERR_OK;
}

//another app specific routine
/*
 * After client sends his json, the server will return his version info
 * followed (if the version is higher than the sending client) by the server
 * json. If the server is behind or the same as the client he will update his
 * json and return just the binary version info.
 */
static err_t tcp_server_json_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    struct server_per_client *per_client = (struct server_per_client *)arg;
    if ((err == ERR_OK || err == ERR_ABRT) && p == NULL) {
        // //remote client closed the connections, free up client stuff
        return tcp_server_result(per_client, err);
    }

    if ((err == ERR_OK || err == ERR_ABRT) && p != NULL) {
        //this callback means we have some data from the client

        // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
        // can use this method to cause an assertion in debug mode, if this method is called when
        // cyw43_arch_lwip_begin IS needed
        cyw43_arch_lwip_check();
        if (p->tot_len > 0) {
            DEBUG_printf("tcp_server_json_recv %d/%d err %d\n", p->tot_len, per_client->recv_len, err);

            // Receive the buffer
            const uint16_t buffer_left = MAX_JSON_BUF_SIZE - per_client->recv_len;
            int this_len = pbuf_copy_partial(p, per_client->buffer_recv + per_client->recv_len,
                                                 p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);
            per_client->recv_len += this_len;
            tcp_recved(tpcb, p->tot_len);
        }
        pbuf_free(p);

        // Have we have received the whole buffer
        if (per_client->recv_len == MAX_JSON_BUF_SIZE) {

            // // check it matches
            // if (memcmp(per_client->buffer_sent, per_client->buffer_recv, MAX_JSON_BUF_SIZE) != 0) {
            //     DEBUG_printf("buffer mismatch\n");
            //     return tcp_server_result(per_client, ERR_USER);
            // }
            DEBUG_printf("tcp_server_json_recv buffer ok\n");

            // Test completed?
            per_client->count++;
            tcp_server_result(per_client, 0);
            return ERR_OK;
        }
    } else {
        DEBUG_printf("tcp_server_json_recv some funny error condition, still free the pbuf\n");
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
        DEBUG_printf("tcp_client_json_sending %d bytes to server\n",
                     json_priv->size);
        tcp_client_json_dump_hdr(json_priv);
        err_t err = tcp_write(pcb, state->buffer, json_priv->size, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            state->status = err;
            DEBUG_printf("tcp_client_json_sending Failed to write data %d\n", err);
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
    DEBUG_printf("tcp_json_poll stat=%d\n", state->status);
    if (state->status == ERR_MEM) {
        state->user_sending(state, tpcb); //retry send
    }
    return ERR_OK;
}

err_t tcp_client_json_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    struct tcp_json_header *json_priv = state->priv;
    DEBUG_printf("tcp_client_json_sent %u\n", len);
    state->sent_len += len;

    if (state->sent_len >= json_priv->size) {

        state->count++;
        // We should receive a new buffer from the server
        state->recv_len = 0;
        state->sent_len = 0;
        DEBUG_printf("Waiting for json from server\n");
    }

    return ERR_OK;
}
/*
 * Here the amount to recieve is in the header from the server
 */
err_t tcp_client_json_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    struct tcp_json_header *json_priv = state->priv;
    if (!p) {
        return tcp_client_result(arg, ERR_USER);
    }
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    if (p->tot_len > 0) {
        DEBUG_printf("tcp_client_json_recv %d err %d\n", p->tot_len, err);
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
            DEBUG_printf("tcp_client_json_recv data len=%d expected=%d\n",
                        this_len, json_priv->size);
        }
        state->recv_len += this_len;
        tcp_recved(tpcb, p->tot_len);
    }
    pbuf_free(p);

    if (state->recv_len >= json_priv->size) {
        return tcp_client_result(arg, ERR_OK);
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
    DEBUG_printf("tcp_client_connected initiate sending to server\n");
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
    DEBUG_printf("json_client_complete\n");

    //fixme embedded guys hate free - it causes fragmentation. ALternative is
    //to reuse the allocated memory.
    free(state); //so multiple init/open cycles can work.
}
/* every client protocol will have a custom open call.
   The mainloop only knows to start it up, every so often.
*/

bool tcp_client_json_init_open(const char *hostname, uint16_t port, struct tcp_json_header *mypriv) {
    static uint8_t json_buffer[MAX_JSON_BUF_SIZE];
    static int fail_count;
    static TCP_CLIENT_T *json_state; //need permanent storage, but not reentrant!
    json_state = tcp_client_init(&mypriv);

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
    TCP_SERVER_T *tcp_serv = tcp_server_init(spriv);

    err_t err = tcp_server_open(tcp_serv, port,
                        tcp_server_json_recv,
                        tcp_server_json_sent,
                        tcp_server_json_send,
                        completed_callback);
    return err;
}
