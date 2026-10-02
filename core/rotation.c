#include "rotation.h"

#include "game_assets.h"
#include "lcd.h"
#include "rand.h"
#include "sound.h"

#define TURN_TICKS 15     /* between the steps of a turn */
#define CLOCK_TICKS 64    /* half a second of playing time */
#define SCRAMBLE_TICKS 3  /* before the next opening turn */
#define SOLVED_TICKS 257  /* the solved board stays for two seconds */
#define TIME_LIMIT 10000

/* Cells are 14x8 pixels; the board is centred on (63, 36) half cells in. */
#define CELL_WIDTH 14
#define CELL_HEIGHT 8

struct rotation rotation;

static uint8_t drawn_x, drawn_y; /* the frame the LCD holds */

/* How many places the frame can take along a side. */
static uint8_t places(void)
{
    return (uint8_t)((rotation.level & 3) + 2);
}

void rotation_init(uint8_t level)
{
    uint8_t x, y, n;

    rotation.level = level;
    rotation.ring = level >> 2;
    rotation.size = (uint8_t)(rotation.ring + (level & 3) + 3);
    rotation.scramble = (uint8_t)(((level & 3) + 3) * 2);
    rotation.phase = ROTATION_IDLE;
    rotation.over = 0;
    rotation.time = 0;
    rotation.score = 0;
    n = rotation.size;

    /* The numbers in random order: each in turn changes places with a cell
       before it, picked at random. */
    for (y = 0; y < n; y++)
        for (x = 0; x < n; x++) {
            uint8_t index = (uint8_t)(x + n * y), other;

            if (!index) {
                rotation.board[0] = 1;
                continue;
            }
            other = (uint8_t)(game_rand() % index);
            other = (uint8_t)(other % n + ROTATION_STRIDE * (other / n));
            rotation.board[x + ROTATION_STRIDE * y] = rotation.board[other];
            rotation.board[other] = (uint8_t)(index + 1);
        }
    rotation.x = (uint8_t)(game_rand() % places());
    rotation.y = (uint8_t)(game_rand() % places());
}

/* The cells of the frame in order against the clock from the top-left one,
   as offsets from it. */
static const uint8_t ring_2x2[] = { 0, 6, 7, 1 };
static const uint8_t ring_3x3[] = { 0, 6, 12, 13, 14, 8, 2, 1 };

static void turn(uint8_t clockwise)
{
    uint8_t *cell = &rotation.board[rotation.x + ROTATION_STRIDE * rotation.y];
    const uint8_t *ring = rotation.ring ? ring_3x3 : ring_2x2;
    uint8_t count = rotation.ring ? 8 : 4, i, first;

    if (clockwise) {
        /* Each cell takes the number from the one after it in the ring. */
        first = cell[ring[0]];
        for (i = 0; i < count - 1; i++)
            cell[ring[i]] = cell[ring[i + 1]];
        cell[ring[count - 1]] = first;
    } else {
        first = cell[ring[count - 1]];
        for (i = count - 1; i; i--)
            cell[ring[i]] = cell[ring[i - 1]];
        cell[ring[0]] = first;
    }
}

/* In order by rows or by columns. */
static uint8_t solved(void)
{
    uint8_t x, y, n = rotation.size, rows = 1, columns = 1;

    for (y = 0; y < n; y++)
        for (x = 0; x < n; x++) {
            uint8_t number = rotation.board[x + ROTATION_STRIDE * y];

            if (number != x + n * y + 1)
                rows = 0;
            if (number != y + n * x + 1)
                columns = 0;
        }
    return rows | columns;
}

uint16_t rotation_resume(void)
{
    if (rotation.phase || rotation.scramble)
        return TURN_TICKS;
    return rotation.time < TIME_LIMIT ? CLOCK_TICKS : 0;
}

uint16_t rotation_key(char key)
{
    uint8_t n = places();

    if (rotation.phase || rotation.scramble)
        return 0;
    switch (key) {
    case '1':
    case '7':
        turn(0);
        rotation.phase = ROTATION_CCW_1;
        return TURN_TICKS;
    case '3':
    case '5':
    case '9':
        rotation.phase = ROTATION_CW_1;
        return TURN_TICKS;
    case '2':
        rotation.y = (uint8_t)((rotation.y + n - 1) % n);
        break;
    case '8':
        rotation.y = (uint8_t)((rotation.y + 1) % n);
        break;
    case '4':
        rotation.x = (uint8_t)((rotation.x + n - 1) % n);
        break;
    case '6':
        rotation.x = (uint8_t)((rotation.x + 1) % n);
        break;
    default:
        break;
    }
    return 0;
}

/* A turn has finished: on with the opening turns, or see whether the
   board is in order. */
static uint16_t settled(void)
{
    uint8_t n = places();

    rotation.phase = ROTATION_IDLE;
    if (rotation.scramble >= 2) {
        /* The next opening turn is somewhere else. */
        uint8_t place = (uint8_t)(game_rand() % (n * n - 1));

        place = (uint8_t)((rotation.y * n + rotation.x + place + 1) % (n * n));
        rotation.x = place % n;
        rotation.y = place / n;
        return SCRAMBLE_TICKS;
    }
    if (rotation.scramble) {
        /* The last one is in the corner. */
        rotation.x = rotation.y = 0;
        return SCRAMBLE_TICKS;
    }
    if (!solved())
        return CLOCK_TICKS;
    rotation.score = (uint16_t)((rotation.level + 1) * (TIME_LIMIT / (rotation.time + 10) + 1));
    rotation.phase = ROTATION_SOLVED;
    sound_play(SOUND_SOLVED);
    return SOLVED_TICKS;
}

uint16_t rotation_tick(void)
{
    switch (rotation.phase) {
    case ROTATION_IDLE:
        if (!rotation.scramble) {
            /* The clock. */
            if (rotation.time >= TIME_LIMIT)
                return 0;
            rotation.time++;
            return CLOCK_TICKS;
        }
        rotation.scramble--;
        if (game_rand() & 1) {
            turn(0);
            rotation.phase = ROTATION_CCW_1;
        } else {
            rotation.phase = ROTATION_CW_1;
        }
        break;
    case ROTATION_CCW_1:
        rotation.phase = ROTATION_CCW_2;
        break;
    case ROTATION_CCW_2:
        rotation.phase = ROTATION_SETTLE;
        break;
    case ROTATION_CW_1:
        rotation.phase = ROTATION_CW_2;
        break;
    case ROTATION_CW_2:
        turn(1);
        rotation.phase = ROTATION_SETTLE;
        break;
    case ROTATION_SETTLE:
        return settled();
    default:
        rotation.over = 1;
        return 0;
    }
    return TURN_TICKS;
}

/* Right-aligned at x, as the firmware's number routine draws: 3x5 digits
   on a 4 px pitch. */
static void draw_number(uint8_t number, int x, int y)
{
    uint8_t tens = 0;

    /* At most 36: counting the tens is quicker than dividing. */
    for (; number >= 10; number -= 10)
        tens++;
    if (tens)
        lcd_blit_bitmap(x - 6, y, 3, 5, games_digit_glyphs + (uint8_t)(tens * 3));
    lcd_blit_bitmap(x - 2, y, 3, 5, games_digit_glyphs + (uint8_t)(number * 3));
}

/* The left and top edge of cell 0, less the number's offset in its cell. */
static int base(void)
{
    return 9 - rotation.size;
}

static void draw_cell(uint8_t x, uint8_t y, int dx, int dy)
{
    draw_number(rotation.board[x + ROTATION_STRIDE * y], (base() + 2 * x) * 7 - 13 + dx, (base() + 2 * y) * 4 - 10 + dy);
}

/* The frame at (fx, fy) and the numbers in it. With `framed` clear the
   frame is left out and the numbers stand in their cells; otherwise the
   numbers on its edge are drawn part of the way round, by the phase. */
static void draw_block(uint8_t fx, uint8_t fy, uint8_t framed)
{
    uint8_t last = (uint8_t)(rotation.ring + 1), x, y;
    int left = (base() + 2 * fx) * 7 - 21, top = (base() + 2 * fy - 3) * 4;
    int width = rotation.ring * CELL_WIDTH + 25, height = rotation.ring * CELL_HEIGHT + 16;
    int8_t slide_x = 0, slide_y = 0;

    lcd_fill_rect(left, top, width, height, framed);
    if (framed) {
        lcd_fill_rect(left + 1, top + 1, width - 2, height - 2, 0);
        if (rotation.phase == ROTATION_CCW_1 || rotation.phase == ROTATION_CW_2) {
            slide_x = 10;
            slide_y = 6;
        } else if (rotation.phase == ROTATION_CCW_2 || rotation.phase == ROTATION_CW_1) {
            slide_x = 4;
            slide_y = 2;
        }
    }
    for (x = 0; x <= last; x++)
        for (y = 0; y <= last; y++) {
            int8_t dx = 0, dy = 0;

            /* Each edge cell is shown on its way from the next one with
               the clock. */
            if (x == 0 && y > 0)
                dy = -1;
            else if (y == 0 && x < last)
                dx = 1;
            else if (x == last && y < last)
                dy = 1;
            else if (x > 0 && y == last)
                dx = -1;
            draw_cell(fx + x, fy + y, dx * slide_x, dy * slide_y);
        }
}

/* The solved board is shown without the frame, inverted. */
static void draw_solved(void)
{
    uint8_t n = rotation.size;

    lcd_fill_rect(base() * 7 - 21, (base() - 3) * 4, n * CELL_WIDTH - 3, n * CELL_HEIGHT + 1, LCD_INVERT);
}

void rotation_draw(void)
{
    uint8_t x, y, last = (uint8_t)(rotation.ring + 1);
    uint8_t solved_now = rotation.phase == ROTATION_SOLVED;

    for (x = 0; x < rotation.size; x++)
        for (y = 0; y < rotation.size; y++)
            if (solved_now || x < rotation.x || x > rotation.x + last || y < rotation.y || y > rotation.y + last)
                draw_cell(x, y, 0, 0);
    if (solved_now)
        draw_solved();
    else
        draw_block(rotation.x, rotation.y, 1);
    drawn_x = rotation.x;
    drawn_y = rotation.y;
}

void rotation_draw_changes(void)
{
    draw_block(drawn_x, drawn_y, 0);
    if (rotation.phase == ROTATION_SOLVED)
        draw_solved();
    else
        draw_block(rotation.x, rotation.y, 1);
    drawn_x = rotation.x;
    drawn_y = rotation.y;
}
