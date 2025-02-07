#ifndef LWIP_SSI_H
#define LWIP_SSI_H

#define C(x) x,
#define RUN_STATE_NAMES C(RUN_STATE_ACCESS_POINT)C(RUN_STATE_NTP)C(RUN_STATE_APPLICATION)
enum app_run_state { RUN_STATE_NAMES RUN_STATE_TOP };
#undef C

extern const char * const application_ssi_tags[];
//run_state gets updated as we progress in starting the app

extern uint8_t ssi_state; //set to run_state

void ssi_init(enum app_run_state dispset);
#endif
