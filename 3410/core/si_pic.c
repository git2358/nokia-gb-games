#include "si_pic.h"

#include <string.h>

#include "lcd.h"

struct si_pic si_pics[SI_PIC_COUNT + 1];

/* The list: si_pics[0] holds its first (next) and last (prev). */
#define FIRST si_pics[0].next
#define LAST si_pics[0].prev

void si_pic_reset(void)
{
    memset(si_pics, 0, sizeof si_pics);
}

static uint8_t alloc(uint8_t kind, uint8_t mode, int x, int y)
{
    uint8_t id;
    struct si_pic *p;

    for (id = 1; id <= SI_PIC_COUNT && si_pics[id].kind != SI_PIC_FREE; id++)
        ;
    if (id > SI_PIC_COUNT)
        return 0;
    p = &si_pics[id];
    memset(p, 0, sizeof *p);
    p->kind = kind;
    p->mode = mode;
    p->x = (int16_t)x;
    p->y = (int16_t)y;
    /* Appended. */
    p->prev = LAST;
    if (LAST)
        si_pics[LAST].next = id;
    else
        FIRST = id;
    LAST = id;
    return id;
}

uint8_t si_pic_create(uint8_t mode, int x, int y, const struct sprite_image *frames, uint8_t count)
{
    uint8_t id = alloc(SI_PIC_BITMAP, mode, x, y);

    if (id) {
        si_pics[id].frames = frames;
        si_pics[id].count = count;
    }
    return id;
}

uint8_t si_pic_create_fill(uint8_t mode, int x, int y, int w, int h)
{
    uint8_t id = alloc(SI_PIC_FILL, mode, x, y);

    if (id) {
        si_pics[id].x2 = (int16_t)w;
        si_pics[id].y2 = (int16_t)h;
    }
    return id;
}

uint8_t si_pic_create_line(uint8_t mode, int x, int y, int x2, int y2)
{
    uint8_t id = alloc(SI_PIC_LINE, mode, x, y);

    if (id) {
        si_pics[id].x2 = (int16_t)x2;
        si_pics[id].y2 = (int16_t)y2;
    }
    return id;
}

static void unlink_pic(uint8_t id)
{
    struct si_pic *p = &si_pics[id];

    if (p->prev)
        si_pics[p->prev].next = p->next;
    else
        FIRST = p->next;
    if (p->next)
        si_pics[p->next].prev = p->prev;
    else
        LAST = p->prev;
    p->next = p->prev = 0;
}

void si_pic_free(uint8_t id)
{
    if (!id)
        return;
    unlink_pic(id);
    memset(&si_pics[id], 0, sizeof si_pics[id]);
}

void si_pic_move_after(uint8_t id, uint8_t after)
{
    struct si_pic *p = &si_pics[id];

    if (!id || id == after)
        return;
    unlink_pic(id);
    if (!after) {
        p->next = FIRST;
        if (FIRST)
            si_pics[FIRST].prev = id;
        else
            LAST = id;
        FIRST = id;
        return;
    }
    p->prev = after;
    p->next = si_pics[after].next;
    if (p->next)
        si_pics[p->next].prev = id;
    else
        LAST = id;
    si_pics[after].next = id;
}

void si_pic_set_frames(uint8_t id, const struct sprite_image *frames, uint8_t count)
{
    si_pics[id].frames = frames;
    si_pics[id].count = count;
    si_pics[id].frame = 0;
}

void si_pic_next_frame(uint8_t id)
{
    struct si_pic *p = &si_pics[id];

    if (++p->frame >= p->count)
        p->frame = 0;
}

/* What a mode does: the high nibble 1 or 2 draws (anything else nothing), a
   low nibble with 2 in it xors, else 1 copies and 2 ors (0x3652c0). */
enum {
    OP_NONE,
    OP_COPY,
    OP_OR,
    OP_XOR
};

static uint8_t mode_op(uint8_t mode)
{
    uint8_t high = mode >> 4;

    /* 0x30, the shield's in the chapters drawn with 0x20, shows in MAME as
       the others there do. */
    if (high == 3)
        return OP_OR;
    if (high != 1 && high != 2)
        return OP_NONE;
    if (mode & 2)
        return OP_XOR;
    return high == 1 ? OP_COPY : OP_OR;
}

static void plot(int x, int y, uint8_t set, uint8_t op)
{
    uint8_t *d, bit;

    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT)
        return;
    d = &sprite_screen[(y >> 3) * LCD_WIDTH + x];
    bit = (uint8_t)(1u << (y & 7));
    switch (op) {
    case OP_COPY:
        *d = set ? (uint8_t)(*d | bit) : (uint8_t)(*d & ~bit);
        break;
    case OP_OR:
        if (set)
            *d |= bit;
        break;
    case OP_XOR:
        if (set)
            *d ^= bit;
        break;
    }
}

static void draw_bitmap(const struct si_pic *p, uint8_t op)
{
    const struct sprite_image *im = &p->frames[p->frame];
    int col, row;

    /* One starting in the screen's last column or row is not drawn: seen
       in MAME, where a picture at x 94 shows its first two columns and one
       at 95 nothing, and a one-pixel star on the last row is not shown. */
    if (p->x >= LCD_WIDTH - 1 || p->y >= LCD_HEIGHT - 1)
        return;

    for (row = 0; row < im->h; row++) {
        int y = p->y + row;

        if (y < 0 || y >= LCD_HEIGHT)
            continue;
        for (col = 0; col < im->w; col++)
            plot(p->x + col, y, (uint8_t)(im->bitmap[(row >> 3) * im->w + col] >> (row & 7) & 1), op);
    }
}

static void draw_line(const struct si_pic *p, uint8_t op)
{
    int x = p->x, y = p->y, dx = p->x2 - x, dy = p->y2 - y, sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1, err;

    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    err = dx - dy;
    for (;;) {
        plot(x, y, 1, op);
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

void si_pic_render(void)
{
    uint8_t id;

    memset(sprite_screen, 0, sizeof sprite_screen);
    for (id = FIRST; id; id = si_pics[id].next) {
        const struct si_pic *p = &si_pics[id];
        uint8_t op = mode_op(p->mode);
        int x, y;

        if (op == OP_NONE)
            continue;
        switch (p->kind) {
        case SI_PIC_BITMAP:
            draw_bitmap(p, op);
            break;
        case SI_PIC_FILL:
            for (y = p->y; y < p->y + p->y2; y++)
                for (x = p->x; x < p->x + p->x2; x++)
                    plot(x, y, 1, op);
            break;
        case SI_PIC_LINE:
            draw_line(p, op);
            break;
        }
    }
}
