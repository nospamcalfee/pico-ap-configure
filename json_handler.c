#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "cJSON.h"
#include <ssi.h>
#include "json_handler.h"
#include "tcp_json.h"

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
        cJSON_AddNumberToObject(mirror, "update_count", 0);
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
    }
    return mirror;
}
void inc_counter(cJSON *ptr) {
    cJSON *counter = cJSON_GetObjectItem(ptr, "update_count");
    if (counter != NULL && cJSON_IsNumber(counter)) {
        int count = counter->valueint + 1;
        cJSON_SetNumberValue(counter, count); //update in the json
    }
}

void print_mirror(cJSON *ptr) {
    char *json_string = cJSON_Print(ptr);
    if (json_string == NULL) {
        printf("Error printing cJSON object.\n");
    } else {

    printf("Generated JSON: %s\n", json_string);

    free(json_string);
    }
}

static int json_get_counter_value()
{
    cJSON *mptr = get_mirror();
    if (mptr == NULL) {
        return false; //mirror problem, about update
    }
    struct tcp_json_header *hptr = (struct tcp_json_header *)json_buffer;
    uint8_t *jptr = json_buffer + sizeof(*hptr);

    // json string follows an allocation for the binary header
    bool jready = cJSON_PrintPreallocated(mptr,
            jptr,
            sizeof(json_buffer) - 5 - sizeof(*hptr), /* max size */
            0);
    if (jready) {
        int jlength = strlen(jptr);
        int count;
        //now update binary header
        hptr->protocol_version = JSON_PROTOCOL_VERSION;
        hptr->size = jlength + sizeof(*hptr); //header size plus json
        cJSON *counter = cJSON_GetObjectItem(mptr, "update_count");
        if (counter != NULL && cJSON_IsNumber(counter)) {
            count = counter->valueint;
        } else {
            count = JSON_DATA_VERSION; //default value
        }

        hptr->data_version = count;
        printf("data version: %d\n", count);

        return count;   //return some non-zero version of my json
    }
    return jready; //let caller know if it started.
}
bool tcp_client_json_update_buddy(const char *hostname)
{
    bool jready = 0;
    if(json_get_counter_value() > 0) {
        struct tcp_json_header *hptr = (struct tcp_json_header *)json_buffer;
        jready = tcp_client_json_init_open(hostname, JSON_PORT, hptr);
    }
    return jready;
}
// when we have a new buffer, if it is "fresher" update the local json
void tcp_client_json_handle_reply(int size, uint8_t *buffer) {
    struct tcp_json_header *hptr = (struct tcp_json_header *)buffer;
    if (size <= sizeof(*hptr)) {
        return; //server agrees my data is freshest;
    }
    int my_counter = json_get_counter_value();

    if (my_counter == 0){
        return;  //this a cjson error exit
    }
    if (my_counter <= hptr->data_version) {
        // I am out of date, use servers version of json.
        printf("client data version: %d his version %d\n", my_counter, hptr->data_version);
        cJSON *localmirror = get_mirror();
        localmirror = cJSON_Parse((const char *)hptr + sizeof(*hptr)); //skip header
        if (localmirror == NULL) {
            printf("tcp_client_json_handle_reply could not parse servers json\n");
        }
        hptr->size = sizeof(*hptr);
    }

}
//return number of bytes to send to client. 0 if there is some total error and
//an abort should be used.

//If the local json has a higher version counter, send header and json
//otherwise just send the header.
int tcp_server_json_check_freshness(int ver_counter) {
    struct tcp_json_header *hptr = (struct tcp_json_header *)json_buffer;
    //json_get_counter_value inits the header to send everything, hdr and json
    int my_counter = json_get_counter_value();

    if (my_counter == 0){
        return my_counter;  //this a cjson error exit
    }
    if (my_counter <= ver_counter) {
        //I need to send to the client just the header, not json data
        printf("my data version: %d his version %d I accept client\n", my_counter, ver_counter);
        hptr->size = sizeof(*hptr);
    }
    return hptr->size;
}