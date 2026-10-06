/* Space Impact's pictures drawn as the Game Boy shows them: into the rows
   of its tiles instead of the phone's bands, the same pictures to the
   pixel (make check-golden compares the two for every frame of the
   replays). The screen is 12 tiles across and 9 down, each eight row
   bytes with the leftmost pixel in bit 7, so that a tile goes to video RAM
   as it is; a picture's rows come ready made from the build, at each of
   the eight places in a byte it can start (tools/extract_si.py), and
   drawing is a byte at a time.

   On the Game Boy (SI_ROWS_FAR, the first of the rows' two banks) this is
   in a bank of its own, called banked; what draws a picture's rows is in
   the first bank (platform/gb/blocks.s) and maps the rows' banks itself. */
#ifndef CORE_SI_ROWS_H
#define CORE_SI_ROWS_H

#include <stdint.h>

#include "lcd.h"
#include "si.h"
#include "sprite.h"

#define SI_ROWS_TILES_X (LCD_WIDTH / 8)
#define SI_ROWS_TILES_Y SPRITE_SCREEN_BANDS

/* Tile (tx, ty)'s row r of `screen` is byte ty * SI_ROWS_ROW_BYTES + tx *
   8 + r; `shown` is the platform's, for what it last put on the screen;
   `temp` si_rows.c's, for the pictures the game makes itself. A platform
   short of work RAM may keep them elsewhere (SI_ROWS_AT), as si.c its
   state. */
/* A row of tiles takes 128 bytes, the last four tiles' unused: so that on
   the Game Boy, with the screen on a 128-byte boundary, the next tile's
   byte is always in the same 256 (gb_rows_put). */
#define SI_ROWS_ROW_BYTES 128
#define SI_ROWS_SCREEN_SIZE (SI_ROWS_TILES_Y * SI_ROWS_ROW_BYTES)
struct si_rows_ram {
    uint8_t screen[SI_ROWS_SCREEN_SIZE];
    uint8_t shown[SI_ROWS_SCREEN_SIZE];
    uint8_t temp[20 * 16];
    /* The terrain's bitmap as it was when temp was made its rows. */
    uint8_t terrain[2 * LCD_WIDTH];
};
#ifdef SI_ROWS_AT
extern __at(SI_ROWS_AT) struct si_rows_ram si_rows_ram;
#else
extern struct si_rows_ram si_rows_ram;
#endif
#define si_rows_screen (si_rows_ram.screen)

/* Draws si_pic's list into si_rows_screen, cleared first. */
void si_rows_render(void) SI_FAR;

/* Puts si_rows_screen on the screen: what changed since the last call, or
   all of it. Supplied by a platform that defines SI_PLATFORM_ROWS. */
void si_rows_present(uint8_t all);

/* Pixel (x, y) of si_rows_screen. */
#define si_rows_pixel(x, y) \
    (si_rows_screen[((y) >> 3) * SI_ROWS_ROW_BYTES + ((x) >> 3) * 8 + ((y) & 7)] >> (7 - ((x) & 7)) & 1)

#endif
