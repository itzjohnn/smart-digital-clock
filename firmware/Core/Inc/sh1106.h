#ifndef INC_SH1106_H_
#define INC_SH1106_H_

#include "main.h"
#include "fonts.h"

// Hardware Configuration
#define SH1106_I2C_ADDR         (0x3C << 1) // 7-bit 0x3C shifted
#define SH1106_WIDTH            128
#define SH1106_HEIGHT           64

// Pixel Color Definitions
typedef enum
{
    SH1106_COLOR_BLACK = 0x00, // Pixel OFF
    SH1106_COLOR_WHITE = 0x01 // Pixel ON
} SH1106_Color_t;

// Driver API
HAL_StatusTypeDef SH1106_Init(I2C_HandleTypeDef *hi2c);
void SH1106_UpdateScreen(I2C_HandleTypeDef *hi2c);
void SH1106_Clear(void);
void SH1106_DrawPixel(int16_t x, int16_t y, SH1106_Color_t color);

// Text & Cursor API
void SH1106_SetCursor(uint8_t x, uint8_t y);
char SH1106_WriteChar(char ch, FontDef_t font, SH1106_Color_t color);
char SH1106_WriteString(char *str, FontDef_t font, SH1106_Color_t color);

#endif