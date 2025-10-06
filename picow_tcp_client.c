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

#define TEST_ITERATIONS 10
#define POLL_TIME_S 5
//maybe should go into a header
err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb);

#if 0
static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb); //defn
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

static err_t tcp_client_close(void *arg) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    err_t err = ERR_OK;
    // DEBUG_printf("tcp_client_close %p\n", state->tcp_pcb);
    if (state->tcp_pcb != NULL) {
        tcp_arg(state->tcp_pcb, NULL);
        tcp_poll(state->tcp_pcb, NULL, 0);
        tcp_sent(state->tcp_pcb, NULL);
        tcp_recv(state->tcp_pcb, NULL);
        tcp_err(state->tcp_pcb, NULL);
        err = tcp_close(state->tcp_pcb);
        if (err != ERR_OK) {
            DEBUG_printf("close failed %d, calling abort\n", err);
            tcp_abort(state->tcp_pcb);
            err = ERR_ABRT;
        }
        state->tcp_pcb = NULL;
    }
    return err;
}

// Called with results of operation
err_t tcp_client_result(void *arg, err_t status) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (status == 0) {
        DEBUG_printf("tcp_client_result success\n");
    } else {
        DEBUG_printf("tcp_client_result failed %d\n", status);
    }
    err_t cls_err = tcp_client_close(arg);
    state->busy = false;    //for pollers, set on open
    if (state->completed_callback != NULL) {
        state->completed_callback(arg, status);
    }
    return cls_err;
}

//to be used by anyone who lets the server initiate communications
err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (err != ERR_OK) {
        printf("connect failed %d\n", err);
        return tcp_client_result(arg, err);
    }
    DEBUG_printf("tcp_client_connected Waiting for buffer from server\n");
    return ERR_OK;
}

// @note The corresponding pcb is already freed when this callback is called!
// So no-one including close can access the pcb.
static void tcp_client_err(void *arg, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (err != ERR_ABRT) {
        DEBUG_printf("tcp_client_err %d\n", err);
        state->tcp_pcb = NULL; //don't do close!
        tcp_client_result(arg, err);
    }
}

//this function is called only when dns is OK, either immediately or after resolving via dns
static void client_request_common(void *arg) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    DEBUG_printf("Connecting to %s port %u\n", ip4addr_ntoa(&state->remote_addr), state->port);
    state->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&state->remote_addr));
    if (!state->tcp_pcb) {
        DEBUG_printf("failed to create pcb\n");
        return; //fixme do I need to do tcp_client_result
    }
    tcp_arg(state->tcp_pcb, state);
    tcp_poll(state->tcp_pcb, state->user_poll, POLL_TIME_S * 2);
    tcp_sent(state->tcp_pcb, state->user_sent);
    tcp_recv(state->tcp_pcb, state->user_recv);
    tcp_err(state->tcp_pcb, tcp_client_err);

    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(state->tcp_pcb, &state->remote_addr, state->port, state->client_connected_callback);
    cyw43_arch_lwip_end();
    if (err) {
        printf("connect stat = %d\n", err);
    }
    return;
}

// Call back with a DNS result
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (ipaddr != NULL) {
        state->remote_addr = *ipaddr;
        // printf("dns address %s\n", ipaddr_ntoa(ipaddr));
        client_request_common(state);
    } else {
        printf("dns request failed\n");
        tcp_client_result(state, ERR_USER);
    }
}
// can return true which means connected or inprogress, or FALSE== some error

bool tcp_client_open(void *arg, const char *hostname, uint16_t port,
                        uint8_t *buffer,
                        tcp_recv_fn recv,
                        tcp_sent_fn sent,
                        tcp_sending_fn sending,
                        tcp_poll_fn poll,
                        tcp_connected_fn connected,
                        complete_callback completed_callback) {
    err_t err;
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    state->port = port;
    state->user_recv = recv;
    state->user_sent = sent;
    state->user_sending = sending;
    state->user_poll = poll;
    state->completed_callback = completed_callback;
    state->client_connected_callback = connected;
    state->buffer = buffer;
    state->status = 0; //last error return
    state->count = 0;  //available to user code
    state->recv_len = 0; //amount accumulated so far
    state->sent_len = 0;   //amount sent so far
    state->busy = true;

    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();
    err = dns_gethostbyname(hostname, &state->remote_addr, dns_found, state);
    cyw43_arch_lwip_end();

    // state->dns_request_sent = true;
    if (err == ERR_OK) {
        //here implies remote_addr is set by dns
        client_request_common(state); // we connected, finish connect
    } else if (err != ERR_INPROGRESS) { // ERR_INPROGRESS means expect a callback
        printf("dns request failed\n");
        tcp_client_result(state, err);
    }
    return true;
}
// Perform initialisation
TCP_CLIENT_T* tcp_client_init(void *priv) {
    TCP_CLIENT_T *state = calloc(1, sizeof(TCP_CLIENT_T));
    if (!state) {
        DEBUG_printf("failed to allocate state\n");
        return NULL;
    }
    state->priv = priv; //keep ptr to users data
    return state;
}
