/* 84x48 1-bit framebuffer and the drawing primitives the games use. */
#ifndef CORE_LCD_H
#define CORE_LCD_H

#include <stdint.h>

#define LCD_WIDTH 84
#define LCD_HEIGHT 48
#define LCD_STRIDE 11 /* bytes per row; the last four bits are unused */

/* One bit per pixel, row-major, the leftmost pixel in bit 7. A set bit is a
   dark pixel. */
extern uint8_t lcd_fb[LCD_STRIDE * LCD_HEIGHT];

/* Bit of pixel x within its byte. */
extern const uint8_t lcd_bit[8];

#define lcd_pixel(x, y) ((lcd_fb[(y) * LCD_STRIDE + ((x) >> 3)] & lcd_bit[(x) & 7]) != 0)

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
