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
    int start = x;

    while (*text && *text != '\n') {
        uint8_t i = glyph_index(*text++);
        const uint16_t *column = font->columns + font->offsets[i];
        const uint16_t *end = font->columns + font->offsets[i + 1];

        for (; column != end; column++, x++) {
            uint16_t bits = *column;
            uint8_t mask, *pixel;

            if (x < 0 || x >= lcd_view_w || y < 0 || y + font->height > lcd_view_h)
                continue;
            mask = lcd_bit[(x + lcd_view_x) & 7];
            for (pixel = lcd_fb + (y + lcd_view_y) * LCD_STRIDE + ((x + lcd_view_x) >> 3); bits; pixel += LCD_STRIDE, bits >>= 1) {
                if (!(bits & 1))
                    continue;
                if (color)
                    *pixel |= mask;
                else
                    *pixel &= (uint8_t)~mask;
            }
        }
    }
    /* Mark what was drawn, clipped as the columns above were. */
    if (y >= 0 && y + font->height <= lcd_view_h) {
        int left = start < 0 ? 0 : start, right = x > lcd_view_w ? lcd_view_w : x;

        if (right > left)
            lcd_mark_dirty((uint8_t)(left + lcd_view_x), (uint8_t)(y + lcd_view_y), (uint8_t)(right - left), font->height);
    }
    return x;
}
