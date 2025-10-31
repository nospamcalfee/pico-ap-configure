#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include "ssi.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
#include "relay_control.h"
#include "json_handler.h"

#undef C
#define C(x) #x,
const char * const application_ssi_tags[] = { RUN_STATE_NAMES };
#undef C

// SSI tags - tag length limited to 8 bytes by default
#define TAG_NAMES C(dtime)C(host)C(volt)C(relay)C(temp)C(led)C(u_cnt)C(json)
#define C(x) x,
enum ssi_tag_enum { TAG_NAMES };
#undef C
#define C(x) #x,
static const char * ssi_tags_name[] = { TAG_NAMES };
#undef C

char local_host_name[20];
// const char * ssi_tags[] = {"volt","temp","led", etc};

short unsigned int ssi_handler(int iIndex, char *pcInsert, int iInsertLen
#if LWIP_HTTPD_SSI_MULTIPART
    , uint16_t current_tag_part, uint16_t *next_tag_part
#endif
) {
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
      printed = snprintf(pcInsert, iInsertLen, "%f ins=%p ilen=%d", voltage, pcInsert, iInsertLen);
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
  case u_cnt:
    {
      inc_mirror_counter(mirror);
      printed = snprintf(pcInsert, iInsertLen, "%d", get_mirror_update_count());
    }
    break;
#if LWIP_HTTPD_SSI_MULTIPART
    #if 0
        //this crazy call assumes the entire iInsertLen buffer will be filled
        //or an indication of the last buffer is sent.
        //the sent string does not appear to be inspected.
        //snprintf is also difficult, it returns the amount it would have printed.
        case json: {
            static int remlen;
            int thischunk;
            struct tcp_json_header *hptr = (struct tcp_json_header *)json_buffer;
            uint8_t *jptr = (uint8_t *)hptr + sizeof(*hptr);

            if (current_tag_part == 0){
                remlen = 0;
                int good = json_prep_get_counter_value(hptr); //get json ascii
                if (good >= 0) {
                    remlen = hptr->size - sizeof(*hptr);
                }
            }
            int sofar = hptr->size - sizeof(*hptr) - remlen; //offset in json buffer
            printed = snprintf(pcInsert, iInsertLen, "\n|||%d %d xxxx 0123456789 is a big old data json chunk %d|||", sofar, remlen, current_tag_part);
            if (printed < 0) {
                break; //should not happen
            } else if (printed >= iInsertLen) {
                //truncated, but some was sent, should not happen here
            } else {
                //fit, print some more
                thischunk = iInsertLen - printed;
                // if (thischunk > remlen) {
                //     thischunk = remlen; //only print what is left
                // }
            }
            int lastprinted = printed;
            printed = snprintf(pcInsert + printed, thischunk, "%s", jptr + sofar);
            if (printed < 0) {
                break; //should not happen
            } else if (printed >= thischunk) {
                //truncated, but some was sent
                remlen -= thischunk - 1;
                *next_tag_part = current_tag_part + 1; //flag we have more to print
                printed = thischunk + lastprinted - 1; //correct the last section output amount.
            } else {
                //Leave "next_tag_part" unchanged to indicate that all data has been returned for this tag
                printed = printed + lastprinted; //correct the last section output amount.
            }
            break;
        }
    #else
        //now that I supposedly understand the ssi transfer protocol, try it
        //again without snprintf
        case json: {
            int remlen;
            int thischunk = iInsertLen - 1;
            //note json_buffer is not reentrant and is racy.
            struct tcp_json_header *hptr = (struct tcp_json_header *)json_buffer;
            uint8_t *jptr = (uint8_t *)hptr + sizeof(*hptr);

            if (current_tag_part == 0){
                remlen = 0;
                int good = json_prep_get_counter_value(hptr); //get json ascii
                if (good >= 0) {
                    remlen = hptr->size - sizeof(*hptr);
                }
            } else {
                remlen = hptr->size - sizeof(*hptr); //total size
                remlen -= current_tag_part * thischunk; //this entry offset
            }
            if (remlen > thischunk) {
                pcInsert[thischunk] = '\0'; //fixme is this needed?
            } else {
                //this is a short chunk
                thischunk = remlen;
            }
            int sofar = hptr->size - sizeof(*hptr) - remlen; //offset in json buffer
            memcpy(pcInsert, jptr + sofar, thischunk); //copy data to netbuf
            if (thischunk >= iInsertLen - 1) {
                //truncated, but some was sent
                *next_tag_part = current_tag_part + 1; //flag we have more to print
            } else {
                //on last, shorter send Leave "next_tag_part" unchanged to
                //indicate that all data has been returned for this tag
            }
            printed = thischunk; //return last section output amount.
            break;
        }
    #endif
#endif
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
