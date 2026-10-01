/* 84x48 1-bit framebuffer and the drawing primitives the games use. */
#ifndef CORE_LCD_H
#define CORE_LCD_H

#include <stdint.h>

#define LCD_WIDTH 84
#define LCD_HEIGHT 48

/* One byte per pixel, row-major, 0 = clear, 1 = set. */
extern uint8_t lcd_fb[LCD_WIDTH * LCD_HEIGHT];

void lcd_clear(void);
void lcd_fill_rect(int x, int y, int w, int h, uint8_t color);

/* Column-major bitmap as stored in the firmware: (h + 7) / 8 bytes per
   column, bit (y & 7) of each byte is row y. Draws both set and clear
   pixels; anything outside the screen is clipped. */
void lcd_blit_bitmap(int x, int y, int w, int h, const uint8_t *data);

/* Bitmap stored as strips of 8 rows, w bytes per strip (the LCD
   controller's own layout): bit (y & 7) of byte x + w * (y / 8). */
void lcd_blit_strips(int x, int y, int w, int h, const uint8_t *data);

#endif
