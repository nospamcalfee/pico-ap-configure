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

TCP_SERVER_T* tcp_server_init(void *priv) {
    TCP_SERVER_T *state = calloc(1, sizeof(TCP_SERVER_T));
    if (!state) {
        DEBUG_printf("failed to allocate state\n");
        return NULL;
    }
    state->user.priv = priv; //set user pointer
    return state;
}
// I want the server to stay alive and accepting.
// so errors or successes only close the client.
static err_t tcp_close_client(void *arg) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    err_t err = ERR_OK;
    if (state->client_pcb != NULL) {
        tcp_arg(state->client_pcb, NULL);
        tcp_poll(state->client_pcb, NULL, 0);
        tcp_sent(state->client_pcb, NULL);
        tcp_recv(state->client_pcb, NULL);
        tcp_err(state->client_pcb, NULL);
        err = tcp_close(state->client_pcb);
        if (err != ERR_OK) {
            DEBUG_printf("close failed %d, calling abort\n", err);
            tcp_abort(state->client_pcb);
            err = ERR_ABRT;
        }
        state->client_pcb = NULL;
    }
    // if (state->server_pcb) {
    //     tcp_arg(state->server_pcb, NULL);
    //     tcp_close(state->server_pcb);
    //     state->server_pcb = NULL;
    // }
    return err;
}

//handle status, generally negative
err_t tcp_server_result(TCP_SERVER_T *state, int status) {
    if (status == 0) {
        DEBUG_printf("tcp_server_result test success\n");
    } else {
        DEBUG_printf("tcp_server_result test failed %d\n", status);
        // state->complete = true; fixme not needed?
    }
    if (state->user.completed_callback != NULL) {
        state->user.completed_callback(state, status);
    }

    return tcp_close_client(state);
}

static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    DEBUG_printf("tcp_server_poll_fn\n");
        if (state->user.status == ERR_MEM) {
            //on memory low, retry sends
            state->user.user_send(arg, state->client_pcb);
    }
    return ERR_OK; //not used here
}

static void tcp_server_err(void *arg, err_t err) {
    if (err != ERR_ABRT) {
        DEBUG_printf("tcp_client_err_fn %d\n", err);
        tcp_server_result(arg, err);
    }
}
/*
 * These functions are protocol specific clients, called from generic client routines
 * fixme - on each accept create a new state block, so multiple connections can work
 */
static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    if (err != ERR_OK || client_pcb == NULL) {
        DEBUG_printf("Failure in accept\n");
        tcp_server_result(arg, err);
        return ERR_VAL;
    }
    DEBUG_printf("tcp_server_accept new Cl connected\n");

    state->client_pcb = client_pcb;
    tcp_arg(client_pcb, state);
    tcp_sent(client_pcb, state->user.user_sent); //tcp_server_sent);
    tcp_recv(client_pcb, state->user.user_recv); //tcp_server_recv);
    tcp_poll(client_pcb, tcp_server_poll, POLL_TIME_S * 2);
    tcp_err(client_pcb, tcp_server_err);

    state->user.count = 0; //new user test count

    // app specific send on connnect
    return state->user.user_send(arg, state->client_pcb);
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
    memset(&state->user, 0, sizeof(state->user)); //clear the user data
    state->user.user_recv = recv;
    state->user.user_send = user_send;
    state->user.user_sent = sent;
    state->user.completed_callback = complete;

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
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    DEBUG_printf("tcp_server_sent %u\n", len);
    state->user.sent_len += len;

    if (state->user.sent_len >= BUF_SIZE) {

        // We should get the data back from the client
        state->user.recv_len = 0;
        DEBUG_printf("tcp_server_sent Waiting for buffer\n");
    }

    return ERR_OK;
}


//sample original userspace functions.
err_t tcp_server_send_data(void *arg, struct tcp_pcb *tpcb)
{

    //fixme this could be moved to a separate function.
    //fixme as now, it rebuilds the buffer on send retries
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    for(int i=0; i< BUF_SIZE; i++) {
        state->buffer_sent[i] = rand();
    }

    state->user.sent_len = 0;
    DEBUG_printf("tcp_server_send_data writing %ld\n", BUF_SIZE);
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    err_t err = tcp_write(tpcb, state->buffer_sent, BUF_SIZE, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        DEBUG_printf("tcp_server_send_data Failed to write data %d\n", err);
            state->user.status = err;
            if (err == ERR_MEM) {
                return ERR_OK; //wait for memory, will be called again by poll
            }
        //real errors exit here
        return tcp_server_result(arg, err);
    }
    return ERR_OK;
}

//another app specific routine
err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    if ((err == ERR_OK || err == ERR_ABRT) && p == NULL) {
        // //remote client closed the connections, free up client stuff
        return tcp_server_result(arg, err);
    }

    if ((err == ERR_OK || err == ERR_ABRT) && p != NULL) {
        //this callback means we have some data from the client

        // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
        // can use this method to cause an assertion in debug mode, if this method is called when
        // cyw43_arch_lwip_begin IS needed
        cyw43_arch_lwip_check();
        if (p->tot_len > 0) {
            DEBUG_printf("tcp_server_recv %d/%d err %d\n", p->tot_len, state->user.recv_len, err);

            // Receive the buffer
            const uint16_t buffer_left = BUF_SIZE - state->user.recv_len;
            state->user.recv_len += pbuf_copy_partial(p, state->buffer_recv + state->user.recv_len,
                                                 p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);
            tcp_recved(tpcb, p->tot_len);
        }
        pbuf_free(p);

        // Have we have received the whole buffer
        if (state->user.recv_len == BUF_SIZE) {

            // check it matches
            if (memcmp(state->buffer_sent, state->buffer_recv, BUF_SIZE) != 0) {
                DEBUG_printf("buffer mismatch\n");
                return tcp_server_result(arg, ERR_USER);
            }
            DEBUG_printf("tcp_server_recv buffer ok\n");

            // Test completed?
            state->user.count++;
            if (state->user.count >= TEST_ITERATIONS) {
                tcp_server_result(arg, 0);
                return ERR_OK;
            }

            // Send another buffer
            return state->user.user_send(arg, state->client_pcb);
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
    TCP_SERVER_T *tcp_serv = tcp_server_init(NULL);

    err_t err = tcp_server_open(tcp_serv, port, tcp_server_recv,
                        tcp_server_sent, tcp_server_send_data,
                        completed_callback);
    return err;
}
