/* The game at 2x on the Game Boy, with its strip of terrain scrolled by the
   hardware. See strip.c. */
#ifndef GB_STRIP_H
#define GB_STRIP_H

#include <stdint.h>

/* The scroll at the top of the screen and the two places it is cut across
   (crt0.s): for each, the last line before the cut and what the LCD control
   and scroll registers are after it. A line of 0xff is no cut. */
struct lcd_cut {
    uint8_t line, lcdc, scx;
};
extern volatile uint8_t lcd_scx;
extern volatile struct lcd_cut lcd_cut[2];

/* Sets all of those at once. */
void lcd_cuts(uint8_t scx, uint8_t line0, uint8_t lcdc0, uint8_t scx0, uint8_t line1, uint8_t lcdc1, uint8_t scx1);

/* The cuts and the tile map the menus and the phone-sized game use: every
   cell of the screen its own tile, 240 of them at 0x8000 and the rest at
   0x9000, scrolled by `scx`. */
void strip_leave(uint8_t scx, uint8_t write_map);

/* Puts the sprite layer's picture on the screen at 2x. With `all`, the
   screen holds something else: everything is made afresh. */
void strip_present(uint8_t all);

/* The same for a game without Space Impact's terrain. */
void zoom_present(uint8_t all);

#endif
