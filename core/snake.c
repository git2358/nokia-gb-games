#include "snake.h"

#include "game_assets.h"
#include "lcd.h"

struct snake snake;

static void set_occupied(int8_t x, int8_t y, uint8_t on)
{
    uint8_t *cell = &snake.occupied[x + SNAKE_COLS * (y / 8)];
    uint8_t bit = (uint8_t)(1 << (y & 7));

    if (on)
        *cell |= bit;
    else
        *cell &= (uint8_t)~bit;
}

static uint8_t ring_get(uint16_t index)
{
    return (snake.ring[index >> 2] >> ((index & 3) << 1)) & 3;
}

static int8_t step_x(uint8_t direction)
{
    return direction == SNAKE_LEFT ? -1 : direction == SNAKE_RIGHT;
}

static int8_t step_y(uint8_t direction)
{
    return direction == SNAKE_UP ? -1 : direction == SNAKE_DOWN;
}

void snake_init(void)
{
    unsigned i;

    snake.direction = SNAKE_RIGHT;
    snake.tail_index = 0;
    snake.head_index = 0;
    snake.head_x = snake.tail_x = 0;
    snake.head_y = snake.tail_y = SNAKE_ROWS - 1;
    for (i = 0; i < sizeof snake.occupied; i++)
        snake.occupied[i] = 0;
    for (i = 0; i < sizeof snake.ring; i++)
        snake.ring[i] = 0;
    set_occupied(snake.head_x, snake.head_y, 1);

    for (i = 0; i < 8; i++)
        snake_move_head();

    snake.food_x = SNAKE_COLS / 2;
    snake.food_y = SNAKE_ROWS / 2;
}

/* The firmware also scores a filled board here; that comes with the tick. */
void snake_move_head(void)
{
    uint8_t shift = (uint8_t)((snake.head_index & 3) << 1);
    uint8_t *slot = &snake.ring[snake.head_index >> 2];

    if ((snake.head_index + 1) % SNAKE_RING_SIZE == snake.tail_index)
        snake_advance_tail();

    *slot = (uint8_t)((*slot & ~(3 << shift)) | (snake.direction << shift));
    snake.head_index = (uint16_t)((snake.head_index + 1) % SNAKE_RING_SIZE);
    snake.head_x += step_x(snake.direction);
    snake.head_y += step_y(snake.direction);
    set_occupied(snake.head_x, snake.head_y, 1);
}

void snake_advance_tail(void)
{
    uint8_t direction = ring_get(snake.tail_index);

    set_occupied(snake.tail_x, snake.tail_y, 0);
    snake.tail_x += step_x(direction);
    snake.tail_y += step_y(direction);
    snake.tail_index = (uint16_t)((snake.tail_index + 1) % SNAKE_RING_SIZE);
}

void snake_draw(void)
{
    uint16_t index;
    int8_t x = snake.tail_x, y = snake.tail_y;

    /* Border: 83x47, leaving the last column and row of the LCD clear. */
    lcd_fill_rect(0, 0, SNAKE_COLS * 4 + 2, 1, 1);
    lcd_fill_rect(SNAKE_COLS * 4 + 2, 0, 1, SNAKE_ROWS * 4 + 2, 1);
    lcd_fill_rect(0, 0, 1, SNAKE_ROWS * 4 + 2, 1);
    lcd_fill_rect(0, SNAKE_ROWS * 4 + 2, SNAKE_COLS * 4 + 3, 1, 1);

    lcd_blit_bitmap(snake.food_x * 4 + 2, snake.food_y * 4 + 2, 4, 4, snake_food_bitmap);

    /* The tail is a 3x3 block; every later segment is widened by one pixel
       towards the cell it came from, so neighbours join. */
    lcd_fill_rect(x * 4 + 2, y * 4 + 2, 3, 3, 1);
    for (index = snake.tail_index; index != snake.head_index; index = (uint16_t)((index + 1) % SNAKE_RING_SIZE)) {
        uint8_t direction = ring_get(index);

        x += step_x(direction);
        y += step_y(direction);
        lcd_fill_rect(x * 4 + 2 - (direction == SNAKE_RIGHT), y * 4 + 2 - (direction == SNAKE_DOWN),
                      direction & 1 ? 4 : 3, direction & 1 ? 3 : 4, 1);
    }
}
