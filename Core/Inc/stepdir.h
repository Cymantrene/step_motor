#ifndef STEPDIR_H
#define STEPDIR_H

#include "stm32g4xx_hal.h"
#include "axis.h"
#include <stdint.h>

typedef struct {
	Axis *axis;

	GPIO_TypeDef *dir_port;
	uint16_t dir_pin;
	uint32_t step_interval_ticks;   // <<< ВАЖНО
	uint32_t step_timer;

	volatile int32_t pending_steps;

	/* --- добавляем аккуратно --- */
	uint32_t current_freq;   // текущая частота шагов
	uint32_t target_freq;    // целевая частота

	int32_t accel_q16;   // текущее ускорение (Hz/s) в формате Q16.16
	int32_t jerk_q16;    // рывок (Hz/s^2) в формате Q16.16 (константа для оси)

} StepDir;

void StepDir_Init(StepDir *sd, Axis *axis, GPIO_TypeDef *dir_port,
		uint16_t dir_pin);

void StepDir_OnStepIRQ(StepDir *sd);
void StepDir_Process(StepDir *sd);
void StepDir_SetSpeed(StepDir *sd, uint16_t pot_value);
void StepDir_UpdateSpeed(StepDir *sd, uint16_t pot_value);
void StepDir_UpdateSpeedScurve(StepDir *sd, uint16_t pot_value);
void StepDir_UpdateTargetFreqScurve(StepDir *sd, uint32_t target_freq);



#endif
