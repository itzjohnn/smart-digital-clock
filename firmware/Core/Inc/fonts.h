#ifndef INC_FONTS_H_
#define INC_FONTS_H_

#include <stdint.h>

// Structure defining font matrix
typedef struct
{
    const uint8_t width;     // Glyph width in pixels
    const uint8_t height;    // Glyph height in pixels
    const uint16_t *data;    // Pointer to character bitmap data array
} FontDef_t;

// Export standard 7x10 font table
extern FontDef_t Font_7x10;

#endif