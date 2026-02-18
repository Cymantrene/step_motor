#include "estop.h"

/* volatile — потому что меняется из прерывания */
static volatile uint8_t estop_active = 0;

void Estop_Trigger(void) {
	estop_active = 1;
}

uint8_t Estop_IsActive(void) {
	return estop_active;
}

void Estop_Clear(void) {
	estop_active = 0;
}
