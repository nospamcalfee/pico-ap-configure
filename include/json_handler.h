#ifndef _JSON_HANDLER_H
#define _JSON_HANDLER_H

#define LATEST_VERSION "1.0"
#define LATEST_JSON_VERSION "1.0"

extern cJSON *mirror; // global containing system json

// return the current mirror or create if it does not exist
cJSON *get_mirror();

void inc_counter(cJSON *ptr);

void print_mirror(cJSON *ptr);

#endif