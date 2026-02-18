#ifndef AXIS_H_
#define AXIS_H_

#include "stm32g4xx_hal.h"
#include <stdint.h>

// --- Константы системы ---
#define MOTION_TICK_HZ            20000U    // Частота прерывания (20 кГц)
#define AXIS_MIN_SPEED_USTEP_S    40U       // Минимальная скорость (микрошагов/сек)
#define START_SPEED_FULL          20U       // Начальная скорость (полных шагов/сек)

// --- Параметры S-Curve (плавность) ---
#define AXIS_ACCEL_MAX_USTEP_S2   40000U    // Макс. ускорение (микрошагов/сек^2)
#define AXIS_JERK_USTEP_S3        150000U   // Рывок (плавность ускорения)
#define AXIS_MAX_SPEED_USTEP_S    (MOTION_TICK_HZ - 1U)  // максимум по таймеру


typedef enum
{
	MICROSTEP_1 = 1,
	MICROSTEP_2 = 2,
	MICROSTEP_4 = 4,
	MICROSTEP_8 = 8,
	MICROSTEP_16 = 16,
	MICROSTEP_32 = 32,
	MICROSTEP_64 = 64
} Microstep;

typedef struct {
	// Состояние движения
	int32_t position;       // Текущая позиция (в микрошагах)
	int32_t remaining;      // Оставшийся путь (в микрошагах)
	uint32_t speed;         // Текущая скорость (микрошагов/сек)
	uint32_t target_speed;  // Целевая скорость (микрошагов/сек)
	uint32_t max_speed;     // Предел скорости для этой оси

	// Генерация шагов (Фазовый аккумулятор)
	uint32_t accumulator;
	uint8_t dir_holdoff_ticks;

	// Профиль скорости (S-Curve)
	uint32_t prof_tick_cnt;
	int32_t accel_q16;      // Ускорение в формате Q16.16

	// Аппаратная часть
	GPIO_TypeDef *step_port;
	uint16_t step_pin;
	GPIO_TypeDef *dir_port;
	uint16_t dir_pin;

	Microstep microstep;
	uint8_t dir;            // 1 - вперед, 0 - назад
	uint8_t active;         // Флаг работы
} Axis;

// Функции
void Axis_Init(Axis *a, GPIO_TypeDef *step_port, uint16_t step_pin,
		GPIO_TypeDef *dir_port, uint16_t dir_pin, Microstep microstep);
void Axis_MoveSteps(Axis *a, int32_t full_steps);
void Axis_Tick(Axis *a);
void Axis_Stop(Axis *a);
void Axis_EmergencyStop(Axis *a);

#endif /* AXIS_H_ */
