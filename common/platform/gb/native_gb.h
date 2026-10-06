/* The Game Boy side of the full-screen menus made at build time: see
   native_tiles.c, and native_put.s for the copying, which is in the first
   bank because it maps the bank of the tiles. */
#ifndef NATIVE_GB_H
#define NATIVE_GB_H

#include <stdint.h>

/* The bank of native_tiles.c and the screens, and the bank of the tiles.
   The port defines them. */
extern const uint8_t native_bank, native_tile_bank;

/* native_tiles.c, called with native_bank mapped. */
uint8_t native_show(uint16_t id);
void native_cursor(uint8_t row, uint8_t on);
/* Puts the tile map back as the menus' framebuffer has it and the palette
   given; returns 0 if no made screen was up. */
uint8_t native_leave(uint8_t palette);

/* native_put.s. Video RAM is written between the lines being drawn, four
   bytes at a time, as flush_tiles does. */
/* 8 bytes to the first bit plane of the tile at `tile`. */
void native_put_tile(uint8_t *tile, const uint8_t *bytes);
/* 8 bytes from the tiles' bank to `out`. */
void native_get_far(uint8_t *out, const uint8_t *bytes);
/* 20 bytes, a row of the tile map. */
void native_put_row(uint8_t *map, const uint8_t *bytes);
void native_put_byte(uint8_t *at, uint8_t value);
/* A row of a screen: the tiles of its made cells copied to theirs, the
   row's first at `tiles`, and native_map_row filled in, each cell's own
   tile being native_first + its column. Returns the next row. */
extern uint8_t native_first, native_map_row[20];
const uint16_t *native_row(const uint16_t *row, uint8_t *tiles);

#endif
