#include "sprite.h"

#include <stddef.h>
#include <string.h>

#include "lcd.h"

struct sprite sprites[SPRITE_COUNT + 1];
static uint16_t free_head;

uint8_t sprite_screen[84 * SPRITE_SCREEN_BANDS];

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

/* One band of 8 rows of a sprite onto one band of the screen: n columns
   (at least one) from src to d, the bitmap's bytes moved up by `up` rows
   (down by -up), touching only the rows in `valid`. A platform whose
   compiler makes slow work of this may supply sprite_band, which takes the
   same in the sprite_band_ variables, and define SPRITE_PLATFORM_BAND. */
#ifdef SPRITE_PLATFORM_BAND
extern uint8_t *sprite_band_dst;
extern const uint8_t *sprite_band_src;
extern uint8_t sprite_band_n, sprite_band_valid, sprite_band_mode;
extern int8_t sprite_band_up;
void sprite_band(void);

/* The columns and the mode are the same for every band of a sprite, and
   are left in their variables from one band to the next. */
#define draw_band(d, src, n, up, valid, mode) \
    (sprite_band_dst = (d), sprite_band_src = (src), sprite_band_n = (n), sprite_band_up = (up), \
     sprite_band_valid = (valid), sprite_band_mode = (mode), sprite_band())
#else
/* What a mode does to the screen byte *d with a byte of bitmap `bits`, of
   which the rows in `valid` are the sprite's. */
#define APPLY_LOOP(expr) \
    for (; n; n--, d++, src++) { \
        uint8_t bits = BITS; \
        *d = (uint8_t)(expr); \
    }
#define APPLY_MODE \
    switch (mode) { \
    case SPRITE_MODE_CLEAR_BACKGROUND: \
        APPLY_LOOP(*d & (bits | keep)) \
        break; \
    case SPRITE_MODE_SET: \
        APPLY_LOOP(*d | (bits & valid)) \
        break; \
    case SPRITE_MODE_XOR: \
        APPLY_LOOP(*d ^ (bits & valid)) \
        break; \
    case SPRITE_MODE_INVERSE: \
        APPLY_LOOP((*d & ~(bits & valid)) | (~bits & valid)) \
        break; \
    default: \
        APPLY_LOOP((*d & keep) | (bits & valid)) \
        break; \
    }

static void draw_band(uint8_t *d, const uint8_t *src, uint8_t n, int8_t up, uint8_t valid, uint8_t mode)
{
    uint8_t keep = (uint8_t)~valid;

    if (up > 0) {
#define BITS (uint8_t)(*src << up)
        APPLY_MODE
#undef BITS
    } else if (up < 0) {
        up = (int8_t)-up;
#define BITS (uint8_t)(*src >> up)
        APPLY_MODE
#undef BITS
    } else {
#define BITS *src
        APPLY_MODE
#undef BITS
    }
}
#endif

/* Where each band of the picture starts in sprite_screen. */
static const uint16_t band_start[SPRITE_SCREEN_BANDS] = { 0, 84, 168, 252, 336, 420 };

/* A bitmap a band of its own at a time: each lands on one band of the
   screen, or across two when the sprite's row is not a multiple of 8. A
   platform that supplies sprite_band may supply this too, as
   sprite_draw_bitmap, which takes the mode in sprite_band_mode, and
   define SPRITE_PLATFORM_BITMAP. */
#ifdef SPRITE_PLATFORM_BITMAP
void sprite_draw_bitmap(const struct sprite *s);

/* What platform/gb/draw.s takes a sprite to be. */
typedef char sprite_layout[offsetof(struct sprite, x) == 3 && offsetof(struct sprite, y) == 4
                           && offsetof(struct sprite, image.bitmap) == 7 && offsetof(struct sprite, image.w) == 9
                           && offsetof(struct sprite, image.h) == 10 ? 1 : -1];

#define draw_bitmap(s, mode) (sprite_band_mode = (mode), sprite_draw_bitmap(s))
#else
static void draw_bitmap(const struct sprite *s, uint8_t mode)
{
    uint8_t w = s->image.w, h = s->image.h, n, shift, band, rows, valid;
    const uint8_t *src = s->image.bitmap;
    uint8_t *d;

    /* A sprite whose far edge has wrapped past 255 is not drawn at all. */
    if (!w || !h || (uint8_t)(s->x + w - 1) < s->x || (uint8_t)(s->y + h - 1) < s->y)
        return;
    if (s->x >= 84 || s->y >= 48)
        return;
    n = (uint8_t)(84 - s->x < w ? 84 - s->x : w);
    shift = s->y & 7;
    band = s->y >> 3;
    d = sprite_screen + band_start[band] + s->x;
    for (; h && band < SPRITE_SCREEN_BANDS; band++, d += 84, src += w) {
        rows = h > 8 ? 8 : h;
        h -= rows;
        valid = (uint8_t)(0xff >> (8 - rows));
        if (!shift) {
            draw_band(d, src, n, 0, valid, mode);
            continue;
        }
        draw_band(d, src, n, (int8_t)shift, (uint8_t)(valid << shift), mode);
        valid >>= 8 - shift;
        if (valid && band + 1 < SPRITE_SCREEN_BANDS)
            draw_band(d + 84, src, n, (int8_t)(shift - 8), valid, mode);
    }
}
#endif

/* A filled rectangle: every pixel is a set bit. */
static void draw_fill(const struct sprite *s, uint8_t mode)
{
    static const uint8_t full[84] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    unsigned x_end = (unsigned)s->x + s->x2, y_end = (unsigned)s->y + s->y2, y = s->y;
    uint8_t n;

    if (s->x >= 84 || y >= 48)
        return;
    if (x_end > 84)
        x_end = 84;
    if (y_end > 48)
        y_end = 48;
    if (x_end <= s->x)
        return;
    n = (uint8_t)(x_end - s->x);
    while (y < y_end) {
        /* The rows of this band the rectangle covers. */
        unsigned band_end = (y | 7) + 1;
        uint8_t valid = (uint8_t)(0xff << (y & 7));

        if (band_end > y_end) {
            valid &= (uint8_t)(0xff >> (band_end - y_end));
            band_end = y_end;
        }
        draw_band(sprite_screen + band_start[y >> 3] + s->x, full, n, 0, valid, mode);
        y = band_end;
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

void sprite_draw(const struct sprite *s)
{
    const uint8_t *ops;

    if (sprite_mode(s) >= SPRITE_MODE_HIDDEN)
        return;
    ops = mode_ops[sprite_mode(s)];
    switch (sprite_kind(s)) {
    case SPRITE_BITMAP:
        draw_bitmap(s, sprite_mode(s));
        break;
    case SPRITE_LINE:
        draw_line(s, ops[0]);
        break;
    case SPRITE_FILL:
        /* All set bits: the clear-background mode has nothing to do. */
        if (ops[0] != OP_NONE)
            draw_fill(s, sprite_mode(s));
        break;
    }
}

void sprite_render(void)
{
    uint16_t id;

    memset(sprite_screen, 0, sizeof sprite_screen);
    for (id = sprites[0].next; id; id = sprites[id].next)
        sprite_draw(&sprites[id]);
}

/* A platform that puts the picture on its screen itself, as the Game Boy
   does, defines SPRITE_PLATFORM_PRESENT and supplies this. */
#ifndef SPRITE_PLATFORM_PRESENT
void sprite_present(uint8_t all)
{
    static uint8_t shown[84 * SPRITE_SCREEN_BANDS];
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
#endif

uint16_t tilemap_init(struct tilemap *t, uint8_t width, uint8_t rows, const uint8_t *map, uint8_t top,
                      const uint16_t *tiles, uint8_t mode)
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

/* A number modulo the strip's width, which is all but always a power of
   two. */
#define MOD_WIDTH(n) ((t->width & (t->width - 1)) == 0 ? (n) & (unsigned)(t->width - 1) : (n) % t->width)

void tilemap_render(struct tilemap *t, unsigned scroll)
{
    unsigned row;

    t->scroll = (uint16_t)scroll;
    memset(t->bitmap, 0, (size_t)t->rows * 84);
    for (row = 0; row < t->rows; row++) {
        unsigned at = (row * t->width + MOD_WIDTH(scroll >> 5)) & 0xff; /* place in the map */
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
                    if (MOD_WIDTH(at) == 0)
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
                if (MOD_WIDTH(at) == 0)
                    at = (row * t->width) & 0xff;
                t->bitmap[x + row * 84] = tilemap_tile(t->tiles[tile - 1])[0];
                column = 1;
                x++;
            } else {
                /* The rest of this tile, or as much as fits. */
                unsigned run = 32 - column;

                if (run > 84 - x)
                    run = 84 - x;
                memcpy(t->bitmap + x + row * 84, tilemap_tile(t->tiles[tile - 1]) + column, run);
                column += run;
                x += run;
            }
        } while (x < 84);
    }
    sprite_set_image(t->sprite, &t->image);
}

/* 1 << (n mod 8) as the phone computes it: for a negative n that is not a
   multiple of 8 the shift count is out of range and the result is 0. */
static uint8_t row_bit(int n)
{
    if (n >= 0)
        return (uint8_t)(1 << (n & 7));
    return n & 7 ? 0 : 1;
}

/* Whether the strip has anything at all under a sprite of width w at column
   x. Most of the time it has not, and the sprite's pixels need not be gone
   through. Only for a sprite that is wholly within the 84 columns: one
   that is not reads the strip's bitmap from other places (see row_collide). */
static uint8_t strip_under(const struct tilemap *t, uint8_t x, uint8_t w)
{
    const uint8_t *p = t->bitmap + x;
    uint8_t band, n;

    for (band = t->rows; band; band--, p += 84 - w)
        for (n = w; n; n--)
            if (*p++)
                return 1;
    return 0;
}

/* One row of a sprite against one row of the strip: w of the sprite's bytes
   from src and the bit of them in sprite_bit, the strip's from index `at`
   of its bitmap and the bit in strip_bit. The phone does not bound the
   column or the row and reads whatever is around the strip's bitmap; here
   that reads as empty. */
static uint8_t row_collide(const struct tilemap *t, const uint8_t *src, uint8_t sprite_bit, int at, uint8_t strip_bit,
                           uint8_t w)
{
    int limit = t->rows * 84;
    const uint8_t *p;

    if (at < 0) {
        if (-at >= w)
            return 0;
        src -= at;
        w = (uint8_t)(w + at);
        at = 0;
    }
    if (at >= limit)
        return 0;
    if (limit - at < w)
        w = (uint8_t)(limit - at);
    for (p = t->bitmap + at; w; w--, src++, p++)
        if ((*src & sprite_bit) && (*p & strip_bit))
            return 1;
    return 0;
}

int tilemap_collide(const struct tilemap *t, uint16_t id, int y)
{
    const struct sprite *s = &sprites[id], *strip = &sprites[t->sprite];
    int h = s->image.h, row;
    uint8_t w = s->image.w, bit;

    if (t->top == 0x2b) {
        if (y > strip->image.h + strip->y)
            return 0;
    } else if (t->top != 0 || strip->y > h + y) {
        return 0;
    }
    if (s->x + w <= 84 && !strip_under(t, s->x, w))
        return 0;
    if (t->top == 0x2b) {
        /* Rows of the strip from the sprite's top down, against the
           sprite's rows from 0. */
        for (row = 0; y < strip->image.h && row < h; y++, row++) {
            bit = row_bit(y);
            if (bit && row_collide(t, s->image.bitmap + w * (row >> 3), (uint8_t)(1 << (row & 7)), s->x + (y >> 3) * 84, bit, w))
                return 1;
        }
        return 0;
    }
    /* The sprite's rows from the strip's top down, against the strip's
       rows from 0. A sprite whose top is below the strip's top starts at
       a negative row, as on the phone. */
    row = strip->y - y;
    for (y = 0; row < h; row++, y++) {
        bit = row_bit(row);
        if (bit && row_collide(t, s->image.bitmap + w * (row >> 3), bit, s->x + (y >> 3) * 84, (uint8_t)(1 << (y & 7)), w))
            return 1;
    }
    return 0;
}
