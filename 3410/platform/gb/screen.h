/* The cut across the Game Boy's screen where its tiles continue in the
   second tile area (crt0.s), and the game's picture put into lcd_fb. See
   screen.c. */
#ifndef GB_SCREEN_H
#define GB_SCREEN_H

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

/* The one cut the screen needs: every cell of it its own tile, 240 of them
   at 0x8000 and the rest at 0x9000. */
void screen_cuts(void);

#endif
