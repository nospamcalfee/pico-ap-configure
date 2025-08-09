#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "cJSON.h"
#include <ssi.h>

#define LATEST_VERSION "1.0"
#define LATEST_JSON_VERSION "1.0"

cJSON *mirror; // global containing system json

// return the current mirror or create if it does not exist
cJSON *get_mirror()
{
    if (!mirror) {
        /* give a default mirror if fail to read a good one */
        printf("Creating json file\n");
        char name[64];
        mirror = cJSON_CreateObject();
        cJSON_AddItemToObject(mirror, "json_version",
                              cJSON_CreateString(LATEST_JSON_VERSION));
        cJSON_AddItemToObject(mirror, "update_count", cJSON_CreateString("0"));
        cJSON_AddItemToObject(mirror, "server_version",
                              cJSON_CreateString(LATEST_VERSION));
        // if (gethostname(name, sizeof(name))) {
        //     /* failed? set default */
        //     strcpy(name, "localhost");
        // }
        strcpy(name, local_host_name); //get my name
        /* add .local and port here */
        strncat(name, ".local", sizeof(name) - strlen(name) - strlen(".local"));
        cJSON_AddItemToObject(mirror, "server_ip", cJSON_CreateString(name));
        cJSON_AddItemToObject(mirror, "buddy_ip", cJSON_CreateString(name));
        cJSON_AddItemToObject(mirror, "well_delay", cJSON_CreateString("1"));
        cJSON_AddItemToObject(mirror, "skip_days", cJSON_CreateString("0"));
        cJSON_AddItemToObject(mirror, "schedule", cJSON_CreateObject());
    }
    return mirror;
}
