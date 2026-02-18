#ifndef MOTION_H
#define MOTION_H

#include <stdint.h>
#include "axis.h"
#include "driver.h"

/*
 * Motion — тонкий “слой управления движением” над Axis.
 * Axis занимается генерацией STEP/профилем, Motion — командами, состоянием и удобным API.
 */

typedef enum {
	AXIS_1 = 0, AXIS_2 = 1
} MotionAxisId;

typedef enum {
	MOTION_OK = 0, MOTION_BUSY,        // ось уже выполняет движение
	MOTION_ESTOP,       // активна авария
	MOTION_BAD_AXIS     // неверный ID оси
} MotionResult;

typedef struct {
	Axis *a1;
	Axis *a2;
	Driver *d1;
	Driver *d2;
} Motion;

/* Инициализация слоя Motion — просто сохраняем указатели */
void Motion_Init(Motion *m, Axis *axis1, Axis *axis2, Driver *drv1,
		Driver *drv2);

/*
 * Запустить движение на N полных шагов:
 *  steps > 0  => вперёд
 *  steps < 0  => назад
 * Возвращает:
 *  MOTION_OK   — команда принята
 *  MOTION_BUSY — ось занята
 *  MOTION_ESTOP — авария активна (двигаться нельзя)
 */
MotionResult Motion_Move(Motion *m, MotionAxisId id, int32_t steps);

/* Проверка: ось сейчас выполняет движение? */
uint8_t Motion_IsBusy(Motion *m, MotionAxisId id);

/* Аварийная остановка (жёстко) — отключить оси и драйверы */
void Motion_EStop(Motion *m);

/* Остановить все оси (пока сделаем как аварийную; позже сделаем “мягкий стоп”) */
void Motion_StopAll(Motion *m);

#endif /* MOTION_H */
