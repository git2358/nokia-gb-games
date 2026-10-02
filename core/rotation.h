/* Rotation. A square of numbers in random order, to be put in order by
   turning the numbers inside a movable frame: a 2x2 frame on the first four
   levels (boards of 3x3 to 6x6), a 3x3 one, whose middle stays put, on the
   last three (4x4 to 6x6). The quicker, the higher the score. */
#ifndef CORE_ROTATION_H
#define CORE_ROTATION_H

#include <stdint.h>

#define ROTATION_LEVELS 7
#define ROTATION_STRIDE 6 /* cells per row of the board array */

/* Steps of a turn's animation. A turn against the clock moves the numbers
   first and shows them sliding in; one with the clock shows them sliding
   out and then moves them. */
enum {
    ROTATION_IDLE,
    ROTATION_CCW_1,
    ROTATION_CCW_2,
    ROTATION_CW_1,
    ROTATION_CW_2,
    ROTATION_SETTLE,
    ROTATION_SOLVED /* the board flashes inverted before the game ends */
};

struct rotation {
    uint8_t level;
    uint8_t size;     /* cells along a side of the board */
    uint8_t ring;     /* 0 for the 2x2 frame, 1 for the 3x3 one */
    uint8_t x, y;     /* the frame's top-left cell */
    uint8_t phase;
    uint8_t scramble; /* turns the game still makes by itself at the start */
    uint8_t over;     /* the game has ended; score is final */
    uint16_t time;    /* half seconds played, not counting turns */
    uint16_t score;
    uint8_t board[ROTATION_STRIDE * ROTATION_STRIDE]; /* cell x + 6 * y */
};

extern struct rotation rotation;

/* New game at level 0..6. */
void rotation_init(uint8_t level);

/* The functions below return the delay until the next rotation_tick in
   scheduler ticks of 7.78125 ms; 0 leaves the timer as it is. */

/* The game comes on the screen, at its start or after a pause. */
uint16_t rotation_resume(void);

/* A phone key: 2/4/6/8 move the frame, wrapping; 1 and 7 turn the numbers
   in it against the clock, 3, 5 and 9 with it. Keys are ignored during a
   turn and during the opening turns. */
uint16_t rotation_key(char key);

/* Sets rotation.over, and returns 0, when the game has ended. */
uint16_t rotation_tick(void);

void rotation_draw(void);

/* The same picture on top of an earlier rotation_draw, drawing only what
   changed. */
void rotation_draw_changes(void);

#endif
