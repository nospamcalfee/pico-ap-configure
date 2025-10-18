#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "cJSON.h"
#include <ssi.h>
#include "json_handler.h"
#include "tcp_json.h"
/*
 * The system needs 2 copies of the json.

 * One copy is an ascii version, always json is transfered as ascii. Ascii is
 * usually a transfer buffer received from a sender or built from the local
 * mirror on xmit.

 * The mirror contains a parsed version of the ascii json. If a parse from
 * some weird xfer fails, null the mirror pointer, so it will be inited with
 * a very small update count.
 */
cJSON *mirror; // global containing system json binary data

// return the current mirror or create if it does not exist
cJSON *get_mirror()
{
    if (!mirror) {
        /* give a default mirror if fail to read a good one */
        printf("Creating json file\n");
        char name[64];
        mirror = cJSON_CreateObject();
        cJSON *buddy_array = NULL;
        cJSON_AddItemToObject(mirror, "json_version",
                              cJSON_CreateString(LATEST_JSON_VERSION));
        cJSON_AddNumberToObject(mirror, "update_count", DEFAULT_UPDATE_COUNT);
        cJSON_AddItemToObject(mirror, "server_version",
                              cJSON_CreateString(LATEST_VERSION));
        buddy_array = cJSON_CreateArray();
        cJSON_AddItemToObject(mirror, "buddy_ip", buddy_array);

        strcpy(name, local_host_name); //get my name
        /* add .local and port here */
        strncat(name, ".local", sizeof(name) - strlen(name) - strlen(".local"));
        cJSON_AddItemToArray(buddy_array, cJSON_CreateString("jedediah.local"));
        cJSON_AddItemToObject(mirror, "server_ip", cJSON_CreateString(name));
        cJSON_AddItemToObject(mirror, "well_delay", cJSON_CreateString("1"));
        cJSON_AddItemToObject(mirror, "skip_days", cJSON_CreateString("0"));
    }
    return mirror;
}
void inc_mirror_counter(cJSON *ptr) {
    cJSON *counter = cJSON_GetObjectItem(ptr, "update_count");
    if (counter != NULL && cJSON_IsNumber(counter)) {
        int count = counter->valueint + 1;
        cJSON_SetNumberValue(counter, count); //update in the json
    }
}
//handy utility function
int get_mirror_update_count(){
    cJSON *mptr = get_mirror();
    cJSON *counter = cJSON_GetObjectItem(mptr, "update_count");
    if (counter != NULL && cJSON_IsNumber(counter)) {
        return counter->valueint;
    }
    return DEFAULT_UPDATE_COUNT; //something is messed up, return default
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
// return local counter value, -1 if json failure
// also preps the json buffer with a header
// json value ranges from 0 to maxint
/*
 * this function prepares a binary buffer for sending using the local mirror
 * binary. It inits both the binary header and the ascii json data.
 */
int json_prep_get_counter_value(struct tcp_json_header *binary)
{
    cJSON *mptr = get_mirror();
    if (mptr == NULL) {
        return -1; //mirror problem, abort update
    }
    struct tcp_json_header *hptr = binary;
    uint8_t *jptr = (uint8_t *)binary + sizeof(*hptr);

    // json string follows an allocation for the binary header
    bool jready = cJSON_PrintPreallocated(mptr,
            jptr,
            MAX_JSON_BUF_SIZE - 5 - sizeof(*hptr), /* max size */
            0);
    if (jready) {
        int jlength = strlen(jptr);
        int count;
        //now update header
        hptr->protocol_version = JSON_PROTOCOL_VERSION;
        hptr->size = jlength + sizeof(*hptr); //header size plus json
        count = get_mirror_update_count();
        hptr->data_version = count;
        printf("%s data version: %d\n", __func__, count);

        return count;   //return some non-zero version of my json
    }
    return -1; //let caller know json is borked
}
bool tcp_client_json_update_buddy(const char *hostname)
{
    struct tcp_json_header *hptr = (struct tcp_json_header *)json_buffer;
    bool jready = 0;
    if (json_prep_get_counter_value(hptr) > 0) {
        const cJSON *buddy = NULL;
        const cJSON *buddys = NULL;
        buddys = cJSON_GetObjectItemCaseSensitive(mirror, "buddy_ip");
        cJSON_ArrayForEach(buddy, buddys) {
            //fixme skip if my serverip name
            jready = tcp_client_json_init_open(buddy->valuestring, JSON_PORT, hptr);
        }
    }
    return jready;
}
// when we have a new buffer, if it is "fresher" update the local json
void tcp_client_json_handle_reply(int size, uint8_t *buffer) {
    struct tcp_json_header *hptr = (struct tcp_json_header *)buffer;
    printf("%s size: %d his version %d\n", __func__, size, hptr->data_version);
    if (size <= sizeof(*hptr)) {
        return; //server agrees my data is freshest;
    }
    int my_counter = get_mirror_update_count();
    if (my_counter <= hptr->data_version) {
        // I am out of date, use servers version of json.
        printf("%s client data version: %d his version %d\n", __func__, my_counter, hptr->data_version);
        get_mirror();
        mirror = cJSON_Parse((const char *)hptr + sizeof(*hptr)); //skip header
        if (mirror == NULL) {
            printf("%s could not parse servers json\n", __func__);
        }
        // hptr->size = sizeof(*hptr); //not relevant, not sending anything back
    }
}
//return number of bytes to send to client. 0 if there is some total error and
//an abort should be used.

//If the local json has a higher version counter, send header and json
//otherwise just send the header. Supplied with a transmitted binary buffer
int tcp_server_json_check_freshness(struct tcp_json_header *hptr) {
    // struct tcp_json_header *hptr = (struct tcp_json_header *)json_buffer;
    //json_get_counter_value inits the header to send everything, hdr and json
    int my_counter = get_mirror_update_count();
    if (my_counter <= hptr->data_version) {
        // client has newer version, use his json and I need to send to the
        // client just the header, not json data
        printf("%s my data version: %d his version %d I accept client version\n", __func__, my_counter, hptr->data_version);
        get_mirror();
        mirror = cJSON_Parse((const char *)hptr + sizeof(*hptr)); //skip header
        if (mirror == NULL) {
            printf("%s could not parse servers json\n", __func__);
        }
        hptr->size = sizeof(*hptr);
    } else {
        printf("%s my data version: %d his version %d use my version len=%d\n", __func__, my_counter, hptr->data_version, hptr->size);
        hptr->size = sizeof(*hptr) + 10; //flag big value as a need to send local json
    }
    return hptr->size;
}
