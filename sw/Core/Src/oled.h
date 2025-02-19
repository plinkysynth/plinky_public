/*
 * oled.h
 *
 *  Created on: Oct 22, 2019
 *      Author: mmalex
 */

#ifndef OLED_H_
#define OLED_H_

#define W 128
#define H 32

void oled_flip(const u8* vram_with_offset);
void oled_init(void);

#ifdef IMPL
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_PAGEADDR   0x22
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETCONTRAST 0x81
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_CHARGEPUMP 0x8D
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_DEACTIVATE_SCROLL 0x2E
#define SSD1306_EXTERNALVCC 0x1
#define SSD1306_SWITCHCAPVCC 0x2
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#include "logo.h"

const static unsigned char oled_i2caddr = 0x3c << 1;

static inline void ssd1306_command(unsigned char c) {
#ifndef EMU
	u8 buf[2] = { 0, c };
	HAL_StatusTypeDef r = HAL_I2C_Master_Transmit(&hi2c2, oled_i2caddr, buf, 2, I2C_TIMEOUT);
	if (r != HAL_OK)
		DebugLog("error in ssd1306 command %d\r\n", (int) r);
	HAL_Delay(1);
#endif
}


static inline bool oled_busy(void) {
#ifndef EMU
	return HAL_I2C_GetState(&hi2c2) == HAL_I2C_STATE_BUSY_TX;
#else
	return false;
#endif
}

static inline void oled_wait(void) {
	  while (oled_busy()) ;
}

bool update_accelerometer_raw(void);


void oled_flip(const u8* vram_with_offset) {
//	  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
	// send a bunch of data in one xmission
	oled_wait();
#ifndef EMU
	ssd1306_command(0); // Page start address (0 = reset)
//	ssd1306_command(3); // Page end address - 32 pixels. 1=16 pixels

#ifdef SSD1305
	ssd1306_command(SSD1306_COLUMNADDR);
	ssd1306_command(0 + 4);   // Column start address (0 = reset) // 0 + 4
	ssd1306_command(W - 1 + 4); // Column end address (127 = reset) // W - 1 + 4
#endif

	HAL_StatusTypeDef r = 	HAL_I2C_Master_Transmit(&hi2c2, oled_i2caddr, (u8*)vram_with_offset, W * H / 8+1, 500);
	if (r != HAL_OK)
		DebugLog("error in ssd1306 data %d\r\n", (int) r);
	update_accelerometer_raw();
#else
	void OledFlipEmu(const u8 *vram);
	OledFlipEmu(vram_with_offset);
#endif
}


void oled_init(void) {
	// Init sequence
	ssd1306_command(SSD1306_DISPLAYOFF);                    // 0xAE
	ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV);            // 0xD5
	ssd1306_command(0x80);                                  // the suggested ratio 0x80

	ssd1306_command(SSD1306_SETMULTIPLEX);                  // 0xA8
	ssd1306_command(H - 1);

	ssd1306_command(SSD1306_SETDISPLAYOFFSET);              // 0xD3
	ssd1306_command(0x0);                                   // no offset
	ssd1306_command(SSD1306_SETSTARTLINE | 0x0);            // line #0
	ssd1306_command(SSD1306_CHARGEPUMP);                    // 0x8D
	ssd1306_command(0x14);  // switchcap
	ssd1306_command(SSD1306_MEMORYMODE);                    // 0x20
	ssd1306_command(0x00);                                  // 0x0 act like ks0108
	ssd1306_command(SSD1306_SEGREMAP | 0x1);
	ssd1306_command(SSD1306_COMSCANDEC);
#ifdef SSD1305
	ssd1306_command(SSD1306_SETCOMPINS);                    // 0xDA for 1305
	ssd1306_command(0x12);                                  // 0x12
#else
	ssd1306_command(SSD1306_SETCOMPINS);                    // 0xDA
	ssd1306_command(0x02);
#endif	
	ssd1306_command(SSD1306_SETCONTRAST);                   // 0x81
	ssd1306_command(0x8F);

	ssd1306_command(SSD1306_SETPRECHARGE);                  // 0xd9
	ssd1306_command(0xF1); // switchcap
	ssd1306_command(SSD1306_SETVCOMDETECT);                 // 0xDB
	ssd1306_command(0x40);
#ifdef SSD1305
	ssd1306_command(SSD1306_SETCOMPINS);                    // 0xDA for 1305
	ssd1306_command(0x12);                                  // 0x12
#endif
	ssd1306_command(SSD1306_DISPLAYALLON_RESUME);           // 0xA4
	ssd1306_command(SSD1306_NORMALDISPLAY);                 // 0xA6

	ssd1306_command(SSD1306_DEACTIVATE_SCROLL);

	// put the plinky logo up before switching on the screen
	// prepare flip
	ssd1306_command(SSD1306_COLUMNADDR);
	ssd1306_command(0);   // Column start address (0 = reset)
	ssd1306_command(W - 1); // Column end address (127 = reset)
	ssd1306_command(SSD1306_PAGEADDR);
	ssd1306_command(0); // Page start address (0 = reset)
	ssd1306_command(3); // Page end address - 32 pixels. 1=16 pixels
	ssd1306_command(SSD1306_DISPLAYON);                 //--turn on oled panel

	oled_flip(logobuf);
	oled_wait();
}
#endif
#endif /* OLED_H_ */
