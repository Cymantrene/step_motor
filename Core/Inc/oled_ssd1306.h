#ifndef OLED_SSD1306_H
#define OLED_SSD1306_H

#include "main.h"
#include <stdint.h>

/* ===== I2C адрес ===== */
/* 7-битный адрес SSD1306 = 0x3C или 0x3D */
/* В HAL используется 8-битный (<<1) */

#define SSD1306_ADDR_0x3C   (0x3C << 1)
#define SSD1306_ADDR_0x3D   (0x3D << 1)

/* <<< ВАЖНО: у тебя найден 0x3C >>> */
#define SSD1306_I2C_ADDR    SSD1306_ADDR_0x3C
// #define SSD1306_I2C_ADDR SSD1306_ADDR_0x3D

/* ===== Размер дисплея ===== */

#define SSD1306_WIDTH     128
#define SSD1306_HEIGHT     64
#define SSD1306_PAGES     (SSD1306_HEIGHT / 8)

/* ===== Таймаут I2C (чтобы не висло) ===== */

#define SSD1306_I2C_TIMEOUT   50

/* ===== Структура дисплея ===== */

typedef struct {
	I2C_HandleTypeDef *hi2c;
	uint8_t buffer[SSD1306_WIDTH * SSD1306_PAGES];
} OledSSD1306;

/* ===== API ===== */

HAL_StatusTypeDef OLED_Init(OledSSD1306 *oled, I2C_HandleTypeDef *hi2c);
void OLED_Clear(OledSSD1306 *oled);
void OLED_SetCursor(uint8_t x, uint8_t page);
void OLED_Print(OledSSD1306 *oled, const char *str);
HAL_StatusTypeDef OLED_Update(OledSSD1306 *oled);

#endif /* OLED_SSD1306_H */
