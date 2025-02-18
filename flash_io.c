#include "ring_buffer.h"
#include "pico/stdlib.h"
#include "flash_io.h"

#define SSID_BUFF (__PERSISTENT_TABLE)
#define SSID_LEN __PERSISTENT_LEN
#define SSID_ID 0x01
//need a pagebuff to do a write/delete but it is not needed between calls, everyone can use it.
uint8_t pagebuff[FLASH_PAGE_SIZE];

/*
 read all ssids from flash. return number of successful reads or negative error
 status
*/
int read_ssids(rb_t *rb){
    int err;

    err = rb_recreate(rb, SSID_BUFF, SSID_LEN / FLASH_SECTOR_SIZE, CREATE_INIT_IF_FAIL);
    if (!(err == RB_OK || err == RB_BLANK_HDR || err == RB_HDR_LOOP)) {
        printf("reopening flash error %d, quitting\n", err);
        return -err;
    }
    uint32_t loopcount = 0;
    while (true) {
        err = rb_read(rb, SSID_ID, pagebuff, sizeof(pagebuff));

        // hexdump(stdout, pagebuff, err + 1, 16, 8);
        if (err <= 0) {
            if (-err != RB_BLANK_HDR) {
                printf("some non-blank read failure %d\n", err);
            }
            break;
        } else {
            printf("Reading ssid %ld starting at 0x%lx stat=%d\n\"%s\"\n", loopcount, rb->next, err, pagebuff);
        }
        loopcount++; //count successes
    }
    if (loopcount) {
        return loopcount;
    }
    return -err;
}
//read a specific ssid entry
int read_ssid(rb_t *rb, int n){
    int i;
    int err;
    err = rb_recreate(rb, SSID_BUFF, SSID_LEN / FLASH_SECTOR_SIZE, CREATE_INIT_IF_FAIL);
    if (!(err == RB_OK || err == RB_BLANK_HDR || err == RB_HDR_LOOP)) {
        printf("reopening flash error %d, quitting\n", err);
        return -err;
    }
    for (i = 1; i < n; i++) {
        err = rb_read(rb, SSID_ID, pagebuff, sizeof(pagebuff));

        printf("skipping ssid %d starting at 0x%lx stat=%d\n\"%s\"\n", i, rb->next, err, pagebuff);
        if (err <= 0) {
            printf("some read failure %d\n", err);
            return err;
        }
    }
    //now read the desired flash entry
    err = rb_read(rb, SSID_ID, pagebuff, sizeof(pagebuff));

    printf("reading ssid %d starting at 0x%lx stat=%d\n\"%s\"\n", i, rb->next, err, pagebuff);
    if (err <= 0) {
        printf("final %d read failure %d\n", i, err);
        return err;
    }
    return err; //return actual length
}
//if ssids are not in flash, write them
//for safety write both the ssid and the password as 2 strings to flash
void create_ssid_rb(rb_t *rb, enum init_choices ssid_choice) {
    // create another write/read set of control buffers for an alternative flash buffer containing strings
    int err = rb_recreate(rb, SSID_BUFF, SSID_LEN / FLASH_SECTOR_SIZE, ssid_choice);
    if (!(err == RB_OK || err == RB_BLANK_HDR || err == RB_HDR_LOOP)) {
        printf("starting flash error %d, quitting\n", err);
        exit(1);
    }
}
//write a new ssid/pw pair
rb_errors_t write_ssid(rb_t *rb, char * ss, char *pw) {
    char tempssid[64]; //note this comes on a very small cpu stack of 4k
    uint32_t good_reads = read_ssids(rb); //number of saved ssids
    if (good_reads < 0) {
        good_reads = 0; //if I cannot read them, say there are no good ssids
    }
    //build 2 strings for one flash write
    int s1len = strlen(ss) + 1;
    memcpy(tempssid, ss, s1len);
    int s2len = strlen(pw) + 1;
    memcpy(tempssid + s1len, pw, s2len);

    rb_errors_t terr = rb_append(rb, SSID_ID, tempssid, s1len+s2len, pagebuff, true);
    printf("finally wrote ssid 0x%x at 0x%lx stat=%d\nssid=%s pw=%s\n",
            SSID_ID, rb->last_wrote, terr, tempssid, tempssid + s1len);

    return terr;
}
