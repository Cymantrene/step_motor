#include "axis.h"
#define STEP_PULSE_US  3U   // 3..5 us — нормально для DM556

static inline void delay_us_dwt(uint32_t us) {
	uint32_t cycles_per_us = SystemCoreClock / 1000000U;
	uint32_t start = DWT->CYCCNT;
	uint32_t cycles = us * cycles_per_us;

	while ((DWT->CYCCNT - start) < cycles) {
		__NOP();
	}
}

// Обновление профиля скорости каждые 5 мс
#define PROFILE_UPDATE_MS      5U
#define PROFILE_UPDATE_TICKS   (MOTION_TICK_HZ * PROFILE_UPDATE_MS / 1000U)

// Быстрый корень для расчета торможения
static uint32_t isqrt_u32(uint32_t n) {
	uint32_t res = 0;
	uint32_t bit = 1U << 30;
	while (bit > n)
		bit >>= 2;
	while (bit != 0) {
		if (n >= res + bit) {
			n -= res + bit;
			res = (res >> 1) + bit;
		} else {
			res >>= 1;
		}
		bit >>= 2;
	}
	return res;
}
/*
// Задержка импульса шага (минимум 2.5 мкс для DM556)
static inline void step_delay(void) {
	for (volatile uint32_t i = 0; i < 250; i++) {
		__NOP();
	}
}
 */
void Axis_Init(Axis *a, GPIO_TypeDef *step_port, uint16_t step_pin,
		GPIO_TypeDef *dir_port, uint16_t dir_pin, Microstep microstep) {
	a->position = 0;
	a->remaining = 0;
	a->speed = AXIS_MIN_SPEED_USTEP_S;
	a->target_speed = AXIS_MIN_SPEED_USTEP_S;
	a->max_speed = AXIS_MAX_SPEED_USTEP_S;

	a->accumulator = 0;
	a->dir_holdoff_ticks = 0;

	a->prof_tick_cnt = 0;
	a->accel_q16 = 0;

	a->step_port = step_port;
	a->step_pin = step_pin;
	a->dir_port = dir_port;
	a->dir_pin = dir_pin;

	a->microstep = microstep;
	a->dir = 0;
	a->active = 0;
}

void Axis_MoveSteps(Axis *a, int32_t full_steps) {
	if (full_steps == 0)
		return;

	if (full_steps > 0) {
		a->dir = 1;
		HAL_GPIO_WritePin(a->dir_port, a->dir_pin, GPIO_PIN_SET);
	} else {
		a->dir = 0;
		HAL_GPIO_WritePin(a->dir_port, a->dir_pin, GPIO_PIN_RESET);
		full_steps = -full_steps;
	}
	a->dir_holdoff_ticks = 1;   // пропускаем 1 тик (50us) перед первым STEP

	a->remaining = full_steps * (int32_t) a->microstep;

	// Начальная скорость с учетом микрошага
	uint32_t s_v = START_SPEED_FULL * (uint32_t) a->microstep;
	if (s_v < AXIS_MIN_SPEED_USTEP_S)
		s_v = AXIS_MIN_SPEED_USTEP_S;

	a->speed = s_v;
	a->target_speed = s_v;
	a->accel_q16 = 0;
	a->accumulator = 0;
	a->prof_tick_cnt = 0;
	a->active = 1;
}

void Axis_Tick(Axis *a) {
	if (!a->active)
		return;

	// --- 1. ПЛАНИРОВЩИК ПРОФИЛЯ (РАСЧЕТ СКОРОСТИ) ---
	a->prof_tick_cnt++;
	if (a->prof_tick_cnt >= PROFILE_UPDATE_TICKS) {
		a->prof_tick_cnt = 0;

		if (a->remaining <= 0) {
			a->active = 0;
			return;
		}

		// Расчет скорости торможения (V = sqrt(2*A*S))
		uint64_t rem = (uint64_t) a->remaining;

		uint64_t vmax = (uint64_t) (MOTION_TICK_HZ - 1U);
		uint64_t vmax2 = vmax * vmax;           // (max speed)^2

		uint64_t expr = 2ULL * (uint64_t) AXIS_ACCEL_MAX_USTEP_S2 * rem;
		if (expr > vmax2)
			expr = vmax2;

		uint32_t stop_speed = isqrt_u32((uint32_t) expr);



		uint32_t limit_v = a->max_speed;
		if (stop_speed < limit_v)
			limit_v = stop_speed;
		if (limit_v < AXIS_MIN_SPEED_USTEP_S)
			limit_v = AXIS_MIN_SPEED_USTEP_S;

		a->target_speed = limit_v;

		// S-Curve расчеты через рывок (Jerk)
		const int32_t DT_Q16 = (int32_t) ((PROFILE_UPDATE_MS * 65536) / 1000);
		const int32_t ACCEL_MAX_Q16 =
				(int32_t) ((uint32_t) AXIS_ACCEL_MAX_USTEP_S2 << 16);
		const int32_t JERK_Q16 = (int32_t) ((uint32_t) AXIS_JERK_USTEP_S3 << 16);

		int32_t speed_err = (int32_t) a->target_speed - (int32_t) a->speed;
		int32_t target_accel_q16 = 0;

		if (speed_err > 0)
			target_accel_q16 = ACCEL_MAX_Q16;
		else if (speed_err < 0)
			target_accel_q16 = -ACCEL_MAX_Q16;

		int32_t jerk_step = (int32_t) (((int64_t) JERK_Q16 * DT_Q16) >> 16);

		// Плавное изменение ускорения
		if (a->accel_q16 < target_accel_q16) {
			a->accel_q16 += jerk_step;
			if (a->accel_q16 > target_accel_q16)
				a->accel_q16 = target_accel_q16;
		} else if (a->accel_q16 > target_accel_q16) {
			a->accel_q16 -= jerk_step;
			if (a->accel_q16 < target_accel_q16)
				a->accel_q16 = target_accel_q16;
		}

		// Применение ускорения к скорости
		int64_t delta_v_q16 = ((int64_t) a->accel_q16 * DT_Q16) >> 16;
		int32_t delta_v = (int32_t) (delta_v_q16 >> 16);

		if (delta_v == 0 && speed_err != 0) {
			delta_v = (speed_err > 0) ? 1 : -1;
		}

		int32_t new_v = (int32_t) a->speed + delta_v;

		// Ограничение по целевой скорости
		if ((speed_err > 0 && new_v > (int32_t) a->target_speed)
				|| (speed_err < 0 && new_v < (int32_t) a->target_speed)) {
			new_v = (int32_t) a->target_speed;
			a->accel_q16 = 0;
		}

		if (new_v < (int32_t) AXIS_MIN_SPEED_USTEP_S)
			new_v = AXIS_MIN_SPEED_USTEP_S;

		// Предел частоты таймера
		if (new_v >= (int32_t) MOTION_TICK_HZ)
			new_v = (int32_t) MOTION_TICK_HZ - 1;

		a->speed = (uint32_t) new_v;
	}
	if (a->dir_holdoff_ticks) {
		a->dir_holdoff_ticks--;
		return;
	}

	// --- 2. ГЕНЕРАЦИЯ ИМПУЛЬСОВ (АККУМУЛЯТОР) ---
	a->accumulator += a->speed;

	if (a->accumulator >= MOTION_TICK_HZ) {
		a->accumulator -= MOTION_TICK_HZ;

		// Физический пин STEP
		// STEP pulse: set -> hold -> reset (BSRR быстрее и без read-modify-write)
		a->step_port->BSRR = a->step_pin;                      // SET
		delay_us_dwt(STEP_PULSE_US);                           // >= 3us
		a->step_port->BSRR = (uint32_t) a->step_pin << 16U;     // RESET


		a->position += (a->dir ? 1 : -1);
		a->remaining--;

		if (a->remaining <= 0) {
			a->active = 0;
			a->remaining = 0;
			a->accel_q16 = 0;
			a->speed = AXIS_MIN_SPEED_USTEP_S;
			a->step_port->BSRR = (uint32_t) a->step_pin << 16U; // STEP=0 на всякий
		}

	}
}

void Axis_Stop(Axis *a) {
	// Плавное торможение: обнуляем путь до дистанции останова
	a->target_speed = AXIS_MIN_SPEED_USTEP_S;
	a->remaining = (int32_t) ((a->speed * a->speed)
			/ (2 * AXIS_ACCEL_MAX_USTEP_S2));
}

void Axis_EmergencyStop(Axis *a) {
	a->active = 0;
	a->remaining = 0;
	a->speed = 0;
	a->accumulator = 0;
	a->accel_q16 = 0;
	a->prof_tick_cnt = 0;
	HAL_GPIO_WritePin(a->step_port, a->step_pin, GPIO_PIN_RESET);
}

