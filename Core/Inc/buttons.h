#ifndef BUTTONS_H
#define BUTTONS_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

/*
 * Простой драйвер кнопок:
 * - антидребезг
 * - событие "нажатие"
 * - событие "повтор" при удержании (ускоряет изменение числа шагов)
 */

typedef struct {
	GPIO_TypeDef *port;
	uint16_t pin;

	uint8_t stable_level;     // стабильный уровень (0/1)
	uint8_t last_raw;         // последний сырой уровень
	uint32_t last_change_ms;  // когда изменился сырой уровень

	uint8_t pressed_event;    // 1 = было нажатие (один раз)
	uint8_t repeat_event;     // 1 = событие повтора (при удержании)

	uint32_t press_start_ms;  // время начала удержания
	uint32_t last_repeat_ms;  // время последнего repeat
} Button;

typedef struct {
	Button up;
	Button down;
	Button ok;
	Button back;
} Buttons;

/* Настройка одной кнопки */
void Button_Init(Button *b, GPIO_TypeDef *port, uint16_t pin);

/* Обновление состояния кнопки (вызов в while(1) часто) */
void Button_Update(Button *b, uint32_t now_ms);

/* Снять событие "pressed" (одноразовое) */
uint8_t Button_GetPressed(Button *b);

/* Снять событие "repeat" (многоразовое при удержании) */
uint8_t Button_GetRepeat(Button *b);

/* Инициализация всех 4 кнопок */
void Buttons_Init(Buttons *bs);

/* Обновление всех 4 кнопок */
void Buttons_Update(Buttons *bs);

#endif
