#include "pico/stdlib.h"
#include "relay_control.h"

//call with 1 to turn on, or 0 to turn off
//with paranoia turn all relays off, then do the put
void relay_put(int pin, int dir) {
	gpio_clr_mask(SPRINKLER_ALL_RELAY_MASK); //clear all the relays
	gpio_put(pin, dir);
}
//init with as many relays as is connected
void relay_init_mask(void) {
	gpio_init_mask(SPRINKLER_ALL_RELAY_MASK);
	//gpio_init(pin);
	//init all gpios to outputs
	gpio_set_dir_masked(SPRINKLER_ALL_RELAY_MASK, SPRINKLER_ALL_RELAY_MASK);
	// gpio_set_dir(pin, GPIO_OUT);
	gpio_clr_mask(SPRINKLER_ALL_RELAY_MASK); //clear all the relays
	// relay_put(pin, 0); //init off
}
