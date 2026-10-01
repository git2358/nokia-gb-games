#include "font.h"

#include "lcd.h"

static uint8_t glyph_index(char c)
{
    uint8_t code = (uint8_t)c;

    if (code < FONT_FIRST_CHAR || code > FONT_LAST_CHAR)
        code = '?';
    return (uint8_t)(code - FONT_FIRST_CHAR);
}

uint8_t font_char_width(const struct font *font, char c)
{
    uint8_t i = glyph_index(c);

    return (uint8_t)(font->offsets[i + 1] - font->offsets[i]);
}

uint8_t font_text_width(const struct font *font, const char *text)
{
    uint8_t width = 0;

    while (*text && *text != '\n')
        width += font_char_width(font, *text++);
    return width;
}

int font_draw(const struct font *font, int x, int y, const char *text, uint8_t color)
{
    while (*text && *text != '\n') {
        uint8_t i = glyph_index(*text++);
        const uint16_t *column = font->columns + font->offsets[i];
        const uint16_t *end = font->columns + font->offsets[i + 1];

        for (; column != end; column++, x++) {
            uint16_t bits = *column;
            uint8_t *pixel;

            if (x < 0 || x >= LCD_WIDTH || y < 0 || y + font->height > LCD_HEIGHT)
                continue;
            for (pixel = lcd_fb + y * LCD_WIDTH + x; bits; pixel += LCD_WIDTH, bits >>= 1)
                if (bits & 1)
                    *pixel = color;
        }
    }
    return x;
}
