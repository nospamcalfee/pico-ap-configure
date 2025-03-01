#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "flash_io.h"

static uint32_t ssid_found; //which ssid matches

//function to find ssid in flash return match number
static int scan_search(const uint8_t *ssid){
    //number of ids ranges from 1 to bigger
    int number_of_ssids = read_flash_ids(SSID_ID, SSID_BUFF, SSID_LEN);
    int len;
    for (int i = 1; i <= number_of_ssids; i++) {
        len = read_flash_id_n(SSID_ID, SSID_BUFF, SSID_LEN, i);
        if (len <= 0) {
            printf("error reading flashid %d", i);
            return len;
        }
        void *flash_l = memchr(pagebuff, '\0', 32);
        int ssid_len = strnlen(ssid, 32);
        if (flash_l) {
            //found terminator
            int flash_len = flash_l - (void *)pagebuff;
            if (ssid_len == flash_len) {
                if (!memcmp(ssid, pagebuff, ssid_len)) {
                    //we have a match
                    printf("scan match ssid#=%d\n", i);
                    return i;   //return entry number
                }
            }
        }
    }
    return 0; //no match
}

static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    if (result) {
        printf("ssid: %-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\n",
            result->ssid, result->rssi, result->channel,
            result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5],
            result->auth_mode);
        if (result->rssi > -70) {
            //ignore weak wifi signals
            int found = scan_search(result->ssid);
            if (found) {
                printf("stop scan %d ssid#=%d\n", found, ssid_found);
                uint32_t *t = env;
                *t = found; //return entry number
                return 0;   //return scan result
            }
        }
    }
    return 0;
}

//return flash entry number for matching ssid
int scan_find_ssid() {
    bool scan_in_progress = false;
    ssid_found = 0; //clear found flag
    while(true) {
        if (!scan_in_progress) {
            cyw43_wifi_scan_options_t scan_options = {0};
            //ok start the scan, returns callback return value, or error
            int err = cyw43_wifi_scan(&cyw43_state, &scan_options, &ssid_found, scan_result);
            if (err == 0) {
                printf("\nPerforming wifi scan\n");
                scan_in_progress = true;
            } else {
                printf("Failed to start scan: %d\n", err);
                hard_assert(true);
            }
        } else  //scan is going on, see if done.
            if (!cyw43_wifi_scan_active(&cyw43_state) || ssid_found) {
                scan_in_progress = false; 
                return ssid_found;
        }
    }
    return 0;
}