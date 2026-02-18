#include "buttons.h"
#include "main.h" // для BTN_*_GPIO_Port / BTN_*_Pin

/* Настройки антидребезга/повтора */
#define DEBOUNCE_MS      30U   // дребезг
#define HOLD_START_MS   400U   // через сколько удержание считается "hold"
#define REPEAT_MS        80U   // период повтора при удержании

void Button_Init(Button *b, GPIO_TypeDef *port, uint16_t pin) {
	b->port = port;
	b->pin = pin;

	uint8_t raw = (uint8_t) HAL_GPIO_ReadPin(port, pin);
	b->stable_level = raw;
	b->last_raw = raw;

	b->last_change_ms = HAL_GetTick();
	b->pressed_event = 0;
	b->repeat_event = 0;

	b->press_start_ms = 0;
	b->last_repeat_ms = 0;
}

static void Button_ClearEvents(Button *b) {
	b->pressed_event = 0;
	b->repeat_event = 0;
}

void Button_Update(Button *b, uint32_t now_ms) {
	Button_ClearEvents(b);

	uint8_t raw = (uint8_t) HAL_GPIO_ReadPin(b->port, b->pin);

	/* Отследить изменение сырого уровня */
	if (raw != b->last_raw) {
		b->last_raw = raw;
		b->last_change_ms = now_ms;
	}

	/* Если уровень стабилен достаточно долго — принимаем его */
	if ((now_ms - b->last_change_ms) >= DEBOUNCE_MS) {
		if (b->stable_level != raw) {
			b->stable_level = raw;

			/*
			 * Мы используем Pull-up, значит:
			 * - не нажата: 1
			 * - нажата:    0
			 * Событие "нажатие" удобно ловить по переходу 1 -> 0
			 */
			if (b->stable_level == 0) {
				b->pressed_event = 1;
				b->press_start_ms = now_ms;
				b->last_repeat_ms = now_ms;
			} else {
				/* отпущена */
				b->press_start_ms = 0;
			}
		}
	}

	/* Повтор при удержании */
	if (b->stable_level == 0 && b->press_start_ms != 0) {
		if ((now_ms - b->press_start_ms) >= HOLD_START_MS) {
			if ((now_ms - b->last_repeat_ms) >= REPEAT_MS) {
				b->repeat_event = 1;
				b->last_repeat_ms = now_ms;
			}
		}
	}
}

uint8_t Button_GetPressed(Button *b) {
	uint8_t v = b->pressed_event;
	b->pressed_event = 0;
	return v;
}

uint8_t Button_GetRepeat(Button *b) {
	uint8_t v = b->repeat_event;
	b->repeat_event = 0;
	return v;
}

void Buttons_Init(Buttons *bs) {
	Button_Init(&bs->up, BTN_UP_GPIO_Port, BTN_UP_Pin);
	Button_Init(&bs->down, BTN_DOWN_GPIO_Port, BTN_DOWN_Pin);
	Button_Init(&bs->ok, BTN_OK_GPIO_Port, BTN_OK_Pin);
	Button_Init(&bs->back, BTN_BACK_GPIO_Port, BTN_BACK_Pin);
}

void Buttons_Update(Buttons *bs) {
	uint32_t now = HAL_GetTick();
	Button_Update(&bs->up, now);
	Button_Update(&bs->down, now);
	Button_Update(&bs->ok, now);
	Button_Update(&bs->back, now);
}
