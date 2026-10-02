/* The sprite list and scrolling tile layer the 3310's games draw with.

   Sprites live in a fixed table and are linked in draw order: by layer,
   and within a layer in order of creation. A sprite's id is its index in
   the table, 1 upwards; the games keep their own per-object data under
   the same index. Drawing replays the whole list onto a cleared screen,
   which gives the picture the phone reaches by redrawing only what
   changed.

   Coordinates are bytes, as on the phone: a sprite that has moved past
   the left edge has an x near 255 and is not drawn. */
#ifndef CORE_SPRITE_H
#define CORE_SPRITE_H

#include <stdint.h>

#define SPRITE_COUNT 60

/* What a sprite is. */
enum {
    SPRITE_BITMAP, /* image */
    SPRITE_LINE,   /* from (x, y) to (x2, y2) */
    SPRITE_FILL    /* rectangle at (x, y), x2 wide and y2 high */
};

/* How a sprite's pixels combine with what is under them. For a bitmap,
   "set" and "clear" are its 1 and 0 bits; lines and fills are all set. */
enum {
    SPRITE_MODE_CLEAR_BACKGROUND, /* clear bits clear the screen, set bits leave it */
    SPRITE_MODE_SET,              /* set bits set, clear bits leave */
    SPRITE_MODE_XOR,              /* set bits flip, clear bits leave */
    SPRITE_MODE_INVERSE,          /* set bits clear, clear bits set */
    SPRITE_MODE_OPAQUE,           /* set bits set, clear bits clear */
    SPRITE_MODE_OPAQUE_BLINK,     /* as opaque here; the phone also blinks it */
    SPRITE_MODE_HIDDEN
};

/* A bitmap in the LCD's own layout: bands of 8 rows, one byte per column,
   bit (y & 7) of byte x + w * (y >> 3). */
struct sprite_image {
    const uint8_t *bitmap;
    uint8_t w, h;
};

struct sprite {
    uint16_t next; /* id of the next sprite in draw order, 0 at the end */
    uint8_t flags; /* kind in bits 0..2, mode in bits 3..5, layer in bits 6..7 */
    uint8_t x, y;
    uint8_t x2, y2; /* lines and fills only */
    struct sprite_image image;
};

/* sprites[0] is only the head of the list. */
extern struct sprite sprites[SPRITE_COUNT + 1];

#define sprite_kind(s) ((s)->flags & 7)
#define sprite_mode(s) (((s)->flags >> 3) & 7)

void sprite_reset_all(void);
uint16_t sprite_create(const struct sprite_image *image, uint8_t mode, uint8_t layer, int x, int y);
uint16_t sprite_create_line(uint8_t mode, uint8_t layer, int x, int y, int x2, int y2);
uint16_t sprite_create_fill(uint8_t mode, uint8_t layer, int x, int y, int w, int h);
/* Returns the id that followed the freed sprite. */
uint16_t sprite_free(uint16_t id);
void sprite_move(uint16_t id, int x, int y);
void sprite_set_line(uint16_t id, int x, int y, int x2, int y2);
void sprite_set_image(uint16_t id, const struct sprite_image *image);
void sprite_set_mode(uint16_t id, uint8_t mode);

/* The picture, in the layout of sprite_image: 84 bytes per band of 8 rows. */
#define SPRITE_SCREEN_BANDS 6
extern uint8_t sprite_screen[84 * SPRITE_SCREEN_BANDS];

/* Draws the list into sprite_screen. */
void sprite_render(void);
/* Copies what changed in sprite_screen since the last call to the LCD view;
   with `all`, everything. */
void sprite_present(uint8_t all);

/* A strip of terrain made of 32x8 tiles that scrolls and wraps. It is drawn
   into a bitmap of its own, shown by one sprite across the screen. */
#define TILEMAP_ROWS_MAX 2

struct tilemap {
    uint8_t width;          /* tiles per row */
    uint8_t rows;
    const uint8_t *map;     /* rows * width tile numbers, 0 for none */
    uint8_t top;            /* 0x2b when the strip is at the top of the screen, else 0 */
    uint16_t sprite;
    struct sprite_image image;
    const uint32_t *tiles;  /* firmware addresses of the tile bitmaps, for tile numbers 1 upwards */
    uint8_t bitmap[84 * TILEMAP_ROWS_MAX];
};

/* Resolves a tile's firmware address to its 32 bytes. */
const uint8_t *tilemap_tile(uint32_t address);

uint16_t tilemap_init(struct tilemap *t, uint8_t width, uint8_t rows, const uint8_t *map, uint8_t top,
                      const uint32_t *tiles, uint8_t mode);
void tilemap_render(struct tilemap *t, unsigned scroll);
/* Whether a sprite's set pixels touch the terrain's when the sprite is at
   row y. */
int tilemap_collide(const struct tilemap *t, uint16_t id, int y);

#endif
