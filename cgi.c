#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"
#include "relay_control.h"
#include "flash_io.h"

// CGI handler which is run when a request for /led.cgi is detected
const char * cgi_led_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    // Check if an request for LED has been made (/led.cgi?led=x)
    if (strcmp(pcParam[0] , "led") == 0){
        // Look at the argument to check if LED is to be turned on (x=1) or off (x=0)
        if(strcmp(pcValue[0], "0") == 0)
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        else if(strcmp(pcValue[0], "1") == 0)
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    }
    // Send the index page back to the user
    return "/index.shtml";
}
// CGI handler which is run when a request for /relay.cgi is detected
const char * cgi_relay_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    // Check if an request for relay has been made (/relay.cgi?relay=x)
    if (strcmp(pcParam[0] , "relay") == 0){
        // Look at the argument to check if relay is to be turned on (x=1) or off (x=0)
        if(strcmp(pcValue[0], "0") == 0)
            relay_put(SPRINKLER_RELAY_GPIO, 0);
        else if(strcmp(pcValue[0], "1") == 0)
            relay_put(SPRINKLER_RELAY_GPIO, 1);
    }
    if (strcmp(pcParam[0] , "relay2") == 0){
        // Look at the argument to check if relay is to be turned on (x=1) or off (x=0)
        if(strcmp(pcValue[0], "0") == 0)
            relay_put(SPRINKLER_RELAY_GPIOB, 0);
        else if(strcmp(pcValue[0], "1") == 0)
            relay_put(SPRINKLER_RELAY_GPIOB, 1);
    }
    // Send the index page back to the user
    return "/index.shtml";
}
// CGI handler which is run to reset all flash -- very dangerous
const char * cgi_resflash_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    // Check if an request for resflash has been made (/resflash.cgi)
    if (strcmp(pcParam[0] , "resflash") == 0) {
         flash_io_erase_ssids_hostnames();
    }
    // Send the index page back to the user
    return "/index.shtml";
}

// tCGI Struct
// Fill this with all of the CGI requests and their respective handlers
static const tCGI cgi_handlers[] = {
    {
        // Html request for "/led.cgi" triggers cgi_handler
        "/led.cgi", cgi_led_handler,
    },
    {
        "/relay.cgi", cgi_relay_handler
    },
    {
        "/resflash.cgi", cgi_resflash_handler
    },
};

void cgi_init(void)
{
    http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));
}