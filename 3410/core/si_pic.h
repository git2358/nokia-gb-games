/* The 3410's graphics library, as much of it as Space Impact uses.

   The phone's games draw with a tree of objects under the screen; Space
   Impact's pictures are all children of the root, drawn from the first to
   the last, each over what is there. A picture is an animated bitmap (a
   list of frames, one shown), a filled rectangle or a line, with a draw
   mode and a position of signed 16 bits; creating one appends it to the
   list, and the game moves two of them to the front each tick. See "Draw
   modes" and "Draw order" in the MAME fork's docs/games_si_3410.md.

   Pictures live in a fixed table; a picture's id is its index there, 1
   upwards, 0 being none. */
#ifndef CORE_SI_PIC_H
#define CORE_SI_PIC_H

#include <stdint.h>

#include "si.h"
#include "sprite.h"

#define SI_PIC_COUNT 112

/* The draw modes the game uses, as the phone numbers them. */
#define SI_MODE_NONE 0x00
#define SI_MODE_COPY 0x10  /* set bits set, clear bits clear */
#define SI_MODE_XOR 0x12   /* set bits flip */
#define SI_MODE_BLINK 0x16 /* xor, and the phone also blinks it */
#define SI_MODE_OR 0x20    /* set bits set */
#define SI_MODE_HIDDEN 0x30

enum {
    SI_PIC_FREE,
    SI_PIC_BITMAP,
    SI_PIC_FILL,
    SI_PIC_LINE
};

struct si_pic {
    uint8_t next, prev; /* in draw order; 0 at the ends */
    uint8_t kind, mode;
    int16_t x, y;
    int16_t x2, y2;            /* a line's other end; a fill's width and height */
    const struct sprite_image *frames; /* a bitmap's frames */
    uint8_t count, frame;
};

#ifdef SI_PICS_AT
extern __at(SI_PICS_AT) struct si_pic si_pics[SI_PIC_COUNT + 1];
#else
extern struct si_pic si_pics[SI_PIC_COUNT + 1];
#endif

/* Frees every picture. */
void si_pic_reset(void) SI_FAR;
uint8_t si_pic_create(uint8_t mode, int x, int y, const struct sprite_image *frames, uint8_t count) SI_FAR;
uint8_t si_pic_create_fill(uint8_t mode, int x, int y, int w, int h) SI_FAR;
uint8_t si_pic_create_line(uint8_t mode, int x, int y, int x2, int y2) SI_FAR;
void si_pic_free(uint8_t id) SI_FAR;
/* Moves a picture to just after `after` in the list, or to the front when
   `after` is 0. */
void si_pic_move_after(uint8_t id, uint8_t after) SI_FAR;
void si_pic_set_frames(uint8_t id, const struct sprite_image *frames, uint8_t count) SI_FAR;
/* The next frame, back to the first after the last. */
void si_pic_next_frame(uint8_t id) SI_FAR;
#define si_pic_set_mode(id, m) (si_pics[id].mode = (m))
#define si_pic_move(id, px, py) (si_pics[id].x = (int16_t)(px), si_pics[id].y = (int16_t)(py))
#define si_pic_move_by(id, dx, dy) (si_pics[id].x = (int16_t)(si_pics[id].x + (dx)), \
                                    si_pics[id].y = (int16_t)(si_pics[id].y + (dy)))
#define si_pic_image(id) (&si_pics[id].frames[si_pics[id].frame])

/* What a mode does: the high nibble 1 or 2 draws (anything else nothing), a
   low nibble with 2 in it xors, else 1 copies and 2 ors (0x3652c0). */
enum {
    SI_OP_NONE,
    SI_OP_COPY,
    SI_OP_OR,
    SI_OP_XOR
};

#ifdef __SDCC
static
#else
static inline
#endif
uint8_t si_pic_op(uint8_t mode)
{
    uint8_t high = mode >> 4;

    /* 0x30, the shield's in the chapters drawn with 0x20, shows in MAME as
       the others there do. */
    if (high == 3)
        return SI_OP_OR;
    if (high != 1 && high != 2)
        return SI_OP_NONE;
    if (mode & 2)
        return SI_OP_XOR;
    return high == 1 ? SI_OP_COPY : SI_OP_OR;
}

/* Draws the list into sprite_screen, cleared first. */
void si_pic_render(void) SI_FAR;

#endif
