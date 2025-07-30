/*
 * Copyright 2025, Steve Calfee. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PICO_TCP_JSON_H_
#define _PICO_TCP_JSON_H_

#include "lwip/tcp.h"
#include "picow_tcp.h"

/*
 * This is the header for both client and server code for tossing around json.
 *
 * Protocol: (only plaintext json passed)
 *
 * A client will connect with a buddy whenever the client's website gets
 * updated.
 *
 * server sends On connection his entire json collection. The client will
 * examine its version info and if the server has fresher data accept that
 * data.
 *
 * Client responds with either a small header giving the server back his
 * version info. Or if the client has more recent data the Client will send
 * the entire, newer json collection.
 *
 * Server will again check the version info and if newer will use the Clients
 * data.
 *
 * At this point both Client and server will have identical json data with
 * identical versions. The client will iterate through the entire collection
 * of Buddies in the json to get everyone up to date. He will keep track of
 * the version known by each buddy so he will know when to stop sending. If a
 * server fails to respond the Client will retry after a long delay.
 */
bool tcp_json_client_init_open(const char *hostname, uint16_t port,
                        complete_callback completed_callback);
#endif
