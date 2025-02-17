#ifndef _FLASH_IO_H_
#define _FLASH_IO_H_
#include "ring_buffer.h"

#define SSID_BUFF (__PERSISTENT_TABLE)
#define SSID_LEN __PERSISTENT_LEN
#define SSID_ID 0x01
//need a pagebuff to do a write/delete but it is not needed between calls, everyone can use it.
extern uint8_t pagebuff[FLASH_PAGE_SIZE];

/*
 read all ssids from flash. return number of successful reads or negative error
 status
*/
int read_ssids(rb_t *rb);
//read a specific ssid entry
int read_ssid(rb_t *rb, int n);
//if ssids are not in flash, write them
//for safety write both the ssid and the password as 2 strings to flash
void create_ssid_rb(rb_t *rb, enum init_choices ssid_choice);
//write a new ssid/pw pair
rb_errors_t write_ssid(rb_t *rb, char * ss, char *pw);

#endif //_FLASH_IO_H_