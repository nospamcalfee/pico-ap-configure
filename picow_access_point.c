#include "lwip/apps/httpd.h"
#include "lwip/apps/mdns.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwipopts.h"
#include "ssi.h"
#include "cgi.h"
#include "dhcpserver.h"
#include "dnsserver.h"
//void mdns_example_init(void);
//WIFI Credentials - passed from cmake command, later maybe overwritten
char wifi_ssid[] = WIFI_SSID;
char wifi_password[] = WIFI_PASSWORD;


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

#define DEBUG_printf printf
void be_access_point() {
    //hang here until someone configures to my local ssid
    TCP_SERVER_T *state = calloc(1, sizeof(TCP_SERVER_T));

    httpd_init();
    printf("Http server for configuration initialised\n");

    // Configure SSI and CGI handler
    ssi_init(1); //we need ssid form
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
    IP4_ADDR(ip_2_ip4(&state->gw), 192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    // Start the dhcp server
    dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &state->gw, &mask);

    // Start the dns server
    dns_server_t dns_server;
    dns_server_init(&dns_server, &state->gw);
    int loopcnt = 40;
    while(loopcnt-- > 0) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(900);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(100);
        printf("Connected as AP to %s.local: ",
            netif_get_hostname(netif_default));
        printf("IP addr: %s\n",
            ip4addr_ntoa(netif_ip4_addr(netif_list)));
    };
    // tcp_server_close(state);
    dns_server_deinit(&dns_server);
    dhcp_server_deinit(&dhcp_server);
    cyw43_arch_disable_ap_mode();
}
int main() {
    stdio_init_all();

    cyw43_arch_init();


    be_access_point();
    // mdns_resp_init();
    // mdns_example_init();
    cyw43_arch_deinit();
    cyw43_arch_init();

    cyw43_arch_enable_sta_mode();
    // Connect to the WiFI network - loop until connected
    while(cyw43_arch_wifi_connect_timeout_ms(wifi_ssid, wifi_password, CYW43_AUTH_WPA2_AES_PSK, 30000) != 0){
        printf("failed to connect...\n");
        be_access_point();
    }
    // Print a success message once connected
    printf("Connected! \n");

    // mdns_resp_add_netif(netif_list, netif_list->hostname);
    // httpd_init();
    printf("Http server initialised\n");

    printf("Connected: %s picoW IP addr: %s\n", netif_get_hostname(netif_default), ip4addr_ntoa(netif_ip4_addr(netif_list)));

    // Initialise web server
    // Configure SSI and CGI handler
    ssi_init(0); //no ssid form
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