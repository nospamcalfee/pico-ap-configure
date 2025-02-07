#ifndef LWIP_SSI_H
#define LWIP_SSI_H

enum run_state {
    RUN_STATE_AP, RUN_STATE_NTP, RUN_STATE_APP
};
//run_state gets updated as we progress in starting the app

extern uint8_t ssi_state; //set to run_state

void ssi_init(enum run_state dispset);
#endif
