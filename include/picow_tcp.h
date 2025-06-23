/*
 * Copyright 2025, Steve Calfee. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * based on:
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 */
#ifndef _PICO_TCP_BUFFER_H_
#define _PICO_TCP_BUFFER_H_

#include "lwip/tcp.h"

#define TCP_PORT 4242
#define DEBUG_printf printf
#define BUF_SIZE 2048

//user function to send data
typedef err_t (*tcp_send_fn)(void *arg, struct tcp_pcb *tpcb);

struct user_header {
    uint16_t xfer_len;  //amount in the transfer
    uint16_t ver;       //data version
    uint8_t id;         //protocol definition
    int complete;       //0, not finished
    /* Function to be called when more send buffer space is available. */
    tcp_sent_fn user_sent;
    /* Function to be called when (in-sequence) data has arrived. */
    tcp_recv_fn user_recv;
    tcp_send_fn user_send; //function to send on socket
    void *priv; //for user tcp data and ptrs
};

typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    struct tcp_pcb *client_pcb;
    bool complete;
    uint8_t buffer_sent[BUF_SIZE];
    uint8_t buffer_recv[BUF_SIZE];
    int sent_len;
    int recv_len;
    int run_count;
    struct user_header user;
} TCP_SERVER_T;

typedef struct TCP_CLIENT_T_ {
    struct tcp_pcb *tcp_pcb;
    ip_addr_t remote_addr;
    uint8_t buffer[BUF_SIZE];
    int buffer_len;
    int sent_len;
    bool complete;
    int run_count;
    bool connected;
} TCP_CLIENT_T;


TCP_SERVER_T* tcp_server_init(void *priv);
TCP_CLIENT_T* tcp_client_init(ip_addr_t remote_addr);
bool tcp_server_open(TCP_SERVER_T *state, uint16_t port, tcp_recv_fn recv,
                        tcp_sent_fn sent, tcp_send_fn user_send);
//handle status, generally negative
err_t tcp_server_result(TCP_SERVER_T *state, int status);


err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
err_t tcp_server_send_data(void *arg, struct tcp_pcb *tpcb);
err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
#endif