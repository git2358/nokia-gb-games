#include "sprite.h"

#include <stddef.h>
#include <string.h>

#include "lcd.h"

struct sprite sprites[SPRITE_COUNT + 1];
static uint16_t free_head;

uint8_t sprite_screen[LCD_WIDTH * SPRITE_SCREEN_BANDS];

void sprite_reset(uint8_t count)
{
    uint16_t id;

    memset(sprites, 0, sizeof sprites);
    for (id = 1; id < count; id++)
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

    if (x >= LCD_WIDTH || y >= LCD_HEIGHT || op == OP_NONE)
        return;
    p = &sprite_screen[x + LCD_WIDTH * (y >> 3)];
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

/* Where a band of the picture starts in sprite_screen, and the rows of
   it that are on the screen: all of them but in the last band. */
#define band_start(band) ((uint16_t)(band) * LCD_WIDTH)
#define LAST_BAND (SPRITE_SCREEN_BANDS - 1)
#define LAST_BAND_ROWS ((LCD_HEIGHT & 7) ? (uint8_t)(0xff >> (8 - (LCD_HEIGHT & 7))) : 0xff)
#define band_rows(band) ((band) == LAST_BAND ? LAST_BAND_ROWS : 0xff)

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
    if (s->x >= LCD_WIDTH || s->y >= LCD_HEIGHT)
        return;
    n = (uint8_t)(LCD_WIDTH - s->x < w ? LCD_WIDTH - s->x : w);
    shift = s->y & 7;
    band = s->y >> 3;
    d = sprite_screen + band_start(band) + s->x;
    for (; h && band < SPRITE_SCREEN_BANDS; band++, d += LCD_WIDTH, src += w) {
        rows = h > 8 ? 8 : h;
        h -= rows;
        valid = (uint8_t)(0xff >> (8 - rows));
        if (!shift) {
            draw_band(d, src, n, 0, (uint8_t)(valid & band_rows(band)), mode);
            continue;
        }
        draw_band(d, src, n, (int8_t)shift, (uint8_t)((valid << shift) & band_rows(band)), mode);
        valid >>= 8 - shift;
        if (valid && band + 1 < SPRITE_SCREEN_BANDS)
            draw_band(d + LCD_WIDTH, src, n, (int8_t)(shift - 8), (uint8_t)(valid & band_rows(band + 1)), mode);
    }
}
#endif

/* A filled rectangle: every pixel is a set bit. */
static void draw_fill(const struct sprite *s, uint8_t mode)
{
    static uint8_t full[LCD_WIDTH];
    unsigned x_end = (unsigned)s->x + s->x2, y_end = (unsigned)s->y + s->y2, y = s->y;
    uint8_t n;

    if (!full[0])
        memset(full, 0xff, sizeof full);
    if (s->x >= LCD_WIDTH || y >= LCD_HEIGHT)
        return;
    if (x_end > LCD_WIDTH)
        x_end = LCD_WIDTH;
    if (y_end > LCD_HEIGHT)
        y_end = LCD_HEIGHT;
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
        draw_band(sprite_screen + band_start(y >> 3) + s->x, full, n, 0, valid, mode);
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
    static uint8_t shown[LCD_WIDTH * SPRITE_SCREEN_BANDS];
    unsigned i, bit;

    for (i = 0; i < sizeof sprite_screen; i++) {
        uint8_t now = sprite_screen[i], changed = all ? 0xff : (uint8_t)(now ^ shown[i]);

        if (!changed)
            continue;
        shown[i] = now;
        for (bit = 0; bit < 8; bit++)
            if (changed >> bit & 1)
                lcd_fill_rect((int)(i % LCD_WIDTH), (int)(i / LCD_WIDTH * 8 + bit), 1, 1, (uint8_t)(now >> bit & 1));
    }
}
#endif
