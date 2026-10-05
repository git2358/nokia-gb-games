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
   (x, y), to eight row bytes of lcd_fb, bit 7 - x of byte y. */
static void block_to_rows(const uint8_t *columns, uint8_t *row, uint8_t rows)
{
    uint8_t y, x, bits, mask;

    for (y = 0, mask = 1; y < rows; y++, mask <<= 1, row += LCD_STRIDE) {
        bits = 0;
        for (x = 0; x < 8; x++)
            bits = (uint8_t)(bits << 1 | ((columns[x] & mask) != 0));
        *row = bits;
    }
}

void sprite_present(uint8_t all)
{
    uint8_t band, block, rows;
    const uint8_t *now = sprite_screen;
    uint8_t *was = shown;

    for (band = 0; band < SPRITE_SCREEN_BANDS; band++) {
        rows = band == SPRITE_SCREEN_BANDS - 1 && (LCD_HEIGHT & 7) ? (LCD_HEIGHT & 7) : 8;
        for (block = 0; block < BLOCKS_X; block++, now += 8, was += 8) {
            if (!all && memcmp(now, was, 8) == 0)
                continue;
            memcpy(was, now, 8);
            block_to_rows(now, lcd_fb + (LCD_PHONE_Y + band * 8) * LCD_STRIDE + (LCD_PHONE_X / 8) + block, rows);
            lcd_dirty[(LCD_PHONE_X / 8) + block + LCD_CELLS_X * (LCD_PHONE_Y / 8 + band)] = 1;
        }
    }
}
