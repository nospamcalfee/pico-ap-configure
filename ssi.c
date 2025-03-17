#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include "ssi.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
#include "relay_control.h"

#undef C
#define C(x) #x,
const char * const application_ssi_tags[] = { RUN_STATE_NAMES };
#undef C

// SSI tags - tag length limited to 8 bytes by default
#define TAG_NAMES C(dtime)C(host)C(volt)C(relay)C(temp)C(led)
#define C(x) x,
enum ssi_tag_enum { TAG_NAMES };
#undef C
#define C(x) #x,
static const char * ssi_tags_name[] = { TAG_NAMES };
#undef C

char local_host_name[20];
// const char * ssi_tags[] = {"volt","temp","led", etc};

short unsigned int ssi_handler(int iIndex, char *pcInsert, int iInsertLen) {
  size_t printed;
  switch (iIndex) {
  case dtime:
    {
        datetime_t t;
        rtc_get_datetime(&t);
        datetime_to_str(pcInsert, iInsertLen, &t);
        printed = strlen(pcInsert); //return length of date/time
    }
    break;
  case host:
    {
        printed = snprintf(pcInsert, iInsertLen, "%s", local_host_name);
    }
    break;

  case volt:
    {
      const float voltage = adc_read() * 3.3f / (1 << 12);
      printed = snprintf(pcInsert, iInsertLen, "%f", voltage);
    }
    break;
  case relay:
    {
      int relay_list = SPRINKLER_ALL_RELAY_MASK;
      printed = 0;
      int last_pin = 0;
      while (relay_list) {
          int pin_offset = ffs(relay_list);
          relay_list >>= pin_offset; //for next loop
          last_pin += pin_offset; //keep correct pin number
          //possibly return multiple values
          printed += snprintf(pcInsert + printed, iInsertLen - printed, "%d ", relay_get(last_pin - 1));
      }
      if (printed == 0) {
          printed += snprintf(pcInsert, iInsertLen, "No Relays Defined ");
      }
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
  http_set_ssi_handler(ssi_handler, ssi_tags_name, LWIP_ARRAYSIZE(ssi_tags_name));
}
