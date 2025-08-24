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

#define ERR_USER -42

#define DEBUG_printf printf
//fixme BUF_SIZE needs to be per protocol!
#define TEST1_BUF_SIZE 2048

//user function to send data
typedef err_t (*tcp_send_fn)(void *arg, struct tcp_pcb *tpcb);
typedef err_t (*tcp_sending_fn)(void *arg, struct tcp_pcb *pcb);
// typedef void (*dns_found_client_callback)(void *state);
typedef void (*complete_callback)(void *state, int status);

struct TCP_SERVER_T_; //forward reference
// data required after a client connects multiple simultaneous clients require
// multiple of these structs limit to some max number to control memory use
struct server_per_client {
    struct TCP_SERVER_T_ *parent; //pointer back to server data
    struct tcp_pcb *client_pcb; //unused slot if NULL here
    uint8_t buffer_sent[TEST1_BUF_SIZE]; //these are very protocol specific
    uint8_t buffer_recv[TEST1_BUF_SIZE];

    err_t status; //last error return
    int recv_size;  //size expected from user info
    int recv_flag;  //flag when header data has been processed
    int send_size;  //amount to send
    int count;  //available to user code
    int recv_len; //amount accumulated so far
    int sent_len;   //amount sent so far
    // bool busy;   //true until completed is called, when false can be reused
};
// mulitple protocols may need different max connections. must be set at init time.
#define MAX_TCPTEST1_CONNECTIONS 2
// Created on an accept, so multiple accepts will work.
typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    //these are for all connections on this protocol
    complete_callback completed_callback;   // function to be called when entire user operation is complete */
    tcp_sent_fn user_sent;  /* Function to be called when more send buffer space is available. */
    tcp_recv_fn user_recv;  /* Function to be called when (in-sequence) data has arrived. */
    tcp_send_fn user_send; //function to send on socket
    tcp_poll_fn user_poll; // function when send is delayed
    tcp_send_fn user_accept;    //function called when new client is accepted
    void *priv; //for user tcp data and ptrs
    int max_connections;    //number of allocations create by init (below)
    struct server_per_client *per_accept; //pointer to per_client array
} TCP_SERVER_T;

typedef struct TCP_CLIENT_T_ {
    struct tcp_pcb *tcp_pcb;
    ip_addr_t remote_addr;
    uint16_t port;      //client only
    //must be cleared on each open
    err_t status; //last error return
    int count;  //available to user code
    int recv_len; //amount accumulated so far
    int sent_len;   //amount sent so far
    bool busy;      //false until completed is called

    //these are for all connections on this protocol
    complete_callback completed_callback;   // function to be called when entire user operation is complete */
    tcp_sent_fn user_sent;  /* Function to be called when more send buffer space is available. */
    tcp_recv_fn user_recv;  /* Function to be called when (in-sequence) data has arrived. */
//fixme are these redundant?
    tcp_sending_fn user_sending; /* function to be called when more data is to be sent */
    tcp_send_fn user_send; //function to send on socket
    tcp_poll_fn user_poll; // function when send is delayed
    tcp_connected_fn client_connected_callback; //function to be called when connection is ready
    uint8_t *buffer;    //users buffer - he knows the length
    void *priv; //for user tcp data and ptrs
} TCP_CLIENT_T;

#define PER_SERVER_ALIGNED_SIZE ((sizeof(int) - sizeof(struct server_per_client) & 0x3) + sizeof(struct server_per_client))
TCP_SERVER_T* tcp_server_init(void *priv, int max_connections);
TCP_CLIENT_T* tcp_client_init(void *priv);
err_t tcp_server_open(TCP_SERVER_T *state, uint16_t port, tcp_recv_fn recv,
                        tcp_sent_fn sent,
                        tcp_send_fn user_send,
                        tcp_send_fn user_accept,
                        complete_callback complete);

err_t tcp_server_result(struct server_per_client *per_client, int status);

bool tcp_client_open(void *arg, const char *hostname, uint16_t port,
                        uint8_t *buffer,
                        tcp_recv_fn recv,
                        tcp_sent_fn sent,
                        tcp_sending_fn sending,
                        tcp_poll_fn poll,
                        tcp_connected_fn connected,
                        complete_callback completed_callback);
//to be used by anyone who lets the server initiate communications
err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err);

err_t tcp_client_result(void *arg, err_t status);

//per server defs
err_t tcp_server_sendtest_init_open(uint16_t port,
                               complete_callback completed_callback);
err_t tcp_client_sendtest_init_open(char *hostname, uint16_t port,
                                    void *spriv);

#endif
