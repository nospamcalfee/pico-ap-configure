/*
 * Copyright 2025, Steve Calfee. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PICO_TCP_JSON_H_
#define _PICO_TCP_JSON_H_

#include "lwip/tcp.h"
#include "picow_tcp.h"

bool tcp_json_client_init_open(const char *hostname, uint16_t port,
                        complete_callback completed_callback);
#endif
