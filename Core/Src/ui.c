#include "ui.h"
#include "oled_ssd1306.h"
#include <stdio.h>

static OledSSD1306 oled;
static uint8_t ui_inited = 0;

void UI_Init(I2C_HandleTypeDef *hi2c) {
	// Можно вызывать один раз после MX_I2C1_Init()
	OLED_Init(&oled, hi2c);
	OLED_Clear(&oled);
	OLED_SetCursor(0, 0);
	OLED_Print(&oled, "UI READY");
	OLED_Update(&oled);

	ui_inited = 1;
}

void UI_Update(uint16_t pot_value, uint32_t freq1, uint32_t freq2,
		uint8_t safety_state) {
	if (!ui_inited)
		return;

	char line[24];

	OLED_Clear(&oled);

	OLED_SetCursor(0, 0);
	snprintf(line, sizeof(line), "POT:%4u", (unsigned) pot_value);
	OLED_Print(&oled, line);

	OLED_SetCursor(0, 1);
	snprintf(line, sizeof(line), "F1:%4lu", (unsigned long) freq1);
	OLED_Print(&oled, line);

	OLED_SetCursor(0, 2);
	snprintf(line, sizeof(line), "F2:%4lu", (unsigned long) freq2);
	OLED_Print(&oled, line);

	OLED_SetCursor(0, 3);
	snprintf(line, sizeof(line), "SAFE:%u", (unsigned) safety_state);
	OLED_Print(&oled, line);

	OLED_Update(&oled);
}

void UI_DrawMenu(uint8_t selected_axis, int32_t steps_full, uint8_t busy1,
		uint8_t busy2, uint32_t spd1, uint32_t spd2) {
	if (!ui_inited)
		return;

	char line[24];

	OLED_Clear(&oled);

	// Строка 0: выбранная ось
	OLED_SetCursor(0, 0);
	snprintf(line, sizeof(line), "AXIS: M%u", (unsigned) (selected_axis + 1));
	OLED_Print(&oled, line);

	// Строка 1: заданные шаги
	OLED_SetCursor(0, 1);
	snprintf(line, sizeof(line), "Steps: %ld", (long) steps_full);
	OLED_Print(&oled, line);

	// Строка 2: статус/скорость M1
	OLED_SetCursor(0, 2);
	snprintf(line, sizeof(line), "M1:%lu %s", (unsigned long) spd1,
			busy1 ? "RUN" : "IDLE");
	OLED_Print(&oled, line);

	// Строка 3: статус/скорость M2
	OLED_SetCursor(0, 3);
	snprintf(line, sizeof(line), "M2:%lu %s", (unsigned long) spd2,
			busy2 ? "RUN" : "IDLE");
	OLED_Print(&oled, line);

	OLED_Update(&oled);
}

void UI_DrawMenu2(uint8_t page, uint8_t selected_axis, int32_t steps_full,
		uint8_t microstep, uint8_t link_motors, uint8_t drivers_enabled,
		uint8_t busy1, uint8_t busy2, uint32_t spd1, uint32_t spd2,
		int32_t pos_sel, int32_t rem_sel,
		uint8_t estop_active, uint8_t start_armed, uint8_t done_show) {
	if (!ui_inited)
		return;

	char line[24];

	OLED_Clear(&oled);

	if (page == 0) /* PAGE_MOVE */
	{
		const char *tag = "     ";
		if (done_show)
			tag = "DONE ";
		else if (start_armed)
			tag = "READY";

		OLED_SetCursor(0, 0);
		snprintf(line, sizeof(line), "MOVE M%u %s",
				(unsigned) (selected_axis + 1), tag);
		OLED_Print(&oled, line);

		OLED_SetCursor(0, 1);
		snprintf(line, sizeof(line), "Steps:%ld", (long) steps_full);
		OLED_Print(&oled, line);

		OLED_SetCursor(0, 2);
		snprintf(line, sizeof(line), "M1:%lu %s", (unsigned long) spd1,
				busy1 ? "RUN" : "IDLE");
		OLED_Print(&oled, line);

		OLED_SetCursor(0, 3);
		snprintf(line, sizeof(line), "M2:%lu %s", (unsigned long) spd2,
				busy2 ? "RUN" : "IDLE");
		OLED_Print(&oled, line);
	} else if (page == 1) /* PAGE_SETTINGS */
	{
		OLED_SetCursor(0, 0);
		snprintf(line, sizeof(line), "SETTINGS%s", estop_active ? " !EST" : "");
		OLED_Print(&oled, line);

		OLED_SetCursor(0, 1);
		snprintf(line, sizeof(line), "uStep:1/%u", (unsigned) microstep);
		OLED_Print(&oled, line);

		OLED_SetCursor(0, 2);
		snprintf(line, sizeof(line), "Link:%s", link_motors ? "ON" : "OFF");
		OLED_Print(&oled, line);

		OLED_SetCursor(0, 3);
		snprintf(line, sizeof(line), "Drv:%s", drivers_enabled ? "EN" : "DIS");
		OLED_Print(&oled, line);
	} else /* PAGE_STATUS */
	{
		/* POS/REM хранятся в микрошаговых единицах.
		 Для удобства показываем FULL steps: делим на microstep. */
		int32_t pos_f = (microstep ? (pos_sel / (int32_t) microstep) : 0);
		int32_t rem_f = (microstep ? (rem_sel / (int32_t) microstep) : 0);

		uint32_t spd = (selected_axis == 0) ? spd1 : spd2;
		uint8_t run = (selected_axis == 0) ? busy1 : busy2;

		OLED_SetCursor(0, 0);
		snprintf(line, sizeof(line), "STATUS M%u 1/%u",
				(unsigned) (selected_axis + 1), (unsigned) microstep);
		OLED_Print(&oled, line);

		OLED_SetCursor(0, 1);
		snprintf(line, sizeof(line), "POSf:%ld", (long) pos_f);
		OLED_Print(&oled, line);

		OLED_SetCursor(0, 2);
		snprintf(line, sizeof(line), "REMf:%ld", (long) rem_f);
		OLED_Print(&oled, line);

		OLED_SetCursor(0, 3);
		snprintf(line, sizeof(line), "SPD:%lu %s%s", (unsigned long) spd,
				run ? "RUN" : "IDLE", estop_active ? " !EST" : "");
		OLED_Print(&oled, line);
	}




	OLED_Update(&oled);
}
