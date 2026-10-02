/* The phone's proportional fonts and text drawing. Font data is generated
   from the firmware's language pack into game_assets.c. */
#ifndef CORE_FONT_H
#define CORE_FONT_H

#include <stdint.h>

#define FONT_FIRST_CHAR 0x20
#define FONT_LAST_CHAR 0x7e

struct font {
    uint8_t height;
    uint8_t baseline;
    /* Glyph i (character FONT_FIRST_CHAR + i) is columns offsets[i] to
       offsets[i + 1]; bit y of a column is row y. The width includes the
       gap to the next character. */
    const uint16_t *offsets;
    const uint16_t *columns;
};

uint8_t font_char_width(const struct font *font, char c);
uint8_t font_text_width(const struct font *font, const char *text);

/* Draws one line with its top-left corner at (x, y) and returns the x after
   it. Stops at the end of the string or at a newline. */
int font_draw(const struct font *font, int x, int y, const char *text, uint8_t color);

#endif
