#include "oled_ssd1306.h"
#include <string.h>
#include "main.h"   // I2C1_SCL_Pin / I2C1_SDA_Pin
#define OLED_ROTATE_180 1   // 1 = rotate 180°, 0 = normal


/* =========================
 * I2C hang protection / bus recovery (I2C1)
 * ========================= */
static void ssd1306_i2c1_recover(I2C_HandleTypeDef *hi2c) {
	if (!hi2c || hi2c->Instance != I2C1)
		return;
	if (__get_IPSR() != 0U)
		return; // НЕ выполнять из IRQ

	(void) HAL_I2C_DeInit(hi2c);

	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* SCL */
	GPIO_InitStruct.Pin = I2C1_SCL_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(I2C1_SCL_GPIO_Port, &GPIO_InitStruct);

	/* SDA */
	GPIO_InitStruct.Pin = I2C1_SDA_Pin;
	HAL_GPIO_Init(I2C1_SDA_GPIO_Port, &GPIO_InitStruct);

	HAL_GPIO_WritePin(I2C1_SCL_GPIO_Port, I2C1_SCL_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(I2C1_SDA_GPIO_Port, I2C1_SDA_Pin, GPIO_PIN_SET);

	/* 9 pulses on SCL */
	for (int i = 0; i < 9; i++) {
		HAL_GPIO_WritePin(I2C1_SCL_GPIO_Port, I2C1_SCL_Pin, GPIO_PIN_RESET);
		for (volatile int d = 0; d < 200; d++)
			__NOP();
		HAL_GPIO_WritePin(I2C1_SCL_GPIO_Port, I2C1_SCL_Pin, GPIO_PIN_SET);
		for (volatile int d = 0; d < 200; d++)
			__NOP();
	}

	/* STOP condition */
	HAL_GPIO_WritePin(I2C1_SDA_GPIO_Port, I2C1_SDA_Pin, GPIO_PIN_RESET);
	for (volatile int d = 0; d < 200; d++)
		__NOP();
	HAL_GPIO_WritePin(I2C1_SCL_GPIO_Port, I2C1_SCL_Pin, GPIO_PIN_SET);
	for (volatile int d = 0; d < 200; d++)
		__NOP();
	HAL_GPIO_WritePin(I2C1_SDA_GPIO_Port, I2C1_SDA_Pin, GPIO_PIN_SET);
	for (volatile int d = 0; d < 200; d++)
		__NOP();

	/* Back to AF */
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;

	GPIO_InitStruct.Pin = I2C1_SCL_Pin;
	HAL_GPIO_Init(I2C1_SCL_GPIO_Port, &GPIO_InitStruct);
	GPIO_InitStruct.Pin = I2C1_SDA_Pin;
	HAL_GPIO_Init(I2C1_SDA_GPIO_Port, &GPIO_InitStruct);

	/* Reset I2C1 peripheral */
	__HAL_RCC_I2C1_FORCE_RESET();
	__HAL_RCC_I2C1_RELEASE_RESET();

	(void) HAL_I2C_Init(hi2c);
	(void) HAL_I2CEx_ConfigAnalogFilter(hi2c, I2C_ANALOGFILTER_ENABLE);
	(void) HAL_I2CEx_ConfigDigitalFilter(hi2c, 0);
}

static HAL_StatusTypeDef ssd1306_i2c_tx_guard(I2C_HandleTypeDef *hi2c,
		uint16_t devAddr, uint8_t *pData, uint16_t size, uint32_t timeout_ms) {
	HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(hi2c, devAddr, pData, size,
			timeout_ms);
	if (st == HAL_OK)
		return HAL_OK;

	ssd1306_i2c1_recover(hi2c);

	return HAL_I2C_Master_Transmit(hi2c, devAddr, pData, size, timeout_ms);
}

/* =========================
 * SSD1306 I2C low-level
 * ========================= */
#ifndef SSD1306_I2C_TIMEOUT_MS
#define SSD1306_I2C_TIMEOUT_MS 50
#endif

static HAL_StatusTypeDef ssd1306_write_cmd(OledSSD1306 *oled, uint8_t cmd) {
	uint8_t data[2] = { 0x00, cmd }; // 0x00 = command
	return ssd1306_i2c_tx_guard(oled->hi2c,
	SSD1306_I2C_ADDR, data, 2,
	SSD1306_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef ssd1306_write_data(OledSSD1306 *oled,
		const uint8_t *data, uint16_t size) {
	/* 1 byte control + up to 16 bytes payload */
	uint8_t buf[1 + 16];
	buf[0] = 0x40; // 0x40 = data

	while (size) {
		uint8_t chunk = (size > 16) ? 16 : (uint8_t) size;
		memcpy(&buf[1], data, chunk);

		/* ВАЖНО: тут тоже guard */
		HAL_StatusTypeDef res = ssd1306_i2c_tx_guard(oled->hi2c,
		SSD1306_I2C_ADDR, buf, (uint16_t) (chunk + 1),
		SSD1306_I2C_TIMEOUT_MS);
		if (res != HAL_OK)
			return res;

		data += chunk;
		size -= chunk;
	}
	return HAL_OK;
}

/* =========================
 * Cursor
 * ========================= */
static uint8_t cursor_x = 0;
static uint8_t cursor_page = 0;

void OLED_SetCursor(uint8_t x, uint8_t page) {
	if (x < SSD1306_WIDTH)
		cursor_x = x;
	if (page < SSD1306_PAGES)
		cursor_page = page;
}

/* =========================
 * 5x7 font (ASCII 32..127)
 * ... дальше оставь твой массив font5x7 и всё остальное без изменений
 * ========================= */
/* =========================
 * 5x7 font (ASCII 32..127)
 * Source: classic public-domain 5x7 font (used widely in SSD1306 libs)
 * Each glyph: 5 columns, LSB at top
 * ========================= */

#define FONT5X7_FIRST 32
#define FONT5X7_LAST  127
#define FONT5X7_COUNT (FONT5X7_LAST - FONT5X7_FIRST + 1)

static const uint8_t font5x7[FONT5X7_COUNT][5] = {
/* 0x20 ' ' */{ 0x00, 0x00, 0x00, 0x00, 0x00 },
/* 0x21 '!' */{ 0x00, 0x00, 0x5F, 0x00, 0x00 },
/* 0x22 '"' */{ 0x00, 0x07, 0x00, 0x07, 0x00 },
/* 0x23 '#' */{ 0x14, 0x7F, 0x14, 0x7F, 0x14 },
/* 0x24 '$' */{ 0x24, 0x2A, 0x7F, 0x2A, 0x12 },
/* 0x25 '%' */{ 0x23, 0x13, 0x08, 0x64, 0x62 },
/* 0x26 '&' */{ 0x36, 0x49, 0x55, 0x22, 0x50 },
/* 0x27 ''' */{ 0x00, 0x05, 0x03, 0x00, 0x00 },
/* 0x28 '(' */{ 0x00, 0x1C, 0x22, 0x41, 0x00 },
/* 0x29 ')' */{ 0x00, 0x41, 0x22, 0x1C, 0x00 },
/* 0x2A '*' */{ 0x14, 0x08, 0x3E, 0x08, 0x14 },
/* 0x2B '+' */{ 0x08, 0x08, 0x3E, 0x08, 0x08 },
/* 0x2C ',' */{ 0x00, 0x50, 0x30, 0x00, 0x00 },
/* 0x2D '-' */{ 0x08, 0x08, 0x08, 0x08, 0x08 },
/* 0x2E '.' */{ 0x00, 0x60, 0x60, 0x00, 0x00 },
/* 0x2F '/' */{ 0x20, 0x10, 0x08, 0x04, 0x02 },

/* 0x30 '0' */{ 0x3E, 0x51, 0x49, 0x45, 0x3E },
/* 0x31 '1' */{ 0x00, 0x42, 0x7F, 0x40, 0x00 },
/* 0x32 '2' */{ 0x42, 0x61, 0x51, 0x49, 0x46 },
/* 0x33 '3' */{ 0x21, 0x41, 0x45, 0x4B, 0x31 },
/* 0x34 '4' */{ 0x18, 0x14, 0x12, 0x7F, 0x10 },
/* 0x35 '5' */{ 0x27, 0x45, 0x45, 0x45, 0x39 },
/* 0x36 '6' */{ 0x3C, 0x4A, 0x49, 0x49, 0x30 },
/* 0x37 '7' */{ 0x01, 0x71, 0x09, 0x05, 0x03 },
/* 0x38 '8' */{ 0x36, 0x49, 0x49, 0x49, 0x36 },
/* 0x39 '9' */{ 0x06, 0x49, 0x49, 0x29, 0x1E },

/* 0x3A ':' */{ 0x00, 0x36, 0x36, 0x00, 0x00 },
/* 0x3B ';' */{ 0x00, 0x56, 0x36, 0x00, 0x00 },
/* 0x3C '<' */{ 0x08, 0x14, 0x22, 0x41, 0x00 },
/* 0x3D '=' */{ 0x14, 0x14, 0x14, 0x14, 0x14 },
/* 0x3E '>' */{ 0x00, 0x41, 0x22, 0x14, 0x08 },
/* 0x3F '?' */{ 0x02, 0x01, 0x51, 0x09, 0x06 },

/* 0x40 '@' */{ 0x32, 0x49, 0x79, 0x41, 0x3E },
/* 0x41 'A' */{ 0x7E, 0x11, 0x11, 0x11, 0x7E },
/* 0x42 'B' */{ 0x7F, 0x49, 0x49, 0x49, 0x36 },
/* 0x43 'C' */{ 0x3E, 0x41, 0x41, 0x41, 0x22 },
/* 0x44 'D' */{ 0x7F, 0x41, 0x41, 0x22, 0x1C },
/* 0x45 'E' */{ 0x7F, 0x49, 0x49, 0x49, 0x41 },
/* 0x46 'F' */{ 0x7F, 0x09, 0x09, 0x09, 0x01 },
/* 0x47 'G' */{ 0x3E, 0x41, 0x49, 0x49, 0x7A },
/* 0x48 'H' */{ 0x7F, 0x08, 0x08, 0x08, 0x7F },
/* 0x49 'I' */{ 0x00, 0x41, 0x7F, 0x41, 0x00 },
/* 0x4A 'J' */{ 0x20, 0x40, 0x41, 0x3F, 0x01 },
/* 0x4B 'K' */{ 0x7F, 0x08, 0x14, 0x22, 0x41 },
/* 0x4C 'L' */{ 0x7F, 0x40, 0x40, 0x40, 0x40 },
/* 0x4D 'M' */{ 0x7F, 0x02, 0x0C, 0x02, 0x7F },
/* 0x4E 'N' */{ 0x7F, 0x04, 0x08, 0x10, 0x7F },
/* 0x4F 'O' */{ 0x3E, 0x41, 0x41, 0x41, 0x3E },

/* 0x50 'P' */{ 0x7F, 0x09, 0x09, 0x09, 0x06 },
/* 0x51 'Q' */{ 0x3E, 0x41, 0x51, 0x21, 0x5E },
/* 0x52 'R' */{ 0x7F, 0x09, 0x19, 0x29, 0x46 },
/* 0x53 'S' */{ 0x46, 0x49, 0x49, 0x49, 0x31 },
/* 0x54 'T' */{ 0x01, 0x01, 0x7F, 0x01, 0x01 },
/* 0x55 'U' */{ 0x3F, 0x40, 0x40, 0x40, 0x3F },
/* 0x56 'V' */{ 0x1F, 0x20, 0x40, 0x20, 0x1F },
/* 0x57 'W' */{ 0x3F, 0x40, 0x38, 0x40, 0x3F },
/* 0x58 'X' */{ 0x63, 0x14, 0x08, 0x14, 0x63 },
/* 0x59 'Y' */{ 0x07, 0x08, 0x70, 0x08, 0x07 },
/* 0x5A 'Z' */{ 0x61, 0x51, 0x49, 0x45, 0x43 },

/* 0x5B '[' */{ 0x00, 0x7F, 0x41, 0x41, 0x00 },
/* 0x5C '\' */{ 0x02, 0x04, 0x08, 0x10, 0x20 },
/* 0x5D ']' */{ 0x00, 0x41, 0x41, 0x7F, 0x00 },
/* 0x5E '^' */{ 0x04, 0x02, 0x01, 0x02, 0x04 },
/* 0x5F '_' */{ 0x40, 0x40, 0x40, 0x40, 0x40 },
/* 0x60 '`' */{ 0x00, 0x01, 0x02, 0x04, 0x00 },

/* 0x61 'a' */{ 0x20, 0x54, 0x54, 0x54, 0x78 },
/* 0x62 'b' */{ 0x7F, 0x48, 0x44, 0x44, 0x38 },
/* 0x63 'c' */{ 0x38, 0x44, 0x44, 0x44, 0x20 },
/* 0x64 'd' */{ 0x38, 0x44, 0x44, 0x48, 0x7F },
/* 0x65 'e' */{ 0x38, 0x54, 0x54, 0x54, 0x18 },
/* 0x66 'f' */{ 0x08, 0x7E, 0x09, 0x01, 0x02 },
/* 0x67 'g' */{ 0x0C, 0x52, 0x52, 0x52, 0x3E },
/* 0x68 'h' */{ 0x7F, 0x08, 0x04, 0x04, 0x78 },
/* 0x69 'i' */{ 0x00, 0x44, 0x7D, 0x40, 0x00 },
/* 0x6A 'j' */{ 0x20, 0x40, 0x44, 0x3D, 0x00 },
/* 0x6B 'k' */{ 0x7F, 0x10, 0x28, 0x44, 0x00 },
/* 0x6C 'l' */{ 0x00, 0x41, 0x7F, 0x40, 0x00 },
/* 0x6D 'm' */{ 0x7C, 0x04, 0x18, 0x04, 0x78 },
/* 0x6E 'n' */{ 0x7C, 0x08, 0x04, 0x04, 0x78 },
/* 0x6F 'o' */{ 0x38, 0x44, 0x44, 0x44, 0x38 },

/* 0x70 'p' */{ 0x7C, 0x14, 0x14, 0x14, 0x08 },
/* 0x71 'q' */{ 0x08, 0x14, 0x14, 0x18, 0x7C },
/* 0x72 'r' */{ 0x7C, 0x08, 0x04, 0x04, 0x08 },
/* 0x73 's' */{ 0x48, 0x54, 0x54, 0x54, 0x20 },
/* 0x74 't' */{ 0x04, 0x3F, 0x44, 0x40, 0x20 },
/* 0x75 'u' */{ 0x3C, 0x40, 0x40, 0x20, 0x7C },
/* 0x76 'v' */{ 0x1C, 0x20, 0x40, 0x20, 0x1C },
/* 0x77 'w' */{ 0x3C, 0x40, 0x30, 0x40, 0x3C },
/* 0x78 'x' */{ 0x44, 0x28, 0x10, 0x28, 0x44 },
/* 0x79 'y' */{ 0x0C, 0x50, 0x50, 0x50, 0x3C },
/* 0x7A 'z' */{ 0x44, 0x64, 0x54, 0x4C, 0x44 },

/* 0x7B '{' */{ 0x00, 0x08, 0x36, 0x41, 0x00 },
/* 0x7C '|' */{ 0x00, 0x00, 0x7F, 0x00, 0x00 },
/* 0x7D '}' */{ 0x00, 0x41, 0x36, 0x08, 0x00 },
/* 0x7E '~' */{ 0x08, 0x04, 0x08, 0x10, 0x08 },
/* 0x7F DEL */{ 0x00, 0x00, 0x00, 0x00, 0x00 },
};

/* =========================
 * Draw a single char into buffer at current cursor
 * ========================= */

static void draw_char(OledSSD1306 *oled, char c) {
	if ((uint8_t) c < FONT5X7_FIRST || (uint8_t) c > FONT5X7_LAST)
		c = ' ';

	// each char uses 6 columns (5 + 1 space)
	if (cursor_x > (SSD1306_WIDTH - 6)) {
		// no space on this line -> ignore
		return;
	}

	uint16_t index = (uint16_t) cursor_page * (uint16_t) SSD1306_WIDTH
			+ cursor_x;
	if (index + 6 > sizeof(oled->buffer))
		return;

	const uint8_t *glyph = font5x7[(uint8_t) c - FONT5X7_FIRST];

	for (int i = 0; i < 5; i++) {
		oled->buffer[index++] = glyph[i];
	}
	oled->buffer[index++] = 0x00; // spacing

	cursor_x += 6;
}

/* =========================
 * Public API
 * ========================= */

void OLED_Print(OledSSD1306 *oled, const char *str) {
	while (*str) {
		if (*str == '\n') {
			cursor_page++;
			cursor_x = 0;
			if (cursor_page >= SSD1306_PAGES)
				cursor_page = (SSD1306_PAGES - 1);
			str++;
			continue;
		}
		draw_char(oled, *str++);
	}
}

void OLED_Clear(OledSSD1306 *oled) {
	memset(oled->buffer, 0x00, sizeof(oled->buffer));
	cursor_x = 0;
	cursor_page = 0;
}

HAL_StatusTypeDef OLED_Init(OledSSD1306 *oled, I2C_HandleTypeDef *hi2c) {
	oled->hi2c = hi2c;
	OLED_Clear(oled);

	// Small power-up delay helps some modules
	HAL_Delay(20);

	/* SSD1306 init sequence for 128x64 */
	if (ssd1306_write_cmd(oled, 0xAE) != HAL_OK)
		return HAL_ERROR; // display off
	ssd1306_write_cmd(oled, 0xD5);
	ssd1306_write_cmd(oled, 0x80);  // clock
	ssd1306_write_cmd(oled, 0xA8);
	ssd1306_write_cmd(oled, 0x3F);  // mux 1/64
	ssd1306_write_cmd(oled, 0xD3);
	ssd1306_write_cmd(oled, 0x00);  // offset
	ssd1306_write_cmd(oled, 0x40);                                // start line
	ssd1306_write_cmd(oled, 0x8D);
	ssd1306_write_cmd(oled, 0x14);  // charge pump
	ssd1306_write_cmd(oled, 0x20);
	ssd1306_write_cmd(oled, 0x00);  // horiz addr mode
	/* ===== Orientation (0/180 deg) ===== */
#if OLED_ROTATE_180
	ssd1306_write_cmd(oled, 0xA0);  // seg remap normal
	ssd1306_write_cmd(oled, 0xC0);  // com scan inc
#else
    ssd1306_write_cmd(oled, 0xA1);  // seg remap
    ssd1306_write_cmd(oled, 0xC8);  // com scan dec
#endif



	ssd1306_write_cmd(oled, 0xDA);
	ssd1306_write_cmd(oled, 0x12);  // com pins
	ssd1306_write_cmd(oled, 0x81);
	ssd1306_write_cmd(oled, 0x7F);  // contrast
	ssd1306_write_cmd(oled, 0xD9);
	ssd1306_write_cmd(oled, 0xF1);  // precharge
	ssd1306_write_cmd(oled, 0xDB);
	ssd1306_write_cmd(oled, 0x40);  // vcomh
	ssd1306_write_cmd(oled, 0xA4);                                // resume RAM
	ssd1306_write_cmd(oled, 0xA6);                             // normal display
	ssd1306_write_cmd(oled, 0xAF);                                // display on

	return HAL_OK;
}

HAL_StatusTypeDef OLED_Update(OledSSD1306 *oled) {
	if (ssd1306_write_cmd(oled, 0x21) != HAL_OK)
		return HAL_ERROR; // column addr
	ssd1306_write_cmd(oled, 0x00);
	ssd1306_write_cmd(oled, SSD1306_WIDTH - 1);

	if (ssd1306_write_cmd(oled, 0x22) != HAL_OK)
		return HAL_ERROR; // page addr
	ssd1306_write_cmd(oled, 0x00);
	ssd1306_write_cmd(oled, SSD1306_PAGES - 1);

	return ssd1306_write_data(oled, oled->buffer,
			(uint16_t) sizeof(oled->buffer));
}
