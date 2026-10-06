#include "si_rows.h"

#include <string.h>

#include "si_data.h"
#include "si_pic.h"
#include "si_rows_data.h"

#ifdef SI_ROWS_AT
__at(SI_ROWS_AT) struct si_rows_ram si_rows_ram;
typedef char si_rows_fits[SI_ROWS_AT + sizeof(struct si_rows_ram) <= SI_ROWS_END && SI_ROWS_AT % 128 == 0 ? 1 : -1];
#else
struct si_rows_ram si_rows_ram;
#endif

#define ROW_BYTES SI_ROWS_ROW_BYTES /* from a tile row to the next */

/* The rows' banks: mapped for a picture's rows, then the bank that was. */
#ifdef SI_ROWS_FAR
/* far.c's far_bank, without the call: the bank mapped, kept in
   far_mapped, and the MBC5's register. */
extern uint8_t far_mapped;
#define far_bank(bank) (far_mapped = (bank), *(volatile uint8_t *)0x2000 = far_mapped)
#define rows_map(which) far_bank((uint8_t)(SI_ROWS_FAR + (which)))
#else
#define rows_map(which) ((void)(which))
#define far_bank(bank) ((void)(bank))
#endif

/* The whole screen set to a byte: a platform with a faster way than the
   compiler's memset (SI_ROWS_FILL, the Game Boy's gb_fill, which takes
   the count in eights) supplies it. */
#ifdef SI_ROWS_FILL
void SI_ROWS_FILL(uint8_t *dst, uint8_t value, uint8_t eights);
#define screen_fill(value) SI_ROWS_FILL(si_rows_screen, (value), SI_ROWS_SCREEN_SIZE / 8)
typedef char screen_eights[SI_ROWS_SCREEN_SIZE / 8 < 256 ? 1 : -1];
#else
#define screen_fill(value) memset(si_rows_screen, (value), SI_ROWS_SCREEN_SIZE)
#endif

/* The first byte of row y. */
static uint8_t *row_start(uint8_t y)
{
    return si_rows_screen + (y >> 3) * ROW_BYTES + (y & 7);
}

static void put(uint8_t *d, uint8_t bits, uint8_t mask, uint8_t op)
{
    switch (op) {
    case SI_OP_COPY:
        *d = (uint8_t)((*d & ~mask) | (bits & mask));
        break;
    case SI_OP_OR:
        *d |= bits & mask;
        break;
    default:
        *d ^= bits & mask;
        break;
    }
}

/* The rows put_rows hands to the loop that draws them: `h` rows from
   `dst`, the first `y` rows into its tile, each of `n` bytes 8 apart from
   `src`, rows `stride` apart there; `first` masks the first byte and
   `last` the last (for copying). A platform with a faster loop
   (SI_ROWS_PUT, the Game Boy's gb_rows_put) takes them in these. */
struct si_rows_put {
    const uint8_t *src;
    uint8_t *dst;
    uint8_t stride, n, h, y, first, last, op;
};
#ifdef SI_ROWS_PUT
extern struct si_rows_put gb_rows;
void SI_ROWS_PUT(void);
#define rows gb_rows
#else
static struct si_rows_put rows;

static void put_rows_loop(void)
{
    uint8_t r, k, y = rows.y;
    const uint8_t *src = rows.src;
    uint8_t *row = rows.dst;

    for (r = 0; r < rows.h; r++, src += rows.stride) {
        uint8_t *d = row;

        for (k = 0; k < rows.n; k++, d += 8) {
            uint8_t mask = 0xff;

            if (k == 0)
                mask = rows.first;
            if (k == rows.n - 1)
                mask &= rows.last;
            put(d, src[k], mask, rows.op);
        }
        row += ++y & 7 ? 1 : ROW_BYTES - 7;
    }
}
#endif

/* h rows of n bytes each, `stride` apart, the first byte's leftmost pixel
   at column x8 * 8 (x8 may be negative), the picture's own pixels those in
   `first` of the first byte and `last` of the last (for copying). */
static void put_rows(const uint8_t *src, uint8_t stride, uint8_t n, uint8_t h, int8_t x8, int16_t y, uint8_t first,
                     uint8_t last, uint8_t op)
{
    uint8_t skip = 0, count = n, r = 0;

    if (x8 < 0)
        skip = (uint8_t)-x8;
    if (x8 + n > SI_ROWS_TILES_X)
        count = (uint8_t)(SI_ROWS_TILES_X - x8);
    if (skip >= count)
        return;
    if (y < 0)
        r = (uint8_t)-y;
    if (y + h > LCD_HEIGHT)
        h = (uint8_t)(LCD_HEIGHT - y);
    if (r >= h)
        return;
    rows.src = src + r * stride + skip;
    rows.dst = row_start((uint8_t)(y + r)) + (x8 + skip) * 8;
    rows.stride = stride;
    rows.n = (uint8_t)(count - skip);
    rows.h = (uint8_t)(h - r);
    rows.y = (uint8_t)(y + r) & 7;
    rows.first = skip ? 0xff : first;
    rows.last = count < n ? 0xff : last;
    rows.op = op;
#ifdef SI_ROWS_PUT
    SI_ROWS_PUT();
#else
    put_rows_loop();
#endif
}

/* The masks of a picture w wide starting at bit s of its first byte. */
static uint8_t first_mask(uint8_t s)
{
    return (uint8_t)(0xff >> s);
}

static uint8_t last_mask(uint8_t s, uint8_t w)
{
    uint8_t end = (uint8_t)((s + w) & 7);

    return end ? (uint8_t)(0xff << (8 - end)) : 0xff;
}

#ifndef SI_ROWS_MADE
/* A picture of the game's own data: its ready-made rows. */
static void draw_made(uint8_t index, uint8_t w, uint8_t h, int16_t x, int16_t y, uint8_t op)
{
    uint8_t s = (uint8_t)x & 7, n = (uint8_t)((w + s + 7) >> 3), was = 0;
    const uint8_t *entry;

#ifdef SI_ROWS_FAR
    was = far_mapped;
#endif
    rows_map(si_rows_bank[index]);
    entry = (si_rows_bank[index] ? si_rows_b : si_rows_a) + si_rows_at[index];
    put_rows(entry + (entry[2 * s] | entry[2 * s + 1] << 8), n, n, h, (int8_t)((x - s) >> 3), y, first_mask(s),
             last_mask(s, w), op);
    far_bank(was);
    (void)was;
}
#endif

/* One 8x8 block of a picture in the phone's bands turned into rows, `cols`
   of its columns (the rest clear), the rows `stride` apart. A platform
   with a faster one (SI_ROWS_BLOCK) supplies it, as the Game Boy's
   gb_block_rows, whose rows are lcd_fb's, 20 bytes apart. */
#ifdef SI_ROWS_BLOCK
void SI_ROWS_BLOCK(const uint8_t *columns, uint8_t *row);
#define TEMP_STRIDE 20
#else
#define TEMP_STRIDE SI_ROWS_TILES_X
#endif

static void block_rows(const uint8_t *columns, uint8_t *row)
{
#ifdef SI_ROWS_BLOCK
    SI_ROWS_BLOCK(columns, row);
#else
    uint8_t r, c, b;

    for (r = 0; r < 8; r++, row += TEMP_STRIDE) {
        for (b = 0, c = 0; c < 8; c++)
            b = (uint8_t)(b << 1 | (columns[c] >> r & 1));
        *row = b;
    }
#endif
}

/* A picture the game made itself, in RAM: the terrain, the game-over
   box. Turned into rows a block at a time when it starts on a byte (the
   terrain, at x 0), else a pixel at a time (the box, seen only at the
   end of a game). */
typedef char temp_fits[TEMP_STRIDE * 16 <= sizeof si_rows_ram.temp ? 1 : -1];
#define temp (si_rows_ram.temp)

/* What temp holds the rows of, when it is the terrain's: its bitmap and
   height (in work RAM, which starts clear, not the cartridge's). */
static const uint8_t *terrain_bitmap;
static uint8_t terrain_h;

/* A platform may do the two loops below faster (SI_ROWS_TERRAIN, the Game
   Boy's gb_shifted_left and gb_terrain_shift): whether a band of the
   bitmap is the old one moved a column left, and the rows moved a pixel
   left with the last column's bits coming in. */
#ifdef SI_ROWS_TERRAIN
uint8_t gb_shifted_left(const uint8_t *now, const uint8_t *was);
void gb_terrain_shift(uint8_t *rows, const uint8_t *last_column, uint8_t h);
typedef char terrain_stride[TEMP_STRIDE == 20 && LCD_WIDTH == 96 ? 1 : -1];
#define band_shifted_left(now, was) gb_shifted_left(now, was)
#define terrain_shift(bitmap, h) gb_terrain_shift(temp, (bitmap) + LCD_WIDTH - 1, h)
#else
static uint8_t band_shifted_left(const uint8_t *now, const uint8_t *was)
{
    uint8_t c;

    for (c = 0, was++; c < LCD_WIDTH - 1; c++)
        if (*now++ != *was++)
            return 0;
    return 1;
}
#endif

/* Whether the terrain's bitmap is the one temp holds the rows of, moved
   one column left: as it is every tick the chapter scrolls. */
static uint8_t terrain_scrolled(const uint8_t *bitmap, uint8_t h)
{
    uint8_t band;

    if (bitmap != terrain_bitmap || h != terrain_h)
        return 0;
    for (band = 0; band < ((h + 7) >> 3); band++)
        if (!band_shifted_left(bitmap + band * LCD_WIDTH, si_rows_ram.terrain + band * LCD_WIDTH))
            return 0;
    return 1;
}

#ifndef SI_ROWS_TERRAIN
/* The rows moved one pixel left, the bitmap's last column coming in. */
static void terrain_shift(const uint8_t *bitmap, uint8_t h)
{
    uint8_t r, k, *row = temp;

    for (r = 0; r < h; r++, row += TEMP_STRIDE) {
        for (k = 0; k < SI_ROWS_TILES_X - 1; k++)
            row[k] = (uint8_t)(row[k] << 1 | row[k + 1] >> 7);
        row[k] = (uint8_t)(row[k] << 1 | (bitmap[(r >> 3) * LCD_WIDTH + LCD_WIDTH - 1] >> (r & 7) & 1));
    }
}
#endif

static void draw_ram(const struct sprite_image *im, int16_t x, int16_t y, uint8_t op)
{
    uint8_t w = im->w, h = im->h, band, bx, cols, r, c;

    /* The terrain: across the screen from x 0, a band or two high. Its
       rows from last time moved on a pixel when it has only scrolled. */
    if (!x && w == LCD_WIDTH && h <= 16) {
        uint8_t bands = (uint8_t)((h + 7) >> 3);

        if (terrain_scrolled(im->bitmap, h)) {
            terrain_shift(im->bitmap, h);
        } else {
            for (band = 0; band < bands; band++)
                for (bx = 0; bx < SI_ROWS_TILES_X; bx++)
                    block_rows(im->bitmap + band * LCD_WIDTH + bx * 8, temp + band * 8 * TEMP_STRIDE + bx);
        }
        memcpy(si_rows_ram.terrain, im->bitmap, bands * LCD_WIDTH);
        terrain_bitmap = im->bitmap;
        terrain_h = h;
        put_rows(temp, TEMP_STRIDE, SI_ROWS_TILES_X, h, 0, y, 0xff, 0xff, op);
        return;
    }
    terrain_bitmap = 0;
    if (!(x & 7) && w <= 8 * SI_ROWS_TILES_X) {
        uint8_t n = (uint8_t)((w + 7) >> 3), bands = (uint8_t)((h + 7) >> 3), columns[8];
        const uint8_t *src = im->bitmap;

        for (band = 0; band < bands; band++) {
            uint8_t *row = temp + (band & 1) * 8 * TEMP_STRIDE;

            for (bx = 0, cols = w; bx < n; bx++, src += 8, row++) {
                const uint8_t *block = src;

                if (cols < 8) {
                    memset(columns, 0, sizeof columns);
                    memcpy(columns, src, cols);
                    block = columns;
                    src += cols - 8; /* to the band's end */
                } else {
                    cols -= 8;
                }
                block_rows(block, row);
            }
            /* Two bands at a time fit the temporary rows. */
            if ((band & 1) || band == bands - 1) {
                uint8_t first = (uint8_t)(band & ~1u), rows_left = (uint8_t)(h - first * 8);

                if (rows_left > 16)
                    rows_left = 16;
                put_rows(temp, TEMP_STRIDE, n, rows_left, (int8_t)(x >> 3), (int16_t)(y + first * 8), 0xff,
                         last_mask(0, w), op);
            }
        }
        return;
    }
    for (r = 0; r < h; r++) {
        int16_t py = y + r;

        if (py < 0 || py >= LCD_HEIGHT)
            continue;
        for (c = 0; c < w; c++) {
            int16_t px = x + c;
            uint8_t bit;

            if (px < 0 || px >= LCD_WIDTH)
                continue;
            bit = (uint8_t)(0x80 >> (px & 7));
            put(row_start((uint8_t)py) + (px >> 3) * 8, (uint8_t)((im->bitmap[(r >> 3) * w + c] >> (r & 7) & 1) ? bit : 0),
                bit, op);
        }
    }
}

/* A platform may draw the pictures of the game's own data itself, all of
   draw_bitmap and draw_made (SI_ROWS_MADE, the Game Boy's gb_rows_made),
   returning 0 for a picture of the game's making, which is left to
   these. */
#ifdef SI_ROWS_MADE
uint8_t SI_ROWS_MADE(const struct si_pic *p, uint8_t op);
#endif

static void draw_bitmap(const struct si_pic *p, uint8_t op)
{
    const struct sprite_image *im;

#ifdef SI_ROWS_MADE
    if (SI_ROWS_MADE(p, op))
        return;
#endif
    im = &p->frames[p->frame];

    /* As si_pic.c: one starting in the screen's last column or row is not
       drawn; wholly off the left or the top, nothing to draw. */
    if (p->x >= LCD_WIDTH - 1 || p->y >= LCD_HEIGHT - 1 || p->x + im->w <= 0 || p->y + im->h <= 0)
        return;
#ifndef SI_ROWS_MADE
    if (im >= si_pictures && im < si_pictures + sizeof si_pictures / sizeof si_pictures[0])
        draw_made((uint8_t)(im - si_pictures), im->w, im->h, p->x, p->y, op);
    else
#endif
        draw_ram(im, p->x, p->y, op);
}

/* A row of set bits, for fills. */
static const uint8_t set_row[SI_ROWS_TILES_X] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
typedef char set_row_fits[SI_ROWS_TILES_X == 12 ? 1 : -1];

static void draw_fill(const struct si_pic *p, uint8_t op)
{
    int16_t right = p->x + p->x2, under = p->y + p->y2;
    uint8_t x, end, top, bottom, x8, last8;

    if (right <= 0 || under <= 0 || p->x >= LCD_WIDTH || p->y >= LCD_HEIGHT)
        return;
    x = p->x < 0 ? 0 : (uint8_t)p->x;
    top = p->y < 0 ? 0 : (uint8_t)p->y;
    end = right > LCD_WIDTH ? LCD_WIDTH : (uint8_t)right;
    bottom = under > LCD_HEIGHT ? LCD_HEIGHT : (uint8_t)under;
    if (x >= end || top >= bottom)
        return;
    /* All of the screen, set: what the game's black ground is. */
    if (!x && !top && end == LCD_WIDTH && bottom == LCD_HEIGHT && op != SI_OP_XOR) {
        screen_fill(0xff);
        return;
    }
    x8 = x >> 3;
    last8 = (uint8_t)((end - 1) >> 3);
    put_rows(set_row, 0, (uint8_t)(last8 - x8 + 1), (uint8_t)(bottom - top), (int8_t)x8, top, first_mask(x & 7),
             last_mask(0, (uint8_t)(end - last8 * 8)), op);
}

static void plot(int16_t x, int16_t y, uint8_t op)
{
    uint8_t bit;

    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT)
        return;
    bit = (uint8_t)(0x80 >> (x & 7));
    put(row_start((uint8_t)y) + (x >> 3) * 8, bit, bit, op);
}

/* As si_pic.c's. */
static void draw_line(const struct si_pic *p, uint8_t op)
{
    int16_t x = p->x, y = p->y, dx = p->x2 - x, dy = p->y2 - y, sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1, err;

    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    err = dx - dy;
    for (;;) {
        plot(x, y, op);
        if (x == p->x2 && y == p->y2)
            return;
        if (2 * err > -dy) {
            err -= dy;
            x += sx;
        }
        if (2 * err < dx) {
            err += dx;
            y += sy;
        }
    }
}

void si_rows_render(void) SI_FAR
{
    uint8_t id;

    /* Cleared first, unless the first picture sets all of it anyway: the
       game's black ground. */
    id = si_pics[0].next;
    if (!id || si_pics[id].kind != SI_PIC_FILL || si_pic_op(si_pics[id].mode) == SI_OP_XOR
        || si_pic_op(si_pics[id].mode) == SI_OP_NONE || si_pics[id].x > 0
        || si_pics[id].y > 0 || si_pics[id].x + si_pics[id].x2 < LCD_WIDTH || si_pics[id].y + si_pics[id].y2 < LCD_HEIGHT)
        screen_fill(0);
    for (id = si_pics[0].next; id; id = si_pics[id].next) {
        const struct si_pic *p = &si_pics[id];
        uint8_t op = si_pic_op(p->mode);

        if (op == SI_OP_NONE)
            continue;
        switch (p->kind) {
        case SI_PIC_BITMAP:
            draw_bitmap(p, op);
            break;
        case SI_PIC_FILL:
            draw_fill(p, op);
            break;
        case SI_PIC_LINE:
            draw_line(p, op);
            break;
        }
    }
    /* Below the screen's last row, in its last row of tiles: clear, as
       the whole-screen fill leaves it set. */
    for (id = 0; id < SI_ROWS_TILES_X; id++)
        memset(si_rows_screen + (SI_ROWS_TILES_Y - 1) * ROW_BYTES + id * 8 + (LCD_HEIGHT & 7), 0,
               8 - (LCD_HEIGHT & 7));
}
