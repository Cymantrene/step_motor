#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "axis.h"
#include "buttons.h"
#include "driver.h"

/*
 * 2 страницы:
 *  PAGE_MOVE     — ввод шагов и старт/стоп
 *  PAGE_SETTINGS — микрошаг, связка моторов, enable/disable
 */

typedef enum {
	PAGE_MOVE = 0, PAGE_SETTINGS = 1, PAGE_STATUS = 2
} MenuPage;



typedef struct {
	Axis *a1;
	Axis *a2;
	Driver *d1;
	Driver *d2;
	MenuPage page;
	uint8_t selected_axis;   // 0=M1, 1=M2
	int32_t steps_full;      // full steps (+/-)
	/* NEW: запрос старта (one-shot) */
	uint8_t start_request;// 1 = меню попросило старт (main выполнит и сбросит)
	uint8_t start_armed;       // 0=не готов, 1=ждём подтверждение OK
	uint32_t start_armed_ms;    // когда нажали OK первый раз
	uint8_t done_show;      // 1 = показываем DONE
	uint32_t done_show_ms;   // когда начали показывать
	uint8_t prev_busy_any;  // предыдущее состояние "хоть одна ось занята"
	/* Settings */
	Microstep microstep;      // 1..64
	uint8_t link_motors;      // 0=отдельно, 1=вместе
	uint8_t drivers_enabled;  // 0=disabled, 1=enabled
	uint32_t last_draw_ms;

} Menu;

void Menu_Init(Menu *m, Axis *axis1, Axis *axis2, Driver *drv1, Driver *drv2);
void Menu_Update(Menu *m, Buttons *bs);



#endif
