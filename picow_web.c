#include "lwip/apps/httpd.h"
#include "lwip/apps/mdns.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwipopts.h"
#include "ssi.h"
#include "cgi.h"
#include "post_handler.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
#include "lwip/apps/sntp.h"
#include "ring_buffer.h"
#include "flash_io.h"
#include "relay_control.h"
#include "find_local_ssid.h"
#include "picow_tcp.h"

void mdns_example_init(void);

void set_host_name(const char *hostname) {
    cyw43_arch_lwip_begin();
    struct netif *n = &cyw43_state.netif[CYW43_ITF_STA];
    netif_set_hostname(n, hostname);
    netif_set_up(n);
    cyw43_arch_lwip_end();
}
typedef struct HTTP_TCP_SERV_T_ {
    struct tcp_pcb *server_pcb;
    bool complete;
    ip_addr_t gw;
    async_context_t *context;
} HTTP_TCP_SERV_T;

static HTTP_TCP_SERV_T *post_state;
// static rb_t ssid_rb;    //keep buffer off stack

// #define DEBUG_printf printf

//seconds difference between 1.1.1970 and 1.1.1900
//ntp is based on since 1900
//rtc is based on since 1970
#define NTP_DELTA 2208988800
void set_system_time(u32_t secs){
    time_t epoch = secs - NTP_DELTA;
    datetime_t t;
    char datetime_str[128];
    struct tm *time = gmtime(&epoch);

datetime_t datetime = {
    .year = (int16_t) (time->tm_year + 1900),
    .month = (int8_t) (time->tm_mon + 1),
    .day = (int8_t) time->tm_mday,
    .hour = (int8_t) time->tm_hour,
    .min = (int8_t) time->tm_min,
    .sec = (int8_t) time->tm_sec,
    .dotw = (int8_t) time->tm_wday,
};

    rtc_set_datetime(&datetime);
    printf ("RTC set to: %04d-%02d-%02d %02d:%02d:%02d\n",
           time->tm_year + 1900, time->tm_mon + 1, time->tm_mday,
           time->tm_hour, time->tm_min, time->tm_sec);
}

void be_access_point(char *ap_name) {
    //hang here until someone configures to my local ssid
    if (post_state == NULL) {
        post_state = calloc(1, sizeof(HTTP_TCP_SERV_T));
    }
    config_changed = 0; // prepare app for ssid config change.
    printf("Http server for configuration initialised\n");

    // Configure SSI and CGI handler
    ssi_init(RUN_STATE_ACCESS_POINT); //we need ssid form
    printf("SSI Handler initialised\n");
    cgi_init();
    printf("CGI Handler initialised\n");

    // const char *ap_name = "picow_test";
#if 1
    const char *password = "password";
#else
    const char *password = NULL;
#endif

    cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);

    ip4_addr_t mask;
    IP4_ADDR(ip_2_ip4(&post_state->gw), 192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    // wait until user sets a ssid/password
    //fixme needs to be pretty long for a user, but for test, short
    for (int i = 0; i < 1 + 3*60; i++) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(900);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(100);
        // we may not have started mdns - so dont use local name
        printf("Access Point IP addr: %s\n",
            ip4addr_ntoa(netif_ip4_addr(netif_list)));
        if (config_changed) {
            sleep_ms(2000); //seems to be a post handling race?
            flash_io_write_ssid(wifi_ssid, wifi_password);
            break; //got a new ssid
        }
    };
    cyw43_arch_disable_ap_mode();
    sleep_ms(200); //after switch, connect seems racy?
}

int main() {
    int8_t err;
    static char datetime_str[128];
    stdio_init_all();
    relay_init_mask(); //eventually control from the json file
    // set default Start on Friday 5th of June 2020 15:45:00
    // if we cannot get to ntp, we will know because of the date
    datetime_t t = {
            .year  = 2020,
            .month = 06,
            .day   = 05,
            .dotw  = 5, // 0 is Sunday, so 5 is Friday
            .hour  = 15,
            .min   = 45,
            .sec   = 00
    };

    rtc_init();
    rtc_set_datetime(&t);
    // clk_sys is >2000x faster than clk_rtc, so datetime is not updated
    // immediately when rtc_get_datetime() is called. The delay is up to 3
    // RTC clock cycles (which is 64us with the default clock settings)
    sleep_us(100);

    /*
        Starting up, first we see if we have any local wifi ssids that match
        known ones in flash. If we cannot connect to any of these we become
        an AP and get the user to give us the local wifi credentials. If no
        ap creds are supplied, we time out and try again in the hope that a
        missing AP may turn up (due to power failure).

        details:

         once started, the wifi AP might not be up yet. So maybe we have to do another
         find of all available if all possible connections fails.

         The sequence is

         OUTER LOOP

         1) get a list of all APs

         INNER LOOP
             2) find the most powerful remaining ap in the list.

             3) see if the flash has an entry for this high power ap, if not in flash, go
             back to step 2 (Inner loop)

             4) if connection fails, retry from step 2 until all discovered and known APs
             have been tried.

         5) if connection still fails after all the known APs have been tried, become
         an AP to see if the user wants to configure wifi.

         6) after AP handling, which may timeout or may set a new flash AP entry,
         start over at step 1.

    */

    cyw43_arch_init();

    cyw43_arch_enable_sta_mode();
    u8_t hwaddr[8 /*NETIF_MAX_HWADDR_LEN*/];
    cyw43_wifi_get_mac(netif_default->state, netif_default->name[1] - '0', hwaddr);
    printf("mac address %02x.%02x.%02x.%02x.%02x.%02x\n", hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
    err = flash_io_read_latest_hostname();
    if (err<0) {
        //no flash hostname
        snprintf(local_host_name, sizeof(local_host_name), "%s_%02x_%02x_%02x", netif_get_hostname(netif_default),hwaddr[3], hwaddr[4], hwaddr[5]);
    } else {
        memcpy(local_host_name, pagebuff, err); //copy in flashes latest hostname
    }
    printf("local host name %s\n", local_host_name);
    set_host_name(local_host_name);

    struct my_scan_result *likelyAP = NULL;
    httpd_init();
    void *tcp_serv = tcp_server_init(NULL);
    struct TCP_CLIENT_T_ *tcp_client = tcp_client_init(NULL);
    int connected = 0; //outer loop exit flag when non-zeroS
    do {
        // outer loop, check all available local ssids on the air
        scan_find_all_ssids(); //build linked list of all local ssids
        // find next most powerful rssi in local ssid list
        cyw43_arch_deinit();
        cyw43_arch_init();
        cyw43_arch_enable_sta_mode();
        //see if in flash, and get password if so..
        do {
            //inner loop, see if any ssids are known in flash.
            likelyAP = scan_find_best_ap(wifi_password); //build list
            if (likelyAP) {
                //if local and in flash, see if I can connect
                memcpy(wifi_ssid, likelyAP->ssid, sizeof(wifi_ssid)); //get ssid set too
                // Connect to the WiFI network - can fail, so try again
                int conres;
                for (int i = 0; i < 4; i++) {
                    conres = cyw43_arch_wifi_connect_timeout_ms(wifi_ssid, wifi_password, CYW43_AUTH_WPA2_AES_PSK, 30000);
                    if (conres != PICO_ERROR_CONNECT_FAILED) {
                        break;  //some real error, exit loop
                    }
                    printf("err=%d retrying connect to %s p=%s...\n", conres, wifi_ssid, wifi_password);
                }
                if (conres != 0){
                    printf("err=%d failed to connect to %s p=%s...\n", conres, wifi_ssid, wifi_password);
                     // be access point for awhile, try to get user to set ssid and password
                    be_access_point(local_host_name);
                    //In any, case if fail, try again to connect to the new ap/password
                    likelyAP = NULL; //force another loop
                } else {
                    // Print a success message once connected
                    printf("Connected! host=%s ssid=%s pw=%s\n", local_host_name, wifi_ssid, wifi_password);
                    connected = 1; //flag to exit both loops
                }
            } else {
                //If an AP exists and we
                //don't have it in flash, be an AP and allow user to
                //configure. either does not exist, or not in our flash
                // be access point for awhile, try to get user to set ssid and password
                be_access_point(local_host_name);
                //In any, case if fail, try again to connect to the new ap/password
                likelyAP = NULL; //force another loop
                break; //exit inner loop
            }
        } while (likelyAP == NULL);
        removelist(&knownnodes); //discard list of active wifi ssids
    } while (!connected);
    //ready to run as a regular website

    set_host_name(local_host_name);

    mdns_example_init();

    printf("Http server initialised\n");

    printf("Connected: %s picoW IP addr: %s\n", netif_get_hostname(netif_default), ip4addr_ntoa(netif_ip4_addr(netif_list)));

    //start getting the time
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();

    // Initialise web server
    // Configure SSI and CGI handler
    ssi_init(RUN_STATE_APPLICATION); //no ssid form
    printf("SSI Handler initialised\n");
    cgi_init();
    printf("CGI Handler initialised\n");
    //start the control tcp server
    if (!tcp_server_open(tcp_serv, 4242, tcp_server_recv,
                        tcp_server_sent, tcp_server_send_data)) {
        tcp_server_result(tcp_serv, -1);
        //fixme what to do if I cannot start? wait, then reset?
    }
    // Infinite loop
    while(1) {
        datetime_t t;

        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

        bool tcp_client_sendtest_open(void *arg, const char *hostname, uint16_t port,
                            complete_callback completed);
        if (!tcp_client_sendtest_open(tcp_client, "jedediah.local", 4242, NULL)) {
            printf("client connection was busy, could not open");
        }
        sleep_ms(9000);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(1000);
        rtc_get_datetime(&t);
        datetime_to_str(datetime_str, sizeof(datetime_str), &t);

        printf("%s Connect to %s.local or IP addr: %s\n", datetime_str, netif_get_hostname(netif_default), ip4addr_ntoa(netif_ip4_addr(netif_list)));
    };
}
