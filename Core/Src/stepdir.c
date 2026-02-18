// ===================== stepdir.c =====================
#include "stepdir.h"
#define TIM6_TICKS_PER_SEC  20000UL

#define STEP_FREQ_MIN     50U
#define STEP_FREQ_MAX   5000U

// ===== S-curve tuning =====
// Обновление скорости вызывается раз в 5 мс (из main.c)
#define S_UPDATE_MS          5U

// Максимальное ускорение (Hz/s)
#define S_ACCEL_MAX_HZ_S     4000

// Максимальный jerk (Hz/s^2) - должен быть <= 32767 для int32 Q16.16
#define S_JERK_HZ_S2         20000

#define ACCEL_STEP_HZ     50U

#define SPEED_UPDATE_MS      5U
#define SPEED_UPDATE_DT_Q16  ((int32_t)((SPEED_UPDATE_MS * 65536) / 1000)) // dt в Q16.16

void StepDir_Init(StepDir *sd, Axis *axis, GPIO_TypeDef *dir_port,
		uint16_t dir_pin) {
	sd->axis = axis;
	sd->dir_port = dir_port;
	sd->dir_pin = dir_pin;
	sd->pending_steps = 0;

	sd->current_freq = STEP_FREQ_MIN;
	sd->target_freq = STEP_FREQ_MIN;
	sd->step_interval_ticks = TIM6_TICKS_PER_SEC / STEP_FREQ_MIN;


	// jerk задаём один раз (можно потом сделать параметром)
	// пример: 200000 Hz/s^2  (подбирается)
	// jerk_q16 = jerk * 65536
	sd->accel_q16 = 0;
	sd->jerk_q16 = (int32_t) (S_JERK_HZ_S2 << 16);
}

void StepDir_UpdateSpeed(StepDir *sd, uint16_t pot_value) {
	/* 1. Потенциометр -> целевая частота */
	sd->target_freq = STEP_FREQ_MIN
			+ ((uint32_t) pot_value * (STEP_FREQ_MAX - STEP_FREQ_MIN)) / 4095;

	/* 2. Плавное ускорение / торможение */
	if (sd->current_freq < sd->target_freq) {
		sd->current_freq += ACCEL_STEP_HZ;
		if (sd->current_freq > sd->target_freq)
			sd->current_freq = sd->target_freq;
	} else if (sd->current_freq > sd->target_freq) {
		if (sd->current_freq > ACCEL_STEP_HZ)
			sd->current_freq -= ACCEL_STEP_HZ;
		else
			sd->current_freq = STEP_FREQ_MIN;
	}

	/* 3. Гарантия допустимого диапазона */
	if (sd->current_freq < STEP_FREQ_MIN)
		sd->current_freq = STEP_FREQ_MIN;

	if (sd->current_freq > STEP_FREQ_MAX)
		sd->current_freq = STEP_FREQ_MAX;

	/* 4. Пересчёт интервала STEP */
	sd->step_interval_ticks = TIM6_TICKS_PER_SEC / sd->current_freq;
}

static uint32_t MapPotToFreq(uint16_t pot) {
	uint32_t f = STEP_FREQ_MIN
			+ ((uint32_t) pot * (STEP_FREQ_MAX - STEP_FREQ_MIN)) / 4095;

	if (f < STEP_FREQ_MIN)
		f = STEP_FREQ_MIN;
	if (f > STEP_FREQ_MAX)
		f = STEP_FREQ_MAX;
	return f;
}

void StepDir_UpdateSpeedScurve(StepDir *sd, uint16_t pot_value) {
	// dt = 5 мс в Q16.16: 0.005 * 65536 ≈ 327
	// Мы фиксируем dt, потому что вызываем строго раз в 5 мс


	// Максимальное ускорение (Hz/s) в Q16.16
	// Подстройка: больше -> резче разгон
	const int32_t DT_Q16 = (int32_t) ((S_UPDATE_MS * 65536) / 1000);
	const int32_t ACCEL_MAX_Q16 = (int32_t) (S_ACCEL_MAX_HZ_S << 16);


	// 1) обновляем target
	sd->target_freq = MapPotToFreq(pot_value);

	// 2) ошибка скорости
	int32_t err = (int32_t) sd->target_freq - (int32_t) sd->current_freq;

	if (err == 0) {
		// уже на цели — гасим ускорение
		sd->accel_q16 = 0;
	} else {
		// 3) хотим ускорение в сторону цели
		int32_t accel_target_q16 = (err > 0) ? ACCEL_MAX_Q16 : -ACCEL_MAX_Q16;

		// 4) ограничиваем изменение accel (jerk)
		// jerk_step = jerk * dt
		int32_t jerk_step_q16 = (int32_t) (((int64_t) sd->jerk_q16 * DT_Q16)
				>> 16);

		if (sd->accel_q16 < accel_target_q16) {
			sd->accel_q16 += jerk_step_q16;
			if (sd->accel_q16 > accel_target_q16)
				sd->accel_q16 = accel_target_q16;
		} else if (sd->accel_q16 > accel_target_q16) {
			sd->accel_q16 -= jerk_step_q16;
			if (sd->accel_q16 < accel_target_q16)
				sd->accel_q16 = accel_target_q16;
		}

		// 5) интегрируем скорость: f += accel * dt
		// delta_f_q16 = accel_q16 * dt_q16
		int64_t delta_f_q16 = ((int64_t) sd->accel_q16 * DT_Q16) >> 16;

		// Переводим в целые Hz
		int32_t delta_f = (int32_t) (delta_f_q16 >> 16);

		// Если delta_f = 0 (малые значения), делаем минимальный шаг 1 Hz
		if (delta_f == 0)
			delta_f = (err > 0) ? 1 : -1;

		int32_t new_f = (int32_t) sd->current_freq + delta_f;

		// 6) не перелетаем через цель
		if ((err > 0 && new_f > (int32_t) sd->target_freq)
				|| (err < 0 && new_f < (int32_t) sd->target_freq)) {
			new_f = (int32_t) sd->target_freq;
			sd->accel_q16 = 0;
		}

		// 7) защита диапазона
		if (new_f < (int32_t) STEP_FREQ_MIN)
			new_f = STEP_FREQ_MIN;
		if (new_f > (int32_t) STEP_FREQ_MAX)
			new_f = STEP_FREQ_MAX;

		sd->current_freq = (uint32_t) new_f;
	}

	// 8) пересчёт интервала (защита от 0)
	if (sd->current_freq < 1)
		sd->current_freq = 1;

	uint32_t interval = TIM6_TICKS_PER_SEC / sd->current_freq;
	if (interval < 1)
		interval = 1;  // интервал не может быть 0

	sd->step_interval_ticks = interval;
}

void StepDir_UpdateTargetFreqScurve(StepDir *sd, uint32_t target_freq) {
	const int32_t DT_Q16 = (int32_t) ((S_UPDATE_MS * 65536) / 1000);
	const int32_t ACCEL_MAX_Q16 = (int32_t) (S_ACCEL_MAX_HZ_S << 16);

	// ограничиваем target_freq в допустимый диапазон
	if (target_freq < STEP_FREQ_MIN)
		target_freq = STEP_FREQ_MIN;
	if (target_freq > STEP_FREQ_MAX)
		target_freq = STEP_FREQ_MAX;

	sd->target_freq = target_freq;

	int32_t err = (int32_t) sd->target_freq - (int32_t) sd->current_freq;

	if (err == 0) {
		sd->accel_q16 = 0;
	} else {
		int32_t accel_target_q16 = (err > 0) ? ACCEL_MAX_Q16 : -ACCEL_MAX_Q16;

		int32_t jerk_step_q16 = (int32_t) (((int64_t) sd->jerk_q16 * DT_Q16)
				>> 16);

		if (sd->accel_q16 < accel_target_q16) {
			sd->accel_q16 += jerk_step_q16;
			if (sd->accel_q16 > accel_target_q16)
				sd->accel_q16 = accel_target_q16;
		} else if (sd->accel_q16 > accel_target_q16) {
			sd->accel_q16 -= jerk_step_q16;
			if (sd->accel_q16 < accel_target_q16)
				sd->accel_q16 = accel_target_q16;
		}

		int64_t delta_f_q16 = ((int64_t) sd->accel_q16 * DT_Q16) >> 16;
		int32_t delta_f = (int32_t) (delta_f_q16 >> 16);

		if (delta_f == 0)
			delta_f = (err > 0) ? 1 : -1;

		int32_t new_f = (int32_t) sd->current_freq + delta_f;

		if ((err > 0 && new_f > (int32_t) sd->target_freq)
				|| (err < 0 && new_f < (int32_t) sd->target_freq)) {
			new_f = (int32_t) sd->target_freq;
			sd->accel_q16 = 0;
		}

		if (new_f < (int32_t) STEP_FREQ_MIN)
			new_f = STEP_FREQ_MIN;
		if (new_f > (int32_t) STEP_FREQ_MAX)
			new_f = STEP_FREQ_MAX;

		sd->current_freq = (uint32_t) new_f;
	}

	if (sd->current_freq < 1)
		sd->current_freq = 1;

	uint32_t interval = TIM6_TICKS_PER_SEC / sd->current_freq;
	if (interval < 1)
		interval = 1;

	sd->step_interval_ticks = interval;
}


void StepDir_OnStepIRQ(StepDir *sd) {
	GPIO_PinState dir = HAL_GPIO_ReadPin(sd->dir_port, sd->dir_pin);

	if (dir == GPIO_PIN_SET)
		sd->pending_steps++;
	else
		sd->pending_steps--;
}

void StepDir_Process(StepDir *sd) {
	if (sd->pending_steps != 0) {
		int32_t steps = sd->pending_steps;
		sd->pending_steps = 0;

		Axis_MoveSteps(sd->axis, steps);
	}
}

// -------------------------------------------------
// ЛОКАЛЬНАЯ функция преобразования потенциометра
// -------------------------------------------------
// static → видна ТОЛЬКО внутри stepdir.c
// main.c и другие файлы о ней не знают
static uint32_t MapPotToStepFreq(uint16_t pot) {
	return STEP_FREQ_MIN
			+ ((uint32_t) pot * (STEP_FREQ_MAX - STEP_FREQ_MIN)) / 4095;
}

// публичная функция
void StepDir_SetSpeed(StepDir *sd, uint16_t pot_value) {
	uint32_t freq = MapPotToStepFreq(pot_value);

	sd->step_interval_ticks = TIM6_TICKS_PER_SEC / freq;
}
