/* Snake. The board is 20x11 cells on a 4 px pitch; the snake is a ring of
   2-bit directions walked from the tail to the head. */
#ifndef CORE_SNAKE_H
#define CORE_SNAKE_H

#include <stdint.h>

#define SNAKE_COLS 20
#define SNAKE_ROWS 11
#define SNAKE_RING_SIZE (SNAKE_COLS * SNAKE_ROWS / 4 * 4)

enum {
    SNAKE_UP,
    SNAKE_RIGHT,
    SNAKE_DOWN,
    SNAKE_LEFT
};

struct snake {
    uint16_t tail_index;
    uint16_t head_index;
    int8_t head_x, head_y;
    int8_t tail_x, tail_y;
    uint8_t direction;
    uint8_t ring[SNAKE_RING_SIZE / 4];
    /* One bit per cell: byte x + SNAKE_COLS * (y / 8), bit y & 7. */
    uint8_t occupied[SNAKE_COLS * ((SNAKE_ROWS - 1) / 8 + 1)];
    int8_t food_x, food_y;
};

extern struct snake snake;

/* New one-player game: nine cells along the bottom row heading right, and
   the first food in the middle of the board. */
void snake_init(void);
void snake_move_head(void);
void snake_advance_tail(void);
void snake_draw(void);

#endif
