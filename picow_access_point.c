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
void mdns_example_init(void);

void set_host_name(const char*hostname)
{
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

#define DEBUG_printf printf

void be_access_point() {
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

    const char *ap_name = "picow_test";
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
    for (int i = 0; i < 60; i++) {
    // while(!config_changed) {
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
            break; //got a new ssid
        }
    };
    // tcp_server_close(post_state);
    dns_server_deinit(&dns_server);
    dhcp_server_deinit(&dhcp_server);
    // free(post_state);
    cyw43_arch_disable_ap_mode();
}
int main() {
    stdio_init_all();

     //init with default ssid/password - for now from compile, later from flash
    strncpy(wifi_ssid, WIFI_SSID, sizeof(wifi_ssid)-1);
    wifi_ssid[sizeof(wifi_ssid) - 1] = 0;
    strncpy(wifi_password, WIFI_PASSWORD, sizeof(wifi_password)-1);
    wifi_password[sizeof(wifi_password) - 1] = 0;

    // be_access_point(); //for test start with ap mode
    cyw43_arch_init();

    cyw43_arch_enable_sta_mode();
    httpd_init();
    // Connect to the WiFI network - loop until connected
     while(cyw43_arch_wifi_connect_timeout_ms(wifi_ssid, wifi_password, CYW43_AUTH_WPA2_AES_PSK, 30000) != 0){
        printf("failed to connect...\n");
        config_changed = 0; // prepare app for ssid config change.
        be_access_point();
        cyw43_arch_deinit();
        cyw43_arch_init();
        cyw43_arch_enable_sta_mode();
    }
    // Print a success message once connected
    printf("Connected! \n");

    mdns_example_init();

    printf("Http server initialised\n");

    printf("Connected: %s picoW IP addr: %s\n", netif_get_hostname(netif_default), ip4addr_ntoa(netif_ip4_addr(netif_list)));

    // Initialise web server
    // Configure SSI and CGI handler
    ssi_init(RUN_STATE_APPLICATION); //no ssid form
    printf("SSI Handler initialised\n");
    cgi_init();
    printf("CGI Handler initialised\n");

    // Infinite loop
    while(1) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(9000);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(1000);
        printf("Connected to %s.local: IP addr: %s\n", netif_get_hostname(netif_default), ip4addr_ntoa(netif_ip4_addr(netif_list)));
    };
}