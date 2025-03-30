#include "lwip/apps/httpd.h"
#include "lwip/apps/mdns.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwipopts.h"
#include "ssi.h"
#include "cgi.h"
#include "dhcpserver.h"
#include "dnsserver.h"
#include "post_handler.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
#include "lwip/apps/sntp.h"
#include "ring_buffer.h"
#include "flash_io.h"
#include "relay_control.h"
#include "find_local_ssid.h"

void mdns_example_init(void);

void set_host_name(const char *hostname) {
    cyw43_arch_lwip_begin();
    struct netif *n = &cyw43_state.netif[CYW43_ITF_STA];
    netif_set_hostname(n, hostname);
    netif_set_up(n);
    cyw43_arch_lwip_end();
}
typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    bool complete;
    ip_addr_t gw;
    async_context_t *context;
} TCP_SERVER_T;

static TCP_SERVER_T *post_state;
static rb_t ssid_rb;    //keep buffer off stack

#define DEBUG_printf printf

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
        post_state = calloc(1, sizeof(TCP_SERVER_T));
    }
    // httpd_init();
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

    // Start the dhcp server
    dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &post_state->gw, &mask);

    // Start the dns server
    dns_server_t dns_server;
    dns_server_init(&dns_server, &post_state->gw);
    // wait until user sets a ssid/password
    for (int i = 0; i < 3*60; i++) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(900);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(100);
        printf("Connected as AP to %s.local: ",
            netif_get_hostname(netif_default));
        printf("Access Point IP addr: %s\n",
            ip4addr_ntoa(netif_ip4_addr(netif_list)));
        if (config_changed) {
            sleep_ms(2000); //seems to be a post handling race?
            flash_io_write_ssid(wifi_ssid, wifi_password);
            break; //got a new ssid
        }
    };
    // tcp_server_close(post_state);
    dns_server_deinit(&dns_server);
    dhcp_server_deinit(&dhcp_server);
    cyw43_arch_disable_ap_mode();
}

int main() {
    int8_t err;
    static char datetime_str[128];
    stdio_init_all();
    relay_init_mask(); //eventually control from the json file
    // set default Start on Friday 5th of June 2020 15:45:00
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

    scan_find_all_ssids();
    /*
        Starting up, first we see if we have any local wifi ssids that match
        known ones in flash. If not we just try the last entry in flash, if
        there is no last entry, we use the compiled in defaults. If we cannot
        connect to any of these 3 we become an AP and get the user to give us
        the local wifi credentials.
    */
    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();
    int ssidnumber = scan_find_ssid();
    cyw43_arch_deinit();

    if (ssidnumber) {
        //we found a correct ssid, use it.
        err = flash_io_read_ssid(ssidnumber);
    } else {
        err = flash_io_read_latest_ssid();
    }
    if (err < 0) {
        //init with default ssid/password - for now from compile, later from flash
        strncpy(wifi_ssid, WIFI_SSID, sizeof(wifi_ssid)-1);
        wifi_ssid[sizeof(wifi_ssid) - 1] = 0;
        strncpy(wifi_password, WIFI_PASSWORD, sizeof(wifi_password)-1);
        wifi_password[sizeof(wifi_password) - 1] = 0;
        flash_io_write_ssid(wifi_ssid, wifi_password); //write out default
    } else {
        // we have the raw data in pagebuff, move to ssid/pw
        int s1len = strlen(pagebuff) + 1;
        memcpy(wifi_ssid, pagebuff, s1len);
        int s2len = strlen(pagebuff + s1len) + 1;
        memcpy(wifi_password, pagebuff + s1len, s2len);
        printf("From flash wifi_ssid=%s wifi_password=%s\n",wifi_ssid, wifi_password);
    }

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

    httpd_init();

    // Connect to the WiFI network - loop until connected
     while(cyw43_arch_wifi_connect_timeout_ms(wifi_ssid, wifi_password, CYW43_AUTH_WPA2_AES_PSK, 30000) != 0){
        printf("failed to connect...\n");
        config_changed = 0; // prepare app for ssid config change.
        be_access_point(local_host_name);
        cyw43_arch_deinit();
        cyw43_arch_init();
        cyw43_arch_enable_sta_mode();
    }
    set_host_name(local_host_name);
    // Print a success message once connected
    printf("Connected! \n");

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

    // Infinite loop
    while(1) {
        datetime_t t;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(9000);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(1000);
        rtc_get_datetime(&t);
        datetime_to_str(datetime_str, sizeof(datetime_str), &t);

        printf("%s Connect to %s.local or IP addr: %s\n", datetime_str, netif_get_hostname(netif_default), ip4addr_ntoa(netif_ip4_addr(netif_list)));
    };
}
