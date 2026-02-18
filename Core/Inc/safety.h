// ===================== safety.h =====================
#ifndef SAFETY_H
#define SAFETY_H

#include "stm32g4xx_hal.h"
#include "driver.h"
#include <stdint.h>

// состояние системы безопасности
typedef enum {
	SAFETY_OK = 0, SAFETY_ESTOP, SAFETY_FAULT
} SafetyState;

typedef struct {
	GPIO_TypeDef *estop_port;
	uint16_t estop_pin;

	Driver *drv1;
	Driver *drv2;

	SafetyState state;

} Safety;

void Safety_Init(Safety *s, GPIO_TypeDef *estop_port, uint16_t estop_pin,
		Driver *drv1, Driver *drv2);

void Safety_Update(Safety *s);
SafetyState Safety_GetState(Safety *s);

#endif
