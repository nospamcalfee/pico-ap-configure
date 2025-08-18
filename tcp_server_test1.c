/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "picow_tcp.h"

#define TEST_ITERATIONS 10
#define POLL_TIME_S 5

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

            // check it matches
            if (memcmp(per_client->buffer_sent, per_client->buffer_recv, BUF_SIZE) != 0) {
                DEBUG_printf("buffer mismatch\n");
                return tcp_server_result(per_client, ERR_USER);
            }
            DEBUG_printf("tcp_server_recv buffer ok\n");

            // Test completed?
            per_client->count++;
            if (per_client->count >= TEST_ITERATIONS) {
                tcp_server_result(per_client, 0);
                return ERR_OK;
            }

            // Send another buffer
            return per_client->parent->user_send(per_client, per_client->client_pcb);
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

err_t tcp_server_sendtest_init_open(uint16_t port,
                               complete_callback completed_callback) {
    TCP_SERVER_T *tcp_serv = tcp_server_init(NULL);

    err_t err = tcp_server_open(tcp_serv, port, tcp_server_recv,
                        tcp_server_sent, tcp_server_send_data,
                        completed_callback);
    return err;
}
