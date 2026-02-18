#ifndef UI_H
#define UI_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

void UI_Init(I2C_HandleTypeDef *hi2c);

void UI_Update(uint16_t pot_value, uint32_t freq1, uint32_t freq2,
		uint8_t safety_state);

void UI_DrawMenu(uint8_t selected_axis, int32_t steps_full, uint8_t busy1,
		uint8_t busy2, uint32_t spd1, uint32_t spd2);

void UI_DrawMenu2(uint8_t page, uint8_t selected_axis, int32_t steps_full,
		uint8_t microstep, uint8_t link_motors, uint8_t drivers_enabled,
		uint8_t busy1, uint8_t busy2, uint32_t spd1, uint32_t spd2,
		int32_t pos_sel, int32_t rem_sel, uint8_t estop_active,
		uint8_t start_armed, uint8_t done_show);




#endif
