#include "menu.h"
#include "ui.h"
#include "estop.h"
#include "driver.h"
#include "stm32g4xx_hal.h"

/* Ввод шагов */
#define STEP_SMALL   100
#define STEP_FAST   1000

/* Обновление дисплея */
#define UI_REFRESH_MS 100U
#define START_CONFIRM_MS 2000U   // 2 секунды на второе нажатие OK
/* DM556: микрошаг задаётся DIP, MS-пинов нет */
#define DRIVER_HAS_MS_PINS 0

/* ===== helpers ===== */

static Axis* sel_axis(Menu *m) {
	return (m->selected_axis == 0) ? m->a1 : m->a2;
}

static void clamp_steps(Menu *m) {
	if (m->steps_full > 200000)
		m->steps_full = 200000;
	if (m->steps_full < -200000)
		m->steps_full = -200000;
	//if (m->steps_full == 0)
	//	m->steps_full = 100;
}
#if DRIVER_HAS_MS_PINS
static Microstep next_microstep(Microstep ms) {
	switch (ms) {
	case MICROSTEP_1:
		return MICROSTEP_2;
	case MICROSTEP_2:
		return MICROSTEP_4;
	case MICROSTEP_4:
		return MICROSTEP_8;
	case MICROSTEP_8:
		return MICROSTEP_16;
	case MICROSTEP_16:
		return MICROSTEP_32;
	case MICROSTEP_32:
		return MICROSTEP_64;
	default:
		return MICROSTEP_1;
	}
}

static Microstep prev_microstep(Microstep ms) {
	switch (ms) {
	case MICROSTEP_64:
		return MICROSTEP_32;
	case MICROSTEP_32:
		return MICROSTEP_16;
	case MICROSTEP_16:
		return MICROSTEP_8;
	case MICROSTEP_8:
		return MICROSTEP_4;
	case MICROSTEP_4:
		return MICROSTEP_2;
	case MICROSTEP_2:
		return MICROSTEP_1;
	default:
		return MICROSTEP_64;
	}
}
#endif

#if DRIVER_HAS_MS_PINS
#include "driver.h"

/* Конвертация Microstep (1..64) в DriverMicrostepMode */
static DriverMicrostepMode Microstep_ToDriverMode(Microstep ms) {
    switch (ms) {
    case MICROSTEP_1:  return DRV_MS_1;
    case MICROSTEP_2:  return DRV_MS_2;
    case MICROSTEP_4:  return DRV_MS_4;
    case MICROSTEP_8:  return DRV_MS_8;
    case MICROSTEP_16: return DRV_MS_16;
    case MICROSTEP_32: return DRV_MS_32;
    case MICROSTEP_64: return DRV_MS_64;
    default:           return DRV_MS_64;
    }
}
#endif


/* Применить микрошаг к Axis и к пинам драйверов */
static void apply_microstep(Menu *m) {
	/* ВАЖНО:
	 Для DM556 микрошаг задаётся DIP-переключателями на драйвере.
	 Этот параметр в меню должен СОВПАДАТЬ с DIP, чтобы расчёты шагов были верные.
	 */
	m->a1->microstep = m->microstep;
	m->a2->microstep = m->microstep;

#if DRIVER_HAS_MS_PINS
	DriverMicrostepMode mode = Microstep_ToDriverMode(m->microstep);
	Driver_SetMicrostep1(mode);
	Driver_SetMicrostep2(mode);
#endif
}

/* Enable/Disable обоих драйверов */
static void apply_enable(Menu *m) {
	if (m->drivers_enabled) {
		Driver_Enable(m->d1);
		Driver_Enable(m->d2);
	} else {
		Axis_Stop(m->a1);
		Axis_Stop(m->a2);
		Driver_Disable(m->d1);
		Driver_Disable(m->d2);
	}
}

void Menu_Init(Menu *m, Axis *axis1, Axis *axis2, Driver *drv1, Driver *drv2) {
	m->a1 = axis1;
	m->a2 = axis2;
	m->d1 = drv1;
	m->d2 = drv2;

	m->page = PAGE_MOVE;
	m->selected_axis = 0;
	m->steps_full = 1000;
	m->start_request = 0;

	/* === подтверждение старта === */
	m->start_armed = 0;
	m->start_armed_ms = 0;
	m->done_show = 0;
	m->done_show_ms = 0;
	m->prev_busy_any = 0;

	/* === настройки === */
	m->microstep = MICROSTEP_64;
	m->link_motors = 0;
	m->drivers_enabled = 1;

	m->last_draw_ms = 0;

	apply_microstep(m);
	apply_enable(m);
}


static void handle_move_page(Menu *m, Buttons *bs) {
	Axis *sel = sel_axis(m);

	/* UP/DOWN: шаги */
	if (Button_GetPressed(&bs->up))
		m->steps_full += STEP_SMALL;
	if (Button_GetPressed(&bs->down))
		m->steps_full -= STEP_SMALL;
	if (Button_GetRepeat(&bs->up))
		m->steps_full += STEP_FAST;
	if (Button_GetRepeat(&bs->down))
		m->steps_full -= STEP_FAST;
	clamp_steps(m);
	/* Если меняли шаги — сбрасываем подтверждение старта */
	if (Button_GetPressed(&bs->up) || Button_GetPressed(&bs->down)
			|| Button_GetRepeat(&bs->up) || Button_GetRepeat(&bs->down)) {
		m->start_armed = 0;
	}

	/* BACK: если едем — стоп; если стоим — смена оси */
	if (Button_GetPressed(&bs->back)) {
		if (sel->active) {
			Axis_Stop(sel);
			if (m->link_motors) {
				Axis_Stop(m->a1);
				Axis_Stop(m->a2);
			}
		} else {
			m->selected_axis ^= 1;   // переключить M1/M2
		}

		/* BACK тоже сбрасывает подтверждение */
		m->start_armed = 0;
	}


	/* OK: подтверждённый старт (двойное нажатие) */
	if (Button_GetPressed(&bs->ok)) {
		/* Если уже едем — сбрасываем подтверждение */
		if (sel->active
				|| (m->link_motors && (m->a1->active || m->a2->active))) {
			m->start_armed = 0;
		} else {
			uint32_t now = HAL_GetTick();

			/* 1-е нажатие: “вооружаем” старт */
			if (!m->start_armed) {
				m->start_armed = 1;
				m->start_armed_ms = now;
			} else {
				/* 2-е нажатие: если успели за таймаут — стартуем */
				if ((now - m->start_armed_ms) <= START_CONFIRM_MS) {
					/* подтверждено: просим main.c запустить движение (one-shot) */
					m->start_request = 1;
				}


				/* в любом случае снимаем “armed” */
				m->start_armed = 0;
			}
		}
	}

	/* Таймаут подтверждения: если 2 сек прошло — сброс */
	if (m->start_armed) {
		if ((HAL_GetTick() - m->start_armed_ms) > START_CONFIRM_MS)
			m->start_armed = 0;
	}


	/* OK: старт */


	/* Удержание OK (repeat) — перейти в Settings (просто и удобно) */
	if (Button_GetRepeat(&bs->ok)) {
		m->page = PAGE_SETTINGS;
	}
}
static void handle_status_page(Menu *m, Buttons *bs);
static void handle_settings_page(Menu *m, Buttons *bs) {
	/* UP/DOWN: меняем microstep */
	/* microstep меняем только когда обе оси НЕ едут */
	uint8_t any_busy = (m->a1->active || m->a2->active);

#if DRIVER_HAS_MS_PINS
if (!any_busy) {
    if (Button_GetPressed(&bs->up)) {
        m->microstep = next_microstep(m->microstep);
        apply_microstep(m);
    }
    if (Button_GetPressed(&bs->down)) {
        m->microstep = prev_microstep(m->microstep);
        apply_microstep(m);
    }
}
#else
	(void) any_busy; // чтобы не ругался компилятор, если any_busy больше нигде не нужен
#endif


	/* OK: переключить link_motors */
	if (Button_GetPressed(&bs->ok)) {
		m->link_motors ^= 1;
	}

	/* BACK: переключить enable/disable (как “power”) */
	if (Button_GetPressed(&bs->back)) {
		m->drivers_enabled ^= 1;
		apply_enable(m);
	}

	/* Удержание BACK (repeat) — вернуться на Move */
	if (Button_GetRepeat(&bs->back)) {
		m->page = PAGE_MOVE;
	}
	/* Удержание OK — перейти на STATUS */
	if (Button_GetRepeat(&bs->ok)) {
		m->page = PAGE_STATUS;
	}

}

void Menu_Update(Menu *m, Buttons *bs) {
	/* Если активен E-STOP — меню не запускает движение, только отображение */
	if (!Estop_IsActive()) {
		if (m->page == PAGE_MOVE)
			handle_move_page(m, bs);
		else if (m->page == PAGE_SETTINGS)
			handle_settings_page(m, bs);
		else
			handle_status_page(m, bs);

	}

	/* ===== DONE логика ===== */
	uint8_t busy_any = (uint8_t) (m->a1->active || m->a2->active);

	if (m->prev_busy_any && !busy_any) {
		m->done_show = 1;
		m->done_show_ms = HAL_GetTick();
	}
	m->prev_busy_any = busy_any;

	if (m->done_show) {
		if ((HAL_GetTick() - m->done_show_ms) >= 2000U)
			m->done_show = 0;
	}

	/* ===== ПОДГОТОВКА ДАННЫХ ДЛЯ UI ===== */
	Axis *sel = (m->selected_axis == 0) ? m->a1 : m->a2;
	int32_t pos_sel;
	int32_t rem_sel;

	__disable_irq();
	pos_sel = sel->position;
	rem_sel = sel->remaining;
	__enable_irq();


	/* ===== РИСУЕМ UI ===== */
	uint32_t now = HAL_GetTick();
	if ((now - m->last_draw_ms) >= UI_REFRESH_MS) {
		m->last_draw_ms = now;

		UI_DrawMenu2(m->page, m->selected_axis, m->steps_full,
				(uint8_t) m->microstep, m->link_motors, m->drivers_enabled,
				m->a1->active, m->a2->active, m->a1->speed, m->a2->speed,
				pos_sel, rem_sel, (uint8_t) Estop_IsActive(), m->start_armed,
				m->done_show);
	}
}

static void handle_status_page(Menu *m, Buttons *bs) {
	/* На STATUS ничего не меняем, только навигация */

	/* BACK коротко: смена выбранной оси (как в MOVE когда IDLE) */
	if (Button_GetPressed(&bs->back)) {
		m->selected_axis ^= 1;
	}

	/* Удержание BACK: вернуться на MOVE */
	if (Button_GetRepeat(&bs->back)) {
		m->page = PAGE_MOVE;
	}
	if (Button_GetRepeat(&bs->ok)) {
		m->page = PAGE_SETTINGS;
	}


}
