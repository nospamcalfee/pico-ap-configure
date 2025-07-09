/*
 * Copyright 2025, Steve Calfee. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * based on:
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 * there are some client/server test scripts to run on linux to test the picow code
 * pico-examples/pico_w/wifi/python_test_tcp
 */
#ifndef _PICO_TCP_BUFFER_H_
#define _PICO_TCP_BUFFER_H_

#include "lwip/tcp.h"

#define DEBUG_printf printf
#define BUF_SIZE 2048

//user function to send data
typedef err_t (*tcp_send_fn)(void *arg, struct tcp_pcb *tpcb);
// typedef void (*dns_found_client_callback)(void *state);
typedef void (*complete_callback)(void *state, int status);

struct user_header {
    uint16_t xfer_len;  //amount in the transfer
    uint16_t ver;       //data version
    uint8_t id;         //protocol definition
    uint8_t *buffer;    //users buffer - he knows the length
    int count;  //available to user code
    int buffer_len; //amount accumulated so far
    int sent_len;   //amount sent so far
    bool busy;      //false until completed is called
    complete_callback completed_callback;   // function to be called when entire user operation is complete */
    tcp_sent_fn user_sent;  /* Function to be called when more send buffer space is available. */
    tcp_recv_fn user_recv;  /* Function to be called when (in-sequence) data has arrived. */
    tcp_send_fn user_send; //function to send on socket (server only)
    void *priv; //for user tcp data and ptrs
};

typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    struct tcp_pcb *client_pcb;
    // bool complete;
    uint8_t buffer_sent[BUF_SIZE];
    uint8_t buffer_recv[BUF_SIZE];
    int sent_len;
    int recv_len;
    // int run_count;
    struct user_header user;
} TCP_SERVER_T;

typedef struct TCP_CLIENT_T_ {
    struct tcp_pcb *tcp_pcb;
    ip_addr_t remote_addr;
    uint16_t port;      //client only
    struct user_header user;
} TCP_CLIENT_T;


TCP_SERVER_T* tcp_server_init(void *priv);
TCP_CLIENT_T* tcp_client_init(void *priv);
bool tcp_server_open(TCP_SERVER_T *state, uint16_t port, tcp_recv_fn recv,
                        tcp_sent_fn sent, tcp_send_fn user_send);
//handle status, generally negative
err_t tcp_server_result(TCP_SERVER_T *state, int status);


err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
err_t tcp_server_send_data(void *arg, struct tcp_pcb *tpcb);
err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

bool tcp_client_open(void *arg, const char *hostname, uint16_t port,
                        uint8_t *buffer,
                        tcp_recv_fn recv, tcp_sent_fn sent,
                        complete_callback completed_callback);
// void client_request_common(void *arg);
err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
err_t tcp_client_result(void *arg, int status);
#endif