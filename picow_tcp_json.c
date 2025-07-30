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

#include "picow_tcp.h"
#include "picow_tcp_json.h"

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

/*
 * These functions are protocol specific clients, called from generic client routines
 *
 * The following should be moved into a new file for server tests.
 */

//sample original userspace functions.
static err_t tcp_server_send_data(void *arg, struct tcp_pcb *tpcb)
{
    struct server_per_client *per_client = (struct server_per_client *)arg;
    //fixme this could be moved to a separate function.
    //fixme as now, it rebuilds the buffer on send retries
    for(int i=0; i< BUF_SIZE; i++) {
        per_client->buffer_sent[i] = rand();
    }

    per_client->sent_len = 0;
    DEBUG_printf("tcp_server_send_data writing %ld\n", BUF_SIZE);
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    err_t err = tcp_write(tpcb, per_client->buffer_sent, BUF_SIZE, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        DEBUG_printf("tcp_server_send_data Failed to write data %d\n", err);
        per_client->status = err;
        if (err == ERR_MEM) {
            return ERR_OK; //wait for memory, will be called again by poll
        }
        //real errors exit here
        return tcp_server_result(per_client, err);
    }
    return ERR_OK;
}

//another app specific routine
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
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
            DEBUG_printf("tcp_server_recv %d/%d err %d\n", p->tot_len, per_client->recv_len, err);

            // Receive the buffer
            const uint16_t buffer_left = BUF_SIZE - per_client->recv_len;
            per_client->recv_len += pbuf_copy_partial(p, per_client->buffer_recv + per_client->recv_len,
                                                 p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);
            tcp_recved(tpcb, p->tot_len);
        }
        pbuf_free(p);

        // Have we have received the whole buffer
        if (per_client->recv_len == BUF_SIZE) {

            // // check it matches
            // if (memcmp(per_client->buffer_sent, per_client->buffer_recv, BUF_SIZE) != 0) {
            //     DEBUG_printf("buffer mismatch\n");
            //     return tcp_server_result(per_client, ERR_USER);
            // }
            DEBUG_printf("tcp_server_recv buffer ok\n");

            // Test completed?
            per_client->count++;
            tcp_server_result(per_client, 0);
            return ERR_OK;
        }
    } else {
        DEBUG_printf("tcp_server_recv some funny error condition, still free the pbuf\n");
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
static err_t tcp_json_sending(void *arg, struct tcp_pcb *pcb) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    // If we have received the whole buffer, send it back to the server
    if (state->recv_len == BUF_SIZE) {
        DEBUG_printf("tcp_json_sending %d bytes to server\n", state->recv_len);
        err_t err = tcp_write(pcb, state->buffer, state->recv_len, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            state->status = err;
            DEBUG_printf("tcp_json_sending Failed to write data %d\n", err);
            if (err == ERR_MEM) {
                return ERR_OK; //wait for memory, will be called again by poll
            }
            return tcp_client_result(state, err);
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

err_t tcp_json_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    DEBUG_printf("tcp_json_sent %u\n", len);
    state->sent_len += len;

    if (state->sent_len >= BUF_SIZE) {

        state->count++;
        // We should receive a new buffer from the server
        state->recv_len = 0;
        state->sent_len = 0;
        tcp_client_result(arg, 0);
    }

    return ERR_OK;
}

err_t tcp_json_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (!p) {
        return tcp_client_result(arg, ERR_USER);
    }
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    if (p->tot_len > 0) {
        DEBUG_printf("tcp_json_recv %d err %d\n", p->tot_len, err);
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            DUMP_BYTES(q->payload, q->len);
        }
        // Receive the buffer
        const uint16_t buffer_left = BUF_SIZE - state->recv_len;
        state->recv_len += pbuf_copy_partial(p, state->buffer + state->recv_len,
                                               p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);
        tcp_recved(tpcb, p->tot_len);
    }
    pbuf_free(p);

    return state->user_sending(state, tpcb);
}
#endif
/* every client protocol will have a custom open call.
   The mainloop only knows to start it up, every so often.
*/
static uint8_t buffer[BUF_SIZE];
static int fail_count;

bool tcp_json_client_init_open(const char *hostname, uint16_t port,
                            complete_callback completed_callback) {
    TCP_CLIENT_T *state = tcp_client_init(NULL);

    if (state->busy) {
        if (fail_count++ > 10) {
            //if something happened,
            printf("clear the busy flag to restart\n");
            state->busy = false;
        } else {
            return false; //could not open, last operation is in progress
        }
    }
    return tcp_client_open(state, hostname, port,
                            buffer,
                            tcp_json_recv,
                            tcp_json_sent,
                            tcp_json_sending,
                            completed_callback);
}

err_t tcp_service_json_init_open(uint16_t port,
                               complete_callback completed_callback) {
    TCP_SERVER_T *tcp_serv = tcp_server_init();

    err_t err = tcp_server_open(tcp_serv, port, tcp_server_recv,
                        tcp_server_sent,
                        tcp_server_send_data,
                        completed_callback);
    return err;
}
