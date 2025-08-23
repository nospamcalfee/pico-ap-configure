/*
 * Copyright 2025, Steve Calfee. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PICO_TCP_JSON_H_
#define _PICO_TCP_JSON_H_

#include "lwip/tcp.h"
//for json server limit connections.
#define MAX_JSON_CONNECTIONS 1
#include "picow_tcp.h"

/*
 * This is the C header for both client and server code for tossing around json.
 *
 * Protocol: A binary header is prepended on json for protocol control.
 *
 * A client will connect with a buddy whenever the client's website gets
 * updated. The client will send his entire, fresh json.
 *
 * The server will compare the received json version info and if the server
 * has fresher data accept that data.
 *
 * The server responds with either a small header giving the client back his
 * version info. Or if the server has more recent data the server will send
 * the entire, newer json collection.
 *
 * THe client will check the server sent version info and if newer will use
 * the server's data.
 *
 * At this point both Client and server will have identical json data with
 * identical versions. If the server did not have fresher info the client
 * will iterate through the entire collection of Buddies in the json to get
 * everyone up to date. The client will not send json data to a buddy twice.
 * If a server fails to respond the Client will retry several times after a
 * long delay.
 */

/*
 * the json protocol header specifies how many bytes are to be sent/received
 * so the application knows when a complete json data packet has been
 * received.

 * The protocol version also specifies the header struct layout for future
 * needs.

 * data_version is used to determine if a complete json transfer is required.
 * Higher values are newer than lower.
 */
#define JSON_PROTOCOL_VERSION 1
#define JSON_DATA_VERSION 1
#define MAX_JSON_BUF_SIZE 2048
#define JSON_PORT 4243

struct tcp_json_header {
    uint16_t protocol_version;
    uint32_t size;
    uint32_t data_version;
};

//user buffer
extern uint8_t json_buffer[MAX_JSON_BUF_SIZE];

void tcp_client_json_dump_hdr(struct tcp_json_header *hdr);

bool tcp_client_json_init_open(const char *hostname, uint16_t port,
                                struct tcp_json_header *mypriv);

err_t tcp_server_json_init_open(uint16_t port,
                               complete_callback completed_callback,
                               struct tcp_json_header *spriv);
#endif
