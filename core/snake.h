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
    uint8_t direction; /* of the last move */
    uint8_t pending;   /* direction the next move will take */
    uint8_t grow;      /* the last move ate, so the tail stays for one move */
    uint8_t hit;       /* the last move was blocked; one more ends the game */
    uint8_t level;     /* 0..8 */
    uint16_t score;
    uint8_t ring[SNAKE_RING_SIZE / 4];
    /* One bit per cell: byte x + SNAKE_COLS * (y / 8), bit y & 7. */
    uint8_t occupied[SNAKE_COLS * ((SNAKE_ROWS - 1) / 8 + 1)];
    int8_t food_x, food_y;
};

extern struct snake snake;

/* New one-player game: nine cells along the bottom row heading right, and
   the first food in the middle of the board. */
void snake_init(uint8_t level);

/* A phone key, '1' to '9': 2/4/6/8 steer, and the corner keys turn towards
   whichever of their two directions is a turn. A reversal is ignored. */
void snake_key(char key);

/* One move. Returns the delay until the next one in scheduler ticks of
   7.78125 ms, or 0 when the game is over. */
uint8_t snake_step(void);

void snake_move_head(void);
void snake_advance_tail(void);
void snake_draw(void);

#endif
