#ifndef _RELAY_CONTROL_H
#define _RELAY_CONTROL_H
#include "pico/stdlib.h"

//legal gpio pins for the sprinkler, up to 4 allowed
//define set of relays required, max set on reserved pins
#define SPRINKLER_RELAY_GPIO 22
#define SPRINKLER_RELAY_GPIOB 21
//define a mask for all relays possibly in use
#define SPRINKLER_ALL_RELAY_MASK (1 << SPRINKLER_RELAY_GPIO | 1 << SPRINKLER_RELAY_GPIOB)
// #define SPRINKLER_ALL_RELAY_MASK (1 << (SPRINKLER_RELAY_GPIO))

//only 1 relay will be on at a time!
//call with 1 to turn on, or 0 to turn off
void relay_put(int pin, int dir);

//init with as many relays as is connected
//turn them all of when inited
void relay_init_mask(void);

//read the current gpio out setting
uint8_t relay_get(uint gpio);

#endif //_RELAY_CONTROL_H
