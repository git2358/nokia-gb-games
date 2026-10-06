/* The full-screen menus made at build time (native_tiles.h), on the Game
   Boy.

   The menus' screen gives every 8x8 cell its own tile in video RAM, the
   first 12 rows' at 0x8000 and the rest at 0x9000 (the cut in crt0.s), and
   the tile map names them in order. A made screen keeps that, except that
   empty cells and full ones name two tiles kept for them; every other cell
   gets its tile copied into its own. So nothing is drawn: a screen is
   about a hundred and fifty tiles copied and its tile map written, a few
   frames.

   Only the first bit plane is written. The palette the menus are shown
   with while a made screen is up takes the colour from that plane alone,
   so what the second holds from before does not matter.

   This file and the screens are in one bank, NATIVE_BANK; the tiles fill
   the next (native_put.s, in the first bank, reads them). The port says
   what both are and calls these through its far_ functions. */
#include <stdint.h>

#include "native_gb.h"
#include "native_tiles.h"

#define REG(addr) (*(volatile uint8_t *)(addr))
#define BGP REG(0xff47)

#define CELLS_X 20
#define CELLS_Y 18
#define SPLIT_ROW 12

/* Colour 1 and 3 dark, 0 and 2 light: the first bit plane decides. */
#define NATIVE_PALETTE 0xcc

/* The two kept tiles, by the number both halves of the screen name them
   with (0x8fd0 and 0x8fe0: tiles 253 and 254 at 0x8000, -3 and -2 at
   0x9000). Clear of the tiles the game at 2x keeps up there. */
#define FULL_ID 0xfd
#define EMPTY_ID 0xfe
#define FULL_TILE ((uint8_t *)0x8fd0)
#define EMPTY_TILE ((uint8_t *)0x8fe0)

#define MAP ((uint8_t *)0x9800)

extern const uint16_t native_screen_count;
extern const uint16_t native_screen_data[];
extern const uint16_t native_screen_start[];
extern const uint8_t native_cursor_data[];
extern const uint16_t native_cursor_start[];
extern const uint8_t native_tiles[][8];

static const uint8_t empty[8];
static const uint8_t full[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* The screen up, as its data; null when none is. */
static const uint16_t *shown;

/* The first cell of a row: its tile, and the number the map names it by. */
static uint8_t row_first(uint8_t ty)
{
    return (uint8_t)((ty < SPLIT_ROW ? ty : ty - SPLIT_ROW) * CELLS_X);
}

static uint8_t *row_tiles(uint8_t ty)
{
    return (ty < SPLIT_ROW ? (uint8_t *)0x8000 : (uint8_t *)0x9000) + ((uint16_t)row_first(ty) << 4);
}

uint8_t native_show(uint16_t id)
{
    const uint16_t *p;
    uint8_t ty;

    if (id >= native_screen_count || native_screen_start[id] == 0xffff)
        return 0;
    p = native_screen_data + native_screen_start[id];
    if (!shown) {
        native_put_tile(EMPTY_TILE, empty);
        native_put_tile(FULL_TILE, full);
        BGP = NATIVE_PALETTE;
    }
    shown = p;
    for (ty = 0; ty < CELLS_Y; ty++) {
        native_first = row_first(ty);
        p = native_row(p, row_tiles(ty));
        native_put_row(MAP + ty * 32, native_map_row);
    }
    return 1;
}

/* The tile of the screen up at a cell: 0 empty, 1 full, or a made one. */
static uint16_t shown_tile(uint8_t tx, uint8_t ty)
{
    const uint16_t *p = shown;
    uint8_t row, n;

    for (row = 0; row < ty; row++)
        p += *p + 1;
    for (n = (uint8_t)*p++; n; n--, p++)
        if ((uint8_t)(*p >> 11) == tx)
            return *p & 0x7ff;
    return 0;
}

void native_cursor(uint8_t row, uint8_t on)
{
    const uint8_t *c, *end;

    if (!shown || row >= NATIVE_CURSOR_ROWS)
        return;
    c = native_cursor_data + native_cursor_start[row];
    end = native_cursor_data + native_cursor_start[row + 1];
    for (; c != end; c += 10) {
        uint8_t tx = c[0], ty = c[1], i;
        uint16_t tile = shown_tile(tx, ty);
        uint8_t bytes[8];

        if (tile >= 2)
            native_get_far(bytes, native_tiles[tile - 2]);
        for (i = 0; i < 8; i++)
            bytes[i] = (uint8_t)((tile == 0 ? 0 : tile == 1 ? 0xff : bytes[i]) | (on ? c[2 + i] : 0));
        native_put_tile(row_tiles(ty) + ((uint16_t)tx << 4), bytes);
        native_put_byte(MAP + ty * 32 + tx, (uint8_t)(row_first(ty) + tx));
    }
}

uint8_t native_leave(uint8_t palette)
{
    uint8_t tx, ty;

    if (!shown)
        return 0;
    shown = 0;
    BGP = palette;
    for (ty = 0; ty < CELLS_Y; ty++) {
        uint8_t first = row_first(ty);

        for (tx = 0; tx < CELLS_X; tx++)
            native_map_row[tx] = (uint8_t)(first + tx);
        native_put_row(MAP + ty * 32, native_map_row);
    }
    return 1;
}
