/* The game's picture on the Game Boy, as it is, and the cut across the
   screen the tile map needs.

   The phone's 96x65 LCD sits in lcd_fb at (32, 40), on whole tiles: 12
   tiles across and 9 down, the last row of them only a pixel high. The
   game draws into sprite_screen, in the phone's own layout of 8-row bands
   with a byte per column; here each 8x8 block of it that changed since it
   was last shown is turned round into lcd_fb's rows and its tile marked
   for main.c to make again. */
#include "screen.h"

#include <string.h>

#include "lcd.h"
#include "native_gb.h"
#include "si_data.h"
#include "si_rows.h"
#include "sprite.h"

#define LCDC_HIGH 0x81 /* LCD on, tiles 0..127 at 0x9000 */

#define BLOCKS_X (LCD_WIDTH / 8)

void lcd_cuts(uint8_t scx, uint8_t line0, uint8_t lcdc0, uint8_t scx0, uint8_t line1, uint8_t lcdc1, uint8_t scx1)
{
    __asm__("di");
    lcd_scx = scx;
    lcd_cut[0].line = line0;
    lcd_cut[0].lcdc = lcdc0;
    lcd_cut[0].scx = scx0;
    lcd_cut[1].line = line1;
    lcd_cut[1].lcdc = lcdc1;
    lcd_cut[1].scx = scx1;
    __asm__("ei");
}

void screen_cuts(void)
{
    lcd_cuts(0, 12 * 8 - 1, LCDC_HIGH, 0, 0xff, LCDC_HIGH, 0);
}

/* What the tiles were made from. */
static uint8_t shown[LCD_WIDTH * SPRITE_SCREEN_BANDS];

/* One 8x8 block: eight column bytes of a band, bit y of byte x the pixel
   (x, y), to eight row bytes of lcd_fb, bit 7 - x of byte y (blocks.s).
   lcd_fb's rows are 20 bytes apart. */
void gb_block_rows(const uint8_t *columns, uint8_t *row);
uint8_t gb_block_changed(const uint8_t *now, uint8_t *was);
typedef char lcd_fb_stride[LCD_STRIDE == 20 ? 1 : -1];

void sprite_present(uint8_t all)
{
    uint8_t band, block, rows;
    const uint8_t *now = sprite_screen;
    uint8_t *was = shown;

    for (band = 0; band < SPRITE_SCREEN_BANDS; band++) {
        rows = band == SPRITE_SCREEN_BANDS - 1 && (LCD_HEIGHT & 7) ? (LCD_HEIGHT & 7) : 8;
        for (block = 0; block < BLOCKS_X; block++, now += 8, was += 8) {
            if (all)
                memcpy(was, now, 8);
            else if (!gb_block_changed(now, was))
                continue;
            if (rows == 8) {
                gb_block_rows(now, lcd_fb + (LCD_PHONE_Y + band * 8) * LCD_STRIDE + (LCD_PHONE_X / 8) + block);
            } else {
                /* The last band: only its rows on the screen, the rest
                   of the block clear. */
                uint8_t part[8], i;

                for (i = 0; i < 8; i++)
                    part[i] = (uint8_t)(now[i] & ((1u << rows) - 1));
                gb_block_rows(part, lcd_fb + (LCD_PHONE_Y + band * 8) * LCD_STRIDE + (LCD_PHONE_X / 8) + block);
            }
            lcd_dirty[(LCD_PHONE_X / 8) + block + LCD_CELLS_X * (LCD_PHONE_Y / 8 + band)] = 1;
        }
    }
}


/* Where each row of tiles starts in lcd_fb and in video RAM, for
   gb_rows_present (blocks.s), which puts what changed on the screen
   itself. */
uint8_t *gb_rows_fb[SI_ROWS_TILES_Y];
uint8_t *gb_rows_tiles[SI_ROWS_TILES_Y];
void gb_rows_present(void);
/* How many pictures si_pictures holds, for gb_rows_made (blocks.s), in
   the first bank with it. */
const uint8_t gb_rows_count = sizeof si_pictures / sizeof si_pictures[0];
typedef char gb_rows_size[SI_ROWS_TILES_X == 12 && SI_ROWS_TILES_Y == 9 && SI_ROWS_ROW_BYTES == 128
                          && LCD_STRIDE == 20 ? 1 : -1];

void si_rows_present(uint8_t all)
{
    uint8_t tx, ty, r;
    const uint8_t *now = si_rows_screen;
    uint8_t *was = si_rows_ram.shown, *fb, *dirty;

    /* Only what changed: straight to the tiles. All of it (a new screen),
       or while a made screen is still up for show() to leave: through the
       dirty cells, as everything else. */
    if (!all && !native_up) {
        if (!gb_rows_fb[0])
            for (ty = 0; ty < SI_ROWS_TILES_Y; ty++) {
                gb_rows_fb[ty] = lcd_fb + (LCD_PHONE_Y + ty * 8) * LCD_STRIDE + LCD_PHONE_X / 8;
                gb_rows_tiles[ty] = gb_tile_address(LCD_PHONE_X / 8, (uint8_t)(LCD_PHONE_Y / 8 + ty));
            }
        gb_rows_present();
        return;
    }

    for (ty = 0; ty < SI_ROWS_TILES_Y; ty++) {
        now = si_rows_screen + ty * SI_ROWS_ROW_BYTES;
        was = si_rows_ram.shown + ty * SI_ROWS_ROW_BYTES;
        fb = lcd_fb + (LCD_PHONE_Y + ty * 8) * LCD_STRIDE + LCD_PHONE_X / 8;
        dirty = lcd_dirty + LCD_CELLS_X * (LCD_PHONE_Y / 8 + ty) + LCD_PHONE_X / 8;
        for (tx = 0; tx < SI_ROWS_TILES_X; tx++, now += 8, was += 8, fb++, dirty++) {
            uint8_t *d = fb;

            if (all)
                memcpy(was, now, 8);
            else if (!gb_block_changed(now, was))
                continue;
            for (r = 0; r < 8; r++, d += LCD_STRIDE)
                *d = now[r];
            *dirty = 1;
        }
    }
}
