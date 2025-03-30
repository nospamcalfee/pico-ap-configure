#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "flash_io.h"
#include "find_local_ssid.h"

struct cdll knownnodes; //global list base
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
        if (result->rssi > -80) {
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
struct my_scan_result {
    uint8_t ssid[32];   ///< wlan access point name
    uint16_t channel;   ///< wifi channel
    int16_t rssi;       ///< signal strength
};
struct my_params{
    bool found;
    struct cdll ll;
    struct my_scan_result res;
};

#define cast_cdll_to_my_params(pt) (cast_p_to_outer( \
            struct cdll *, pt, \
            struct my_params, ll))

//return 0 if not unique
static int unique_ssid(const uint8_t *ssid) {
    struct cdll *ll;
    struct my_params *test;
    struct my_scan_result *result;
    if (cdll_empty(&knownnodes)) {
        return 1; //if list is empty, it is unique
    }
    cdll_for_each(ll, &knownnodes) {
        test = cast_cdll_to_my_params(ll);
        result = &test->res;
        if (strlen(result->ssid) == strlen(ssid)) {
            int r = memcmp(ssid, result->ssid, strlen(result->ssid));
            if (!r) {
                return r; //found a match, not unique
            }
        }
    }
    return 1; //searched all, not unique
}
//return 1 if new ssid is better than current
static struct my_scan_result *better_rssi(const uint8_t *ssid, int rssi)
{
    struct cdll *ll;
    struct my_params *test;
    struct my_scan_result *result;
    cdll_for_each(ll, &knownnodes) {
        test = cast_cdll_to_my_params(ll);
        result = &test->res;
        if (strlen(result->ssid) == strlen(ssid)) {
            int r = rssi > result->rssi;
            if (r) {
                return result; //found a better rssi
            }
        }
    }
    return NULL; //searched all, no better rssi
}
static int scan_all_result(void *env, const cyw43_ev_scan_result_t *result) {
    if (result) {
        int uniq = 0;
        printf("ssid: %-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\n",
            result->ssid, result->rssi, result->channel,
            result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5],
            result->auth_mode);
        if (unique_ssid(result->ssid)) {
            uniq = 1;
        }
        //unique or not and better rssi gets a new entry
        if (uniq) {
            struct my_params *listtest = calloc(1, sizeof(*listtest));
            cdll_init(&listtest->ll);
            memcpy(listtest->res.ssid, result->ssid, sizeof(listtest->res.ssid)); //make a copy of the returned struct
            listtest->res.channel = result->channel;
            listtest->res.rssi = result->rssi;
            cdll_insert_node_tail(&listtest->ll, &knownnodes);
            printf("scanlist %p ll=%p\n",  listtest, &listtest->ll);
        } else {
            //not unique, is it a better choice with a better rssi?
            struct my_scan_result *better = better_rssi(result->ssid, result->rssi);
            if (better) {
                better->channel = result->channel;
                better->rssi = result->rssi;
                printf("new better scan %p chan: %3D rssi %4d\n",  better->channel, better->rssi);
            }
        }
    }
    return 0;
}
static void printlist(struct cdll *p)
{
    struct cdll *ll;
    struct my_params *test;
    struct my_scan_result *result;
    cdll_for_each(ll, p) {
        test = cast_cdll_to_my_params(ll);
        result = &test->res;
        printf("list ssid: %-32s rssi: %4d chan: %3d\n",
            result->ssid, result->rssi, result->channel);
        printf("list %p ll=%p\n", test, ll);
    }

}
/* remove a cdll list included in a my_params struct */
void removelist(struct cdll *p)
{
    struct cdll *vlist;
    struct my_params *test;
    //cannot use cdll_for_each because loop uses freed pointer
    struct cdll *next_vlist = p;
    for (vlist = p->next; vlist != p; vlist = next_vlist) {
        test = cast_cdll_to_my_params(vlist);
        next_vlist = vlist->next;
        printf("fifo delete %p ll=%p known=%p\n", vlist, next_vlist, p->next);
        cdll_delete_node(vlist);
        free(cast_cdll_to_my_params(vlist));
    }
}

/*
    return 0 if all ssids have been scanned, or failure code
*/
int scan_find_all_ssids()
{
    // struct cdll *ll = &localll;
    cdll_init(&knownnodes); //init parent
    // cdll_init(ll);  //get my list ready

    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    struct my_params params = { false, &knownnodes};
    cyw43_wifi_scan_options_t scan_options = {0};
    printf("\nPerforming wifi scan and list\n");
    int err = cyw43_wifi_scan(&cyw43_state, &scan_options, &params, scan_all_result);
    hard_assert(err == 0);

    while (cyw43_wifi_scan_active(&cyw43_state)) {
        // cyw43_arch_poll();
        // cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
        sleep_ms(1000);
    }
    cyw43_arch_deinit();
    printlist(&knownnodes);
    removelist(&knownnodes);
    return 0;
}
