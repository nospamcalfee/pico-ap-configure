#ifndef _JSON_HANDLER_H
#define _JSON_HANDLER_H
#include "cJSON.h"

#define LATEST_VERSION "1.0"
#define LATEST_JSON_VERSION "1.0"

extern cJSON *mirror; // global containing system json

// return the current mirror or create if it does not exist
cJSON *get_mirror();

void inc_counter(cJSON *ptr);

void print_mirror(cJSON *ptr);

int tcp_server_json_check_freshness(int ver_counter);

// when we have a new buffer, if it is "fresher" update the local json
void tcp_client_json_handle_reply(int size, uint8_t *buffer);

#endif