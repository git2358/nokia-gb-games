/* The full-screen menus made at build time (native_tiles.h), on the GBA.

   The GBA shows lcd_fb as pixels, a cell at a time where it changed. A
   made screen is unpacked straight into lcd_fb, and only the cells whose
   bytes differ from what it held are marked to be shown again: going from
   one menu to the next redraws the cells that differ, not the screen. What
   is on the screen stays what lcd_fb holds, so the cursor and everything
   else are drawn over it as before. */
#include <stdint.h>

#include "lcd.h"
#include "native_tiles.h"

extern const uint16_t native_screen_count;
extern const uint16_t native_row_data[];
extern const uint16_t native_screen_rows[];
extern const uint16_t native_screen_start[];
extern const uint8_t native_tiles[][8];

static const uint8_t empty[8];
static const uint8_t full[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

uint8_t native_fb_show(uint16_t id)
{
    const uint16_t *rows;
    uint8_t tx, ty, row;

    if (id >= native_screen_count || native_screen_start[id] == 0xffff)
        return 0;
    rows = native_screen_rows + native_screen_start[id];
    for (ty = 0; ty < LCD_CELLS_Y; ty++) {
        const uint16_t *p = native_row_data + rows[ty];
        uint16_t n = *p++;

        for (tx = 0; tx < LCD_CELLS_X; tx++) {
            uint8_t *fb = lcd_fb + ty * 8 * LCD_STRIDE + tx, changed = 0;
            const uint8_t *src = empty;

            if (n && (*p >> 11) == tx) {
                uint16_t tile = *p++ & 0x7ff;

                n--;
                src = tile == 1 ? full : native_tiles[tile - 2];
            }
            for (row = 0; row < 8; row++, fb += LCD_STRIDE) {
                changed |= (uint8_t)(*fb ^ src[row]);
                *fb = src[row];
            }
            if (changed)
                lcd_dirty[tx + LCD_CELLS_X * ty] = 1;
        }
    }
    return 1;
}
