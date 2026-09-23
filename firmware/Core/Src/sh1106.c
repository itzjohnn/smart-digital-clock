#include "sh1106.h"
#include <string.h>

// 1 KB Framebuffer: 128 columns * 64 rows / 8 bits per byte
static uint8_t SH1106_Buffer[SH1106_WIDTH * SH1106_HEIGHT / 8];

// Transmits a single control command byte (prefixed with 0x00)
static HAL_StatusTypeDef SH1106_WriteCommand(I2C_HandleTypeDef *hi2c, uint8_t cmd)
{
    return HAL_I2C_Mem_Write(hi2c, SH1106_I2C_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, &cmd, 1, 10);
}

// Clears internal RAM framebuffer (all pixels OFF)
void SH1106_Clear(void)
{
    memset(SH1106_Buffer, 0, sizeof(SH1106_Buffer));
}

// Sets or clears a single pixel at coordinate (x, y) inside framebuffer
void SH1106_DrawPixel(int16_t x, int16_t y, SH1106_Color_t color)
{
    // Bounds clipping
    if (x < 0 || x >= SH1106_WIDTH || y < 0 || y >= SH1106_HEIGHT)
    {
        return;
    }

    // Calculate byte index and bit position inside that vertical byte
    // y / 8 determines the page (0 - 7), multiplied by width (128), plus x column
    uint16_t index = x + (y / 8) * SH1106_WIDTH;
    uint8_t bit = y % 8;

    if (color == SH1106_COLOR_WHITE)
    {
        SH1106_Buffer[index] |= (1 << bit);
    }
    else
    {
        SH1106_Buffer[index] &= ~(1 << bit);
    }
}

// Sends standard startup command sequence to wake up and configure SH1106 panel
HAL_StatusTypeDef SH1106_Init(I2C_HandleTypeDef *hi2c)
{
    // Startup initialization sequence per SH1106 datasheet
    static const uint8_t init_cmds[] = {
        0xAE,        // Display OFF
        0x02,        // Set low column address offset (+2 for SH1106 center alignment)
        0x10,        // Set high column address
        0x40,        // Set start line address = 0
        0x81, 0x80,  // Set contrast control (default mid-level)
        0xA1,        // Set segment re-map (columns 127 mapped to SEG0, flips horizontally)
        0xC8,        // Set COM output scan direction (remapped mode, flips vertically)
        0xA6,        // Normal display mode (0 = pixel off, 1 = pixel on)
        0xA8, 0x3F,  // Multiplex ratio: 1/64 duty (0x3F = 63)
        0xD3, 0x00,  // Display offset: no offset
        0xD5, 0x80,  // Display clock divide ratio/oscillator frequency
        0xD9, 0x22,  // Pre-charge period
        0xDA, 0x12,  // COM pins hardware configuration
        0xDB, 0x40,  // VCOMH deselect level
        0x8D, 0x14,  // Charge pump regulator: enable internal DC-DC converter
        0xAF         // Turn display ON
        };

    for (size_t i = 0; i < sizeof(init_cmds); i++)
    {
        if (SH1106_WriteCommand(hi2c, init_cmds[i]) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }

    // Clear display buffer on boot
    SH1106_Clear();
    SH1106_UpdateScreen(hi2c);

    return HAL_OK;
}

// Pushes full 1024-byte framebuffer to display controller over I2C page by page
void SH1106_UpdateScreen(I2C_HandleTypeDef *hi2c)
{
    // SH1106 RAM has 8 pages (0 - 7)
    for (uint8_t page = 0; page < 8; page++)
    {
        // Set Page address: 0xB0 to 0xB7
        SH1106_WriteCommand(hi2c, 0xB0 + page);

        // Set Column address with +2 offset for 132->128 centering
        // Low nibble: 0x00 + 2 = 0x02
        // High nibble: 0x10 + 0 = 0x10
        SH1106_WriteCommand(hi2c, 0x02);
        SH1106_WriteCommand(hi2c, 0x10);

        // Write 128 bytes of data for this page (0x40 = data stream prefix)
        HAL_I2C_Mem_Write(hi2c, SH1106_I2C_ADDR, 0x40, I2C_MEMADD_SIZE_8BIT, &SH1106_Buffer[page * SH1106_WIDTH], SH1106_WIDTH, 100);
    }
}