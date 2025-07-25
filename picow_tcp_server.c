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

TCP_SERVER_T* tcp_server_init(void) {
    TCP_SERVER_T *state = calloc(1, sizeof(TCP_SERVER_T));
    if (!state) {
        DEBUG_printf("failed to allocate state\n");
        return NULL;
    }
    return state;
}
// I want the server to stay alive and accepting.
// so errors or successes only close the client.
static err_t tcp_close_client(struct server_per_client *per_client) {
    err_t err = ERR_OK;
    if (per_client->client_pcb != NULL) {
        tcp_arg(per_client->client_pcb, NULL);
        tcp_poll(per_client->client_pcb, NULL, 0);
        tcp_sent(per_client->client_pcb, NULL);
        tcp_recv(per_client->client_pcb, NULL);
        tcp_err(per_client->client_pcb, NULL);
        err = tcp_close(per_client->client_pcb);
        if (err != ERR_OK) {
            DEBUG_printf("close failed %d, calling abort\n", err);
            tcp_abort(per_client->client_pcb);
            err = ERR_ABRT;
        }
        per_client->client_pcb = NULL;
    }
    return err;
}

//handle status per connection
err_t tcp_server_result(struct server_per_client *per_client, int status) {
    if (status == 0) {
        DEBUG_printf("tcp_server_result test success\n");
    } else {
        DEBUG_printf("tcp_server_result test failed %d\n", status);
    }
    if (per_client->parent->completed_callback != NULL) {
        per_client->parent->completed_callback(per_client, status);
    }

    return tcp_close_client(per_client);
}

static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb) {
    struct server_per_client *per_client = (struct server_per_client *)arg;
    DEBUG_printf("tcp_server_poll\n");
        if (per_client->status == ERR_MEM) {
            //on memory low, retry sends
            per_client->parent->user_send(per_client, per_client->client_pcb);
    }
    return ERR_OK; //not used here
}

static void tcp_server_err(void *arg, err_t err) {
    if (err != ERR_ABRT) {
        DEBUG_printf("tcp_server_err %d\n", err);
        tcp_server_result(arg, err);
    }
}
/*
 * These functions are protocol specific clients, called from generic client routines
 * fixme - on each accept create a new state block, so multiple connections can work
 */
static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
    TCP_SERVER_T *server_state = (TCP_SERVER_T*)arg;
    struct server_per_client *per_client = NULL;
    if (err != ERR_OK || client_pcb == NULL) {
        DEBUG_printf("Failure in accept\n");
        tcp_server_result(arg, err);
        return ERR_VAL;
    }
    //find a free slot for this connection
    int i;
    for (i = 0; i < MAX_CONNECTIONS; i++) {
        if (server_state->per_accept[i].client_pcb == NULL) {
            per_client = &server_state->per_accept[i];
            break;
        }
    }
    if (per_client == NULL) {
        //all clients are busy, fail accept
        err_t err = tcp_close(client_pcb);
        if (err != ERR_OK) {
            DEBUG_printf("close failed %d, calling abort\n", err);
            // tcp_abort(state->client_pcb); //fixme what here?
        }
        return ERR_MEM; //fail the accept
    }
    DEBUG_printf("tcp_server_accept new Cl connected\n");

    per_client->parent = server_state;
    per_client->client_pcb = client_pcb;
    tcp_arg(client_pcb, per_client);
    tcp_sent(client_pcb, server_state->user_sent); //tcp_server_sent);
    tcp_recv(client_pcb, server_state->user_recv); //tcp_server_recv);
    tcp_poll(client_pcb, tcp_server_poll, POLL_TIME_S * 2);
    tcp_err(client_pcb, tcp_server_err);

    per_client->count = 0; //new user test count

    // app specific send on connnect
    return server_state->user_send(per_client, per_client->client_pcb);
}

err_t tcp_server_open(TCP_SERVER_T *state, uint16_t port, tcp_recv_fn recv,
                        tcp_sent_fn sent, tcp_send_fn user_send,
                        complete_callback complete) {
    DEBUG_printf("Starting server at %s on port %u\n", ip4addr_ntoa(netif_ip4_addr(netif_list)), port);

    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb) {
        DEBUG_printf("failed to create pcb\n");
        return false;
    }
    state->user_recv = recv;
    state->user_send = user_send;
    state->user_sent = sent;
    state->completed_callback = complete;

    err_t err = tcp_bind(pcb, NULL, port);
    if (err) {
        DEBUG_printf("failed to bind to port %u err=%d\n", port, err);
        return err;
    }

    state->server_pcb = tcp_listen_with_backlog(pcb, 1);
    if (!state->server_pcb) {
        DEBUG_printf("failed to listen\n");
        if (pcb) {
            tcp_close(pcb);
        }
        return ERR_MEM;
    }

    tcp_arg(state->server_pcb, state);
    tcp_accept(state->server_pcb, tcp_server_accept);
    return ERR_OK;
}

err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct server_per_client *per_client = (struct server_per_client*) arg;
    DEBUG_printf("tcp_server_sent %u\n", len);
    per_client->sent_len += len;

    if (per_client->sent_len >= BUF_SIZE) {

        // We should get the data back from the client
        per_client->recv_len = 0;
        DEBUG_printf("tcp_server_sent Waiting for buffer\n");
    }

    return ERR_OK;
}


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

err_t tcp_service_sendtest_init_open(uint16_t port,
                               complete_callback completed_callback) {
    TCP_SERVER_T *tcp_serv = tcp_server_init();

    err_t err = tcp_server_open(tcp_serv, port, tcp_server_recv,
                        tcp_server_sent, tcp_server_send_data,
                        completed_callback);
    return err;
}
