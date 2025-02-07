#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include "ssi.h"

#undef C
#define C(x) #x,
const char * const application_ssi_tags[] = { RUN_STATE_NAMES };
#undef C

// SSI tags - tag length limited to 8 bytes by default
#define TAG_NAMES C(volt)C(temp)C(led)C(disp)C(ntpready)
#define C(x) x,
enum ssi_tag_enum { TAG_NAMES };
#undef C
#define C(x) #x,
static const char * ssi_tags_name[] = { TAG_NAMES };
#undef C

// const char * ssi_tags[] = {"volt","temp","led", "disp", "ntpready"};
uint8_t ssi_state; //0 is we don't have ntp time yet.

short unsigned int ssi_handler(int iIndex, char *pcInsert, int iInsertLen) {
  size_t printed;
  switch (iIndex) {
  case volt:
    {
      const float voltage = adc_read() * 3.3f / (1 << 12);
      printed = snprintf(pcInsert, iInsertLen, "%f", voltage);
    }
    break;
  case temp:
    {
    const float voltage = adc_read() * 3.3f / (1 << 12);
    const float tempC = 27.0f - (voltage - 0.706f) / 0.001721f;
    printed = snprintf(pcInsert, iInsertLen, "%f", tempC);
    }
    break;
  case led:
    {
      bool led_status = cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN);
      if(led_status == true){
        printed = snprintf(pcInsert, iInsertLen, "ON");
      }
      else{
        printed = snprintf(pcInsert, iInsertLen, "OFF");
      }
    }
    break;
  case disp:
    {
      if (ssi_state == RUN_STATE_ACCESS_POINT) {
        printed = snprintf(pcInsert, iInsertLen, "block");
      } else {
        printed = snprintf(pcInsert, iInsertLen, "none");
      }

    }
  break;
  case ntpready:
    {
      if (ssi_state == RUN_STATE_APPLICATION) { //we have ntp time
        printed = snprintf(pcInsert, iInsertLen, "block");
      } else {
        printed = snprintf(pcInsert, iInsertLen, "none");
      }
    }
    break;
  default:
    printed = 0;
    break;
  }

  return (short unsigned int)printed;
}

// Initialise the SSI handler
//dispset says 1 to display initialize network form or 0 not to display it
void ssi_init(enum app_run_state dispset) {
  // Initialise ADC (internal pin)
  adc_init();
  adc_set_temp_sensor_enabled(true);
  adc_select_input(4);
  ssi_state = dispset;
  http_set_ssi_handler(ssi_handler, ssi_tags_name, LWIP_ARRAYSIZE(ssi_tags_name));
}
