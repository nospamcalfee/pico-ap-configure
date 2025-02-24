#include "pico/stdlib.h"
#include "relay_control.h"

//call with dir==1 to turn on, or dir==0 to turn off
//with paranoia turn all relays off, then do the put
void relay_put(int pin, int dir) {
	gpio_clr_mask(SPRINKLER_ALL_RELAY_MASK); //clear all the relays
	gpio_put(pin, dir);
}
//init with as many relays as is connected
void relay_init_mask(void) {
	gpio_init_mask(SPRINKLER_ALL_RELAY_MASK);
	//init all relay gpios to outputs
	gpio_set_dir_masked(SPRINKLER_ALL_RELAY_MASK, SPRINKLER_ALL_RELAY_MASK);
	gpio_clr_mask(SPRINKLER_ALL_RELAY_MASK); //clear all the relays
}

uint8_t relay_get(uint gpio){
	//read the current gpio out setting
	return gpio_get(gpio);
}