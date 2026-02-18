// ===================== potentiometer.c =====================
// Реализация модуля потенциометра

#include "potentiometer.h"

// -----------------------------------------------------------
// Коэффициент фильтрации
// -----------------------------------------------------------
// POT_FILTER_ALPHA = 8 → фильтр 1/8
// Чем больше число – тем плавнее, но медленнее реакция
// -----------------------------------------------------------
#define POT_FILTER_ALPHA 8

void Potentiometer_Init(Potentiometer *p, ADC_HandleTypeDef *hadc, uint16_t min,
		uint16_t max) {
// Сохраняем параметры
	p->hadc = hadc;
	p->raw = 0;
	p->filtered = 0;
	p->min = min;
	p->max = max;

// Запускаем ADC (один раз)
// Далее он будет опрашиваться через PollForConversion
	HAL_ADC_Start(p->hadc);
}

void Potentiometer_Update(Potentiometer *p) {
// Проверяем, завершилось ли преобразование
	if (HAL_ADC_PollForConversion(p->hadc, 0) == HAL_OK) {
// Считываем «сырое» значение
		p->raw = HAL_ADC_GetValue(p->hadc);

// -------------------------------------------------------
// Экспоненциальный IIR-фильтр
// filtered = filtered * (N-1)/N + raw/N
// -------------------------------------------------------
		p->filtered = (p->filtered * (POT_FILTER_ALPHA - 1) + p->raw)
				/ POT_FILTER_ALPHA;
	}
}

uint16_t Potentiometer_GetValue(Potentiometer *p) {
// Ограничение диапазона (защита от выбросов)
	if (p->filtered < p->min)
		return p->min;

	if (p->filtered > p->max)
		return p->max;

	return p->filtered;
}
