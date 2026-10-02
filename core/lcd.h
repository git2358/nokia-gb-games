/* 1-bit framebuffer and the drawing primitives the games use.

   The phone's LCD is 84x48. A platform may give the core a bigger
   framebuffer, the size of its own screen; the phone's LCD is then a window
   in the middle of it. Drawing happens in a view, which is either that
   window or the whole framebuffer, and is clipped to it. */
#ifndef CORE_LCD_H
#define CORE_LCD_H

#include <stdint.h>

#define LCD_WIDTH 84
#define LCD_HEIGHT 48

/* Framebuffer size; a platform overrides both on the compiler command line. */
#ifndef LCD_FB_WIDTH
#define LCD_FB_WIDTH LCD_WIDTH
#define LCD_FB_HEIGHT LCD_HEIGHT
#endif

#define LCD_STRIDE ((LCD_FB_WIDTH + 7) / 8) /* bytes per row */
#define LCD_CELLS_X LCD_STRIDE
#define LCD_CELLS_Y (LCD_FB_HEIGHT / 8)

/* Where the phone's LCD sits in the framebuffer. */
#define LCD_PHONE_X ((LCD_FB_WIDTH - LCD_WIDTH) / 2)
#define LCD_PHONE_Y ((LCD_FB_HEIGHT - LCD_HEIGHT) / 2)
/* A platform may show the phone's LCD magnified by this much while the view
   is the phone's; the framebuffer still holds it 1:1. */
#ifndef LCD_PHONE_ZOOM
#define LCD_PHONE_ZOOM 1
#endif
/* The first framebuffer row below the phone's LCD as shown. */
#define LCD_BELOW_PHONE (LCD_FB_HEIGHT / 2 + LCD_HEIGHT * LCD_PHONE_ZOOM / 2)
/* Whether there is room around the phone's LCD. */
#define LCD_HAS_SURROUND (LCD_FB_WIDTH > LCD_WIDTH)

/* One bit per pixel, row-major, the leftmost pixel in bit 7. A set bit is a
   dark pixel. */
extern uint8_t lcd_fb[LCD_STRIDE * LCD_FB_HEIGHT];

/* Bit of pixel x within its byte. */
extern const uint8_t lcd_bit[8];

/* Nonzero for each 8x8 cell of the framebuffer drawn to since the platform
   last cleared it: cell x / 8 + LCD_CELLS_X * (y / 8). Lets a platform
   update only what changed. */
extern uint8_t lcd_dirty[LCD_CELLS_X * LCD_CELLS_Y];

/* The view: its top-left corner in the framebuffer and its size. */
extern uint8_t lcd_view_x, lcd_view_y, lcd_view_w, lcd_view_h;

void lcd_view_phone(void);
void lcd_view_full(void);
void lcd_view_set(uint8_t x, uint8_t y, uint8_t w, uint8_t h);

/* A pixel by framebuffer position, and by position in the view. */
#define lcd_fb_pixel(x, y) ((lcd_fb[(y) * LCD_STRIDE + ((x) >> 3)] & lcd_bit[(x) & 7]) != 0)
#define lcd_pixel(x, y) lcd_fb_pixel((x) + lcd_view_x, (y) + lcd_view_y)

/* Marks the cells touched by a rectangle given in framebuffer coordinates
   that lies inside the framebuffer. */
void lcd_mark_dirty(uint8_t x, uint8_t y, uint8_t w, uint8_t h);

/* Everything below takes coordinates in the view. */

void lcd_clear(void); /* clears the view */
void lcd_fill_rect(int x, int y, int w, int h, uint8_t color);

/* Column-major bitmap as stored in the firmware: (h + 7) / 8 bytes per
   column, bit (y & 7) of each byte is row y. Draws both set and clear
   pixels; anything outside the view is clipped. */
void lcd_blit_bitmap(int x, int y, int w, int h, const uint8_t *data);

/* Bitmap stored as strips of 8 rows, w bytes per strip (the LCD
   controller's own layout): bit (y & 7) of byte x + w * (y / 8). */
void lcd_blit_strips(int x, int y, int w, int h, const uint8_t *data);

#endif
