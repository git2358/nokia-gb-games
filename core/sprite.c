#include "sprite.h"

#include <string.h>

#include "lcd.h"

struct sprite sprites[SPRITE_COUNT + 1];
static uint16_t free_head;

uint8_t sprite_screen[84 * SPRITE_SCREEN_BANDS];
static uint8_t shown[84 * SPRITE_SCREEN_BANDS];

void sprite_reset_all(void)
{
    uint16_t id;

    memset(sprites, 0, sizeof sprites);
    for (id = 1; id < SPRITE_COUNT; id++)
        sprites[id].next = (uint16_t)(id + 1);
    sprites[id].next = 0;
    free_head = 1;
}

/* Takes a sprite from the free list and links it after the last sprite of
   its own or a lower layer. */
static uint16_t sprite_alloc(uint8_t flags)
{
    uint16_t id = free_head, prev = 0, at = sprites[0].next;

    if (!id)
        return 0;
    while (at && (sprites[at].flags >> 6) <= (flags >> 6)) {
        prev = at;
        at = sprites[at].next;
    }
    at = sprites[prev].next;
    sprites[prev].next = id;
    free_head = sprites[id].next;
    sprites[id].next = at;
    sprites[id].flags = flags;
    return id;
}

uint16_t sprite_free(uint16_t id)
{
    uint16_t prev = 0, at;

    if (!id)
        return 0;
    while ((at = sprites[prev].next) != 0) {
        if (at == id) {
            sprites[prev].next = sprites[id].next;
            sprites[id].next = free_head;
            free_head = id;
            return sprites[prev].next;
        }
        prev = at;
    }
    return 0;
}

void sprite_move(uint16_t id, int x, int y)
{
    sprites[id].x = (uint8_t)x;
    sprites[id].y = (uint8_t)y;
}

void sprite_set_line(uint16_t id, int x, int y, int x2, int y2)
{
    sprite_move(id, x, y);
    sprites[id].x2 = (uint8_t)x2;
    sprites[id].y2 = (uint8_t)y2;
}

void sprite_set_image(uint16_t id, const struct sprite_image *image)
{
    sprites[id].image = *image;
}

void sprite_set_mode(uint16_t id, uint8_t mode)
{
    sprites[id].flags = (uint8_t)((sprites[id].flags & 0xc7) | mode << 3);
}

static uint8_t make_flags(uint8_t kind, uint8_t mode, uint8_t layer)
{
    return (uint8_t)(((mode + layer * 8) & 0x1f) << 3 | kind);
}

uint16_t sprite_create(const struct sprite_image *image, uint8_t mode, uint8_t layer, int x, int y)
{
    uint16_t id = sprite_alloc(make_flags(SPRITE_BITMAP, mode, layer));

    if (id) {
        sprite_move(id, x, y);
        sprites[id].image = *image;
    }
    return id;
}

uint16_t sprite_create_line(uint8_t mode, uint8_t layer, int x, int y, int x2, int y2)
{
    uint16_t id = sprite_alloc(make_flags(SPRITE_LINE, mode, layer));

    if (id)
        sprite_set_line(id, x, y, x2, y2);
    return id;
}

uint16_t sprite_create_fill(uint8_t mode, uint8_t layer, int x, int y, int w, int h)
{
    uint16_t id = sprite_alloc(make_flags(SPRITE_FILL, mode, layer));

    if (id)
        sprite_set_line(id, x, y, w, h);
    return id;
}

/* The phone's pixel operations, by drawing attribute. */
enum {
    OP_NONE,
    OP_SET,
    OP_CLEAR,
    OP_XOR
};

/* What each mode does with a bitmap's set bits and with its clear bits. */
static const uint8_t mode_ops[6][2] = {
    { OP_NONE, OP_CLEAR }, { OP_SET, OP_NONE }, { OP_XOR, OP_NONE },
    { OP_CLEAR, OP_SET },  { OP_SET, OP_CLEAR }, { OP_SET, OP_CLEAR },
};

static void put(unsigned x, unsigned y, uint8_t op)
{
    uint8_t *p, bit;

    if (x >= 84 || y >= 48 || op == OP_NONE)
        return;
    p = &sprite_screen[x + 84 * (y >> 3)];
    bit = (uint8_t)(1 << (y & 7));
    if (op == OP_SET)
        *p |= bit;
    else if (op == OP_CLEAR)
        *p &= (uint8_t)~bit;
    else
        *p ^= bit;
}

static void draw_bitmap(const struct sprite *s, const uint8_t *ops)
{
    unsigned x_end = (uint8_t)(s->x + s->image.w - 1), y_end = (uint8_t)(s->y + s->image.h - 1);
    unsigned x, y;

    /* A sprite whose far edge has wrapped past 255 is not drawn at all. */
    for (x = s->x; x <= x_end; x++) {
        for (y = s->y; y <= y_end; y++) {
            unsigned row = y - s->y;
            uint8_t bits = s->image.bitmap[(x - s->x) + s->image.w * (row >> 3)];

            put(x, y, ops[(bits >> (row & 7)) & 1 ? 0 : 1]);
        }
    }
}

/* Lines here are only ever vertical or horizontal. */
static void draw_line(const struct sprite *s, uint8_t op)
{
    int x = s->x, y = s->y;
    int dx = s->x2 > s->x ? 1 : s->x2 < s->x ? -1 : 0, dy = s->y2 > s->y ? 1 : s->y2 < s->y ? -1 : 0;

    for (;;) {
        put((unsigned)x, (unsigned)y, op);
        if (x == s->x2 && y == s->y2)
            break;
        if (x != s->x2)
            x += dx;
        if (y != s->y2)
            y += dy;
    }
}

void sprite_render(void)
{
    uint16_t id;

    memset(sprite_screen, 0, sizeof sprite_screen);
    for (id = sprites[0].next; id; id = sprites[id].next) {
        const struct sprite *s = &sprites[id];
        const uint8_t *ops;
        unsigned x, y;

        if (sprite_mode(s) >= SPRITE_MODE_HIDDEN)
            continue;
        ops = mode_ops[sprite_mode(s)];
        switch (sprite_kind(s)) {
        case SPRITE_BITMAP:
            draw_bitmap(s, ops);
            break;
        case SPRITE_LINE:
            draw_line(s, ops[0]);
            break;
        case SPRITE_FILL:
            for (y = s->y; y < (unsigned)s->y + s->y2; y++)
                for (x = s->x; x < (unsigned)s->x + s->x2; x++)
                    put(x, y, ops[0]);
            break;
        }
    }
}

void sprite_present(uint8_t all)
{
    unsigned i, bit;

    for (i = 0; i < sizeof sprite_screen; i++) {
        uint8_t now = sprite_screen[i], changed = all ? 0xff : (uint8_t)(now ^ shown[i]);

        if (!changed)
            continue;
        shown[i] = now;
        for (bit = 0; bit < 8; bit++)
            if (changed >> bit & 1)
                lcd_fill_rect((int)(i % 84), (int)(i / 84 * 8 + bit), 1, 1, (uint8_t)(now >> bit & 1));
    }
}

uint16_t tilemap_init(struct tilemap *t, uint8_t width, uint8_t rows, const uint8_t *map, uint8_t top,
                      const uint32_t *tiles, uint8_t mode)
{
    t->width = width;
    t->rows = rows;
    t->map = map;
    memset(t->bitmap, 0, sizeof t->bitmap);
    t->image.bitmap = t->bitmap;
    t->image.w = 84;
    t->image.h = (uint8_t)(rows << 3);
    t->top = top;
    t->tiles = tiles;
    t->sprite = sprite_create(&t->image, mode, 1, 0, top ? 0 : 48 - rows * 8);
    return t->sprite;
}

void tilemap_render(struct tilemap *t, unsigned scroll)
{
    unsigned row;

    memset(t->bitmap, 0, (size_t)t->rows * 84);
    for (row = 0; row < t->rows; row++) {
        unsigned at = (row * t->width + (scroll >> 5) % t->width) & 0xff; /* place in the map */
        unsigned column = scroll & 0x1f;                                  /* column of the tile */
        unsigned x = 0;

        do {
            unsigned tile = t->map[at];

            if (!tile) {
                if (x > 83)
                    break;
                /* Skip empty tiles a whole tile at a time. */
                do {
                    x = x - column + 32;
                    at = (at + 1) & 0xff;
                    if (at % t->width == 0)
                        at = (t->width * row) & 0xff;
                    tile = t->map[at];
                    column = 0;
                } while (!tile && x <= 83);
                if (!tile)
                    break;
            }
            if (x > 83)
                break;
            if (column > 31) {
                /* The phone steps to the next place in the map here but
                   draws this one column from the tile it was on. */
                at = (at + 1) & 0xff;
                if (at % t->width == 0)
                    at = (row * t->width) & 0xff;
                column = 0;
            }
            t->bitmap[x + row * 84] |= tilemap_tile(t->tiles[tile - 1])[column];
            column = (column + 1) & 0xff;
            x++;
        } while (x < 84);
    }
    sprite_set_image(t->sprite, &t->image);
}

/* 1 << (n mod 8) as the phone computes it: for a negative n that is not a
   multiple of 8 the shift count is out of range and the result is 0. */
static unsigned row_bit(int n)
{
    unsigned shift = (unsigned)(n % 8) & 0xff;

    return shift < 8 ? 1u << shift : 0;
}

/* The strip's byte for a pixel. The phone does not bound the column or the
   row and reads whatever follows its bitmap; here that reads as empty. */
static uint8_t strip_byte(const struct tilemap *t, int x, int y)
{
    int i = x + (y >> 3) * 84;

    return i >= 0 && i < t->rows * 84 ? t->bitmap[i] : 0;
}

int tilemap_collide(const struct tilemap *t, uint16_t id, int y)
{
    const struct sprite *s = &sprites[id], *strip = &sprites[t->sprite];
    int w = s->image.w, h = s->image.h, col, row, strip_row;

    if (t->top == 0x2b) {
        if (y > strip->image.h + strip->y)
            return 0;
        /* Rows of the strip from the sprite's top down, against the
           sprite's rows from 0. */
        for (row = 0; y < strip->image.h; y++, row++) {
            for (col = 0; col < w; col++) {
                if (row >= h)
                    return 0;
                if ((row_bit(row) & s->image.bitmap[col + w * (row >> 3)]) && (row_bit(y) & strip_byte(t, s->x + col, y)))
                    return 1;
            }
        }
        return 0;
    }
    if (t->top != 0 || strip->y > h + y)
        return 0;
    /* The sprite's rows from the strip's top down, against the strip's
       rows from 0. A sprite whose top is below the strip's top starts at
       a negative row, as on the phone. */
    row = strip->y - y;
    for (strip_row = 0; row < h; row++, strip_row++) {
        for (col = 0; col < w; col++) {
            if ((row_bit(row) & (s->image.bitmap + w * (row >> 3))[col]) && (row_bit(strip_row) & strip_byte(t, s->x + col, strip_row)))
                return 1;
        }
    }
    return 0;
}
