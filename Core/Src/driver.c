// ===================== driver.c =====================
#include "driver.h"
#include "main.h"   // нужен для M1_MS1_GPIO_Port / M1_MS1_Pin и т.д.

/* Вспомогательная функция: выставить 3 пина микрошагов */
static void set_ms_pins(GPIO_TypeDef *ms1_port, uint16_t ms1_pin,
		GPIO_TypeDef *ms2_port, uint16_t ms2_pin, GPIO_TypeDef *ms3_port,
		uint16_t ms3_pin, GPIO_PinState s1, GPIO_PinState s2, GPIO_PinState s3) {
	HAL_GPIO_WritePin(ms1_port, ms1_pin, s1);
	HAL_GPIO_WritePin(ms2_port, ms2_pin, s2);
	HAL_GPIO_WritePin(ms3_port, ms3_pin, s3);
}

/*
 * Таблица микрошагов -> состояния MS1/MS2/MS3
 * ВНИМАНИЕ: комбинации зависят от конкретного драйвера!
 * Здесь поставлена типичная логика, где 1/64 = 1,1,1.
 * Если потом выяснится другая таблица — меняем только этот switch.
 */
static void microstep_to_pins(DriverMicrostepMode mode, GPIO_PinState *ms1,
		GPIO_PinState *ms2, GPIO_PinState *ms3) {
	switch (mode) {
	case DRV_MS_1:
		*ms1 = GPIO_PIN_RESET;
		*ms2 = GPIO_PIN_RESET;
		*ms3 = GPIO_PIN_RESET;
		break;

	case DRV_MS_2:
		*ms1 = GPIO_PIN_SET;
		*ms2 = GPIO_PIN_RESET;
		*ms3 = GPIO_PIN_RESET;
		break;

	case DRV_MS_4:
		*ms1 = GPIO_PIN_RESET;
		*ms2 = GPIO_PIN_SET;
		*ms3 = GPIO_PIN_RESET;
		break;

	case DRV_MS_8:
		*ms1 = GPIO_PIN_SET;
		*ms2 = GPIO_PIN_SET;
		*ms3 = GPIO_PIN_RESET;
		break;

	case DRV_MS_16:
		*ms1 = GPIO_PIN_RESET;
		*ms2 = GPIO_PIN_RESET;
		*ms3 = GPIO_PIN_SET;
		break;

	case DRV_MS_32:
		*ms1 = GPIO_PIN_SET;
		*ms2 = GPIO_PIN_RESET;
		*ms3 = GPIO_PIN_SET;
		break;

	case DRV_MS_64:
		*ms1 = GPIO_PIN_SET;
		*ms2 = GPIO_PIN_SET;
		*ms3 = GPIO_PIN_SET;
		break;

	default:
		// безопасное значение по умолчанию
		*ms1 = GPIO_PIN_RESET;
		*ms2 = GPIO_PIN_RESET;
		*ms3 = GPIO_PIN_RESET;
		break;
	}
}

/* Установить микрошаг для мотора 1 */
void Driver_SetMicrostep1(DriverMicrostepMode mode) {
	(void) mode;
#if defined(M1_MS1_GPIO_Port) && defined(M1_MS1_Pin) && \
    defined(M1_MS2_GPIO_Port) && defined(M1_MS2_Pin) && \
    defined(M1_MS3_GPIO_Port) && defined(M1_MS3_Pin)

    GPIO_PinState s1, s2, s3;
    microstep_to_pins(mode, &s1, &s2, &s3);
    set_ms_pins(M1_MS1_GPIO_Port, M1_MS1_Pin,
                M1_MS2_GPIO_Port, M1_MS2_Pin,
                M1_MS3_GPIO_Port, M1_MS3_Pin,
                s1, s2, s3);
#endif
}

void Driver_SetMicrostep2(DriverMicrostepMode mode) {
	(void) mode;
#if defined(M2_MS1_GPIO_Port) && defined(M2_MS1_Pin) && \
    defined(M2_MS2_GPIO_Port) && defined(M2_MS2_Pin) && \
    defined(M2_MS3_GPIO_Port) && defined(M2_MS3_Pin)

    GPIO_PinState s1, s2, s3;
    microstep_to_pins(mode, &s1, &s2, &s3);
    set_ms_pins(M2_MS1_GPIO_Port, M2_MS1_Pin,
                M2_MS2_GPIO_Port, M2_MS2_Pin,
                M2_MS3_GPIO_Port, M2_MS3_Pin,
                s1, s2, s3);
#endif
}


void Driver_Init(Driver *d, GPIO_TypeDef *enable_port, uint16_t enable_pin,
		uint8_t enable_active_level) {
	d->enable_port = enable_port;
	d->enable_pin = enable_pin;
	d->enable_active_level = enable_active_level;

	d->state = DRIVER_DISABLED;

// по умолчанию драйвер выключен
	Driver_Disable(d);
}

void Driver_Enable(Driver *d) {
	if (d->enable_active_level)
		HAL_GPIO_WritePin(d->enable_port, d->enable_pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(d->enable_port, d->enable_pin, GPIO_PIN_RESET);

	d->state = DRIVER_ENABLED;
}

void Driver_Disable(Driver *d) {
	if (d->enable_active_level)
		HAL_GPIO_WritePin(d->enable_port, d->enable_pin, GPIO_PIN_RESET);
	else
		HAL_GPIO_WritePin(d->enable_port, d->enable_pin, GPIO_PIN_SET);

	d->state = DRIVER_DISABLED;
}

void Driver_EmergencyStop(Driver *d) {
// мгновенное отключение драйвера
	Driver_Disable(d);
}

#include "main.h"

void Driver_DisableAll(void) {
	/*
	 * ВНИМАНИЕ:
	 * Здесь предполагаем, что "выключить" = GPIO_PIN_SET.
	 * Если твой драйвер включается уровнем HIGH — поменяем на RESET.
	 */
	HAL_GPIO_WritePin(EN_M1_GPIO_Port, EN_M1_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(EN_M2_GPIO_Port, EN_M2_Pin, GPIO_PIN_SET);
}
