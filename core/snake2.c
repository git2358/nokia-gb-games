#include "snake2.h"

#include "game_assets.h"
#include "games.h"
#include "lcd.h"
#include "rand.h"
#include "snake2_data.h"
#include "sprite.h"

/* The board: 20 by 9 cells of 4x4 pixels, cell (x, y) at pixel
   (2 + 4x, 10 + 4y). The phone works the size out from the screen's. */
#define W 20
#define H 9
#define CELLS (W * H)
#define CELL_X(x) ((uint8_t)(2 + 4 * (x)))
#define CELL_Y(y) ((uint8_t)(10 + 4 * (y)))

/* The ring of segments, tail to head, counted modulo 299 as the phone's
   is: when the head index has gone round past the tail's, the dying snake
   no longer blinks. */
#define RING 299
#define NO_CELL 0xff /* the segment the phone leaves off the screen */

/* Where the phone draws what is not on the board. */
#define DIGIT_W 4
#define DIGIT_H 5
#define SCORE_DIGITS 4
#define COUNTDOWN_UNITS_X 79
#define COUNTDOWN_TENS_X 75
#define ICON_X 66
#define ICON_Y 1
#define LINE_Y 6
#define HIDDEN 0xff /* a sprite's x when it is off the screen */

enum {
    LEFT,
    UP,
    RIGHT,
    DOWN
};

enum {
    CRASH_NONE,
    CRASH_BLOCKED, /* stood still for one short tick, which a turn can save */
    CRASH_DEAD
};

uint16_t snake2_mask;

static struct {
    uint16_t tail, head;   /* ring places */
    int8_t head_x, head_y; /* cells */
    int8_t tail_x, tail_y;
    uint8_t dir, prev_dir, pending;
    uint8_t grow;          /* the head was on the food after the last step: the tail stays */
    uint8_t swallow;       /* draw the next neck fat */
    uint8_t counter;       /* +4 a meal until a creature comes, then its ticks left */
    uint8_t creature_out;
    uint8_t crash;
    uint8_t mode;          /* 1 once a game has started */
    uint8_t blink;
    uint8_t hidden;        /* the segments are blinked off */
    uint8_t big_out, big_frame, big_eaten;
    uint16_t period;       /* the level's tick, ms */
    uint8_t level;
    int8_t food_x, food_y; /* -1 when there is none */
    int8_t cells[8];       /* the creature's four cells, x and y; -1 when unused */
    /* The creature's sprite and the countdown's icon: picture and place. */
    uint8_t creature_pic, creature_px, creature_py;
    uint8_t icon_pic, countdown;
    uint8_t occupied[(H + 7) / 8 * W]; /* a bit per cell, walls and segments, in bands as the LCD */
    uint8_t ring[RING];    /* cell of each segment */
    uint8_t picture[CELLS];
} s;

static struct game_context *game;

/* Drawing into sprite_screen, which is 84 columns of 8-row bands. */

enum {
    PUT_SET,    /* set bits set: the phone's mode 1 */
    PUT_OPAQUE, /* set bits set, clear bits clear: mode 4 */
    PUT_CLEAR,  /* the rectangle cleared: a sprite taken away */
    PUT_FILL    /* the rectangle set */
};

/* A bitmap of up to 8 rows, a byte per column. */
static void put(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bits, uint8_t how)
{
    uint8_t shift = y & 7, rows = (uint8_t)((1u << h) - 1), i;
    uint16_t mask = (uint16_t)(rows << shift);
    uint8_t *p;

    if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
        return;
    p = sprite_screen + (y >> 3) * LCD_WIDTH + x;
    for (i = 0; i < w && x + i < LCD_WIDTH; i++, p++) {
        uint16_t v = how == PUT_FILL ? mask : how == PUT_CLEAR ? 0 : (uint16_t)((bits[i] & rows) << shift);

        if (how != PUT_SET) {
            p[0] &= (uint8_t)~mask;
            if (y < LCD_HEIGHT - 8)
                p[LCD_WIDTH] &= (uint8_t)~(mask >> 8);
        }
        p[0] |= (uint8_t)v;
        if (y < LCD_HEIGHT - 8)
            p[LCD_WIDTH] |= (uint8_t)(v >> 8);
    }
}

/* A filled rectangle of any height. */
static void fill(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    while (h) {
        uint8_t n = (uint8_t)(8 - (y & 7));

        if (n > h)
            n = h;
        put(x, y, w, n, 0, PUT_FILL);
        y = (uint8_t)(y + n);
        h = (uint8_t)(h - n);
    }
}

static const uint8_t *descriptor(uint8_t picture)
{
    return SNAKE2_ROM(SNAKE2_REF(SNAKE2_PICTURES_FIRST)) + (uint16_t)(picture - 1) * 12;
}

static void put_picture(uint8_t picture, uint8_t x, uint8_t y, uint8_t how)
{
    const uint8_t *d = descriptor(picture);
    /* The bitmaps lie within 64 KiB of the data's start. */
    snake2_ref bitmap = (snake2_ref)(((uint16_t)d[2] << 8 | d[3]) - (uint16_t)SNAKE2_DATA_BASE);

    put(x, y, d[8], d[9], SNAKE2_ROM(bitmap), how);
}

/* A picture by set and index. */
#define PICTURE(set, index) ((uint8_t)(SNAKE2_PICTURE(set) + (index)))

static uint8_t cell_of(int8_t x, int8_t y)
{
    return (uint8_t)(x + W * y);
}

static void draw_cell(uint8_t cell)
{
    uint8_t x = CELL_X(cell % W), y = CELL_Y(cell / W);

    put(x, y, 4, 4, 0, PUT_CLEAR);
    if (s.picture[cell] && !s.hidden)
        put_picture(s.picture[cell], x, y, PUT_OPAQUE);
}

static void set_picture(uint8_t cell, uint8_t picture)
{
    if (cell == NO_CELL)
        return;
    s.picture[cell] = picture;
    draw_cell(cell);
}

/* Takes a sprite away: clears where it was and draws again the cells it
   was over. */
static void take_away(uint8_t picture, uint8_t px, uint8_t py)
{
    const uint8_t *d;
    uint8_t x, y;

    if (px == HIDDEN)
        return;
    d = descriptor(picture);
    put(px, py, d[8], d[9], 0, PUT_CLEAR);
    for (y = 0; y < d[9]; y = (uint8_t)(y + 4))
        for (x = 0; x < d[8]; x = (uint8_t)(x + 4))
            draw_cell(cell_of((int8_t)((px + x - 2) / 4), (int8_t)((py + y - 10) / 4)));
}

static void draw_digit(uint8_t x, uint8_t y, uint8_t digit)
{
    put(x, y, DIGIT_W, DIGIT_H, si_digit_glyphs + 4 * digit, PUT_OPAQUE);
}

static void draw_score(void)
{
    uint16_t value = (uint16_t)game->score;
    uint8_t i = SCORE_DIGITS;

    while (i--) {
        draw_digit((uint8_t)(i * DIGIT_W), 0, (uint8_t)(value % 10));
        value /= 10;
    }
}

/* The creature's countdown and icon, shown while the value is above 0. */
static void draw_countdown(uint8_t value)
{
    s.countdown = value;
    if (!value) {
        put(COUNTDOWN_UNITS_X, 0, DIGIT_W, DIGIT_H, 0, PUT_CLEAR);
        put(COUNTDOWN_TENS_X, 0, DIGIT_W, DIGIT_H, 0, PUT_CLEAR);
        put(ICON_X, ICON_Y, 8, 4, 0, PUT_CLEAR);
        return;
    }
    draw_digit(COUNTDOWN_UNITS_X, 0, (uint8_t)(value % 10));
    draw_digit(COUNTDOWN_TENS_X, 0, (uint8_t)(value / 10 % 10));
    put_picture(s.icon_pic, ICON_X, ICON_Y, PUT_OPAQUE);
}

static void draw_food(void)
{
    if (s.food_x >= 0)
        put_picture(PICTURE(SNAKE2_FOOD, 0), CELL_X(s.food_x), CELL_Y(s.food_y), PUT_SET);
}

static void move_creature(uint8_t px, uint8_t py)
{
    take_away(s.creature_pic, s.creature_px, s.creature_py);
    s.creature_px = px;
    s.creature_py = py;
    if (px != HIDDEN)
        put_picture(s.creature_pic, px, py, PUT_SET);
}

static void hide_creature(void)
{
    move_creature(HIDDEN, HIDDEN);
}

static void clear_cells(void)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
        s.cells[i] = -1;
}

/* A maze record's runs of wall. The pointer lies within 64 KiB of the
   data's start. */
static const uint8_t *maze_runs(const uint8_t *maze)
{
    return SNAKE2_ROM((snake2_ref)(((uint16_t)maze[6] << 8 | maze[7]) - (uint16_t)SNAKE2_DATA_BASE));
}

/* The frame round the board, then a 2-pixel line through the middle of
   the cells of each run of wall. */
static void draw_walls(const uint8_t *maze)
{
    const uint8_t *run;
    uint8_t n;

    fill(0, 8, 4 * W + 3, 1);
    fill(0, 8, 1, 4 * H + 3);
    fill(4 * W + 3, 8, 1, 4 * H + 4);
    fill(0, 4 * H + 11, 4 * W + 3, 1);
    run = maze_runs(maze);
    for (n = maze[0]; n; n--, run += 8) {
        int8_t x1 = (int8_t)run[0], y1 = (int8_t)run[1], x2 = (int8_t)run[4], y2 = (int8_t)run[5];

        /* No maze is one run at (-1, -1), which lands off the screen. */
        if (y1 == y2)
            fill((uint8_t)(4 * x1 + 3), (uint8_t)(4 * y1 + 11), (uint8_t)(4 * (x2 - x1) + 2), 2);
        else if (x1 == x2)
            fill((uint8_t)(4 * x1 + 3), (uint8_t)(4 * y1 + 11), 2, (uint8_t)(4 * (y2 - y1) + 2));
    }
}

/* Occupancy. */

static uint8_t occupied(int8_t x, int8_t y)
{
    return (uint8_t)(s.occupied[(y >> 3) * W + x] >> (y & 7) & 1);
}

static void occupy(int8_t x, int8_t y, uint8_t on)
{
    uint8_t *p, bit;

    if (x < 0 || y < 0)
        return;
    p = &s.occupied[(y >> 3) * W + x];
    bit = (uint8_t)(1 << (y & 7));
    if (on)
        *p |= bit;
    else
        *p &= (uint8_t)~bit;
}

static void build_maze(const uint8_t *maze)
{
    const uint8_t *run = maze_runs(maze);
    uint8_t n;
    int8_t i;

    for (n = maze[0]; n; n--, run += 8) {
        int8_t x1 = (int8_t)run[0], y1 = (int8_t)run[1], x2 = (int8_t)run[4], y2 = (int8_t)run[5];

        if (y1 == y2)
            for (i = x1; i <= x2; i++)
                occupy(i, y1, 1);
        else if (x1 == x2)
            for (i = y1; i <= y2; i++)
                occupy(x1, i, 1);
    }
    draw_walls(maze);
}

/* Steps. */

static int8_t step_x(int8_t x, uint8_t dir)
{
    return (int8_t)((W + x + (dir == LEFT ? -1 : dir == RIGHT ? 1 : 0)) % W);
}

static int8_t step_y(int8_t y, uint8_t dir)
{
    return (int8_t)((H + y + (dir == UP ? -1 : dir == DOWN ? 1 : 0)) % H);
}

static uint8_t pixel_x(uint8_t cell)
{
    return cell == NO_CELL ? HIDDEN : CELL_X(cell % W);
}

static uint8_t pixel_y(uint8_t cell)
{
    return cell == NO_CELL ? HIDDEN : CELL_Y(cell / W);
}

/* The way from one segment to the next, as the phone works it out from
   where their sprites are: a jump of more than a cell is the way round
   the edge. The segment it leaves off the screen therefore counts as left
   of everything. */
static uint8_t way(uint8_t from, uint8_t to)
{
    int16_t dx = (int16_t)pixel_x(to) - pixel_x(from), dy;

    if (dx == 0) {
        dy = (int16_t)pixel_y(to) - pixel_y(from);
        if (dy < 1)
            return dy >= -4 ? UP : DOWN;
        return dy > 4 ? UP : DOWN;
    }
    if (dx < 1)
        return dx >= -4 ? LEFT : RIGHT;
    return dx > 4 ? LEFT : RIGHT;
}

static uint16_t ring_next(uint16_t place)
{
    return place == RING - 1 ? 0 : (uint16_t)(place + 1);
}

/* Moves the tail on, picturing the new tail by the way to the segment
   after it. */
static void tail_remove(void)
{
    uint8_t old = s.ring[s.tail], now;

    s.tail = ring_next(s.tail);
    now = s.ring[s.tail];
    occupy(s.tail_x, s.tail_y, 0);
    if (old != NO_CELL)
        set_picture(old, 0);
    switch (way(old, now)) {
    case LEFT:
        s.tail_x = step_x(s.tail_x, LEFT);
        break;
    case UP:
        s.tail_y = step_y(s.tail_y, UP);
        break;
    case RIGHT:
        s.tail_x = step_x(s.tail_x, RIGHT);
        break;
    default:
        s.tail_y = step_y(s.tail_y, DOWN);
        break;
    }
    set_picture(now, PICTURE(SNAKE2_TAILS, way(now, s.ring[ring_next(s.tail)])));
}

/* The corner a turn makes, by the way before and the way after: 0 up then
   right or left then down, 1 up then left or right then down, 2 left then
   up or down then right, 3 right then up or down then left. */
static const uint8_t corners[4][4] = {
    { 0xff, 2, 0xff, 0 },
    { 1, 0xff, 0, 0xff },
    { 0xff, 3, 0xff, 1 },
    { 3, 0xff, 2, 0xff }
};

static uint8_t on_creature(int8_t x, int8_t y)
{
    uint8_t i;

    for (i = 0; i < 8; i += 2)
        if (s.cells[i] == x && s.cells[i + 1] == y)
            return 1;
    return 0;
}

/* The mouth opens when the cell ahead holds the food or the creature. */
static uint8_t mouth_open(void)
{
    int8_t x = step_x(s.head_x, s.dir), y = step_y(s.head_y, s.dir);

    return (uint8_t)((x == s.food_x && y == s.food_y) || on_creature(x, y));
}

/* The head moves a cell. `objects` is 0 for the steps a new game makes
   before there is food, which the phone checks against whatever it had
   then. Returns 100 when the ring is full. */
static uint8_t head_step(uint8_t objects)
{
    uint8_t result = 0, cell, neck, prev = s.prev_dir, dir = s.dir, picture;

    if ((uint16_t)((s.head + 2) % RING) == s.tail)
        result = 100;
    else if (ring_next(s.head) == s.tail)
        tail_remove();
    s.head_x = step_x(s.head_x, dir);
    s.head_y = step_y(s.head_y, dir);
    occupy(s.head_x, s.head_y, 1);
    cell = cell_of(s.head_x, s.head_y);
    s.head = ring_next(s.head);
    s.ring[s.head] = cell;
    set_picture(cell, PICTURE(SNAKE2_HEADS, dir));
    if (objects && on_creature(s.head_x, s.head_y)) {
        hide_creature();
        /* The cells behind, on the side it came from, are let go. */
        switch (dir) {
        case LEFT:
            s.cells[0] = s.cells[1] = s.cells[4] = s.cells[5] = -1;
            break;
        case UP:
            s.cells[0] = s.cells[1] = s.cells[2] = s.cells[3] = -1;
            break;
        case RIGHT:
            s.cells[2] = s.cells[3] = s.cells[6] = s.cells[7] = -1;
            break;
        default:
            s.cells[4] = s.cells[5] = s.cells[6] = s.cells[7] = -1;
            break;
        }
    }
    if (objects && mouth_open())
        set_picture(cell, PICTURE(SNAKE2_HEADS_OPEN, dir));

    neck = s.ring[(uint16_t)((RING + s.head - 1) % RING)];
    if (prev == dir)
        picture = s.swallow ? PICTURE(SNAKE2_BODY_FAT, dir) : PICTURE(SNAKE2_BODY, dir);
    else if (corners[prev][dir] != 0xff)
        picture = PICTURE(s.swallow ? SNAKE2_CORNERS_FAT : SNAKE2_CORNERS, corners[prev][dir]);
    else
        picture = 0; /* a turn back, which cannot happen: left as it was */
    if (picture)
        set_picture(neck, picture);
    s.prev_dir = dir;
    s.swallow = 0;
    return result;
}

static uint8_t blocked(void)
{
    int8_t x = step_x(s.head_x, s.dir), y = step_y(s.head_y, s.dir);

    if (x == s.tail_x && y == s.tail_y)
        return s.grow;
    return occupied(x, y);
}

/* Food and creatures. */

static uint8_t clear_of_creature(int8_t x, int8_t y)
{
    uint8_t i;

    for (i = 0; i < 8; i += 2)
        if (x == s.cells[i] || y == s.cells[i + 1])
            return 0;
    return 1;
}

static void place_food(void)
{
    uint8_t tries;
    int8_t x, y;

    if (s.food_x >= 0) {
        put(CELL_X(s.food_x), CELL_Y(s.food_y), 3, 3, 0, PUT_CLEAR);
        draw_cell(cell_of(s.food_x, s.food_y));
    }
    for (tries = 100; tries; tries--) {
        x = (int8_t)(game_rand16() % W);
        y = (int8_t)(game_rand16() % H);
        if (!occupied(x, y) && clear_of_creature(x, y))
            goto found;
    }
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            if (!occupied(x, y))
                goto found;
    s.food_x = s.food_y = -1;
    return;
found:
    s.food_x = x;
    s.food_y = y;
    draw_food();
}

static uint8_t clear_of_food(int8_t x, int8_t y)
{
    return (uint8_t)(x != s.food_x && y != s.food_y);
}

/* A new creature, small or once in fifty the large one. Returns 1 when it
   found a place. When it finds none its cells keep the last place tried,
   as on the phone, and no creature comes again in that game. */
static uint8_t spawn_creature(void)
{
    uint8_t tries, kind;
    int8_t *c = s.cells;

    clear_cells();
    if (game_rand16() % 50 == 0 && (snake2_mask & 0x8000) && s.mode == 1 && !s.big_eaten) {
        for (tries = 100; tries; tries--) {
            c[0] = (int8_t)(game_rand16() % (W - 1));
            c[1] = (int8_t)(game_rand16() % (H - 1));
            c[2] = (int8_t)(c[0] + 1);
            c[3] = c[1];
            c[4] = c[0];
            c[5] = (int8_t)(c[1] + 1);
            c[6] = c[2];
            c[7] = c[5];
            if (!occupied(c[0], c[1]) && !occupied(c[2], c[3]) && !occupied(c[4], c[5]) && !occupied(c[6], c[7])
                && clear_of_food(c[0], c[1]) && clear_of_food(c[2], c[3]) && clear_of_food(c[4], c[5])
                && clear_of_food(c[6], c[7])) {
                s.creature_pic = PICTURE(SNAKE2_BIG, 0);
                s.icon_pic = PICTURE(SNAKE2_BIG_ICON, 0);
                s.big_out = 1;
                s.big_frame = 0;
                move_creature(CELL_X(c[0]), CELL_Y(c[1]));
                return 1;
            }
        }
        hide_creature();
        return 0;
    }
    for (tries = 100; tries; tries--) {
        c[0] = (int8_t)(game_rand16() % (W - 1));
        c[1] = (int8_t)(game_rand16() % H);
        c[2] = (int8_t)(c[0] + 1);
        c[3] = c[1];
        c[4] = c[0];
        c[5] = c[1];
        c[6] = c[2];
        c[7] = c[3];
        if (!occupied(c[0], c[1]) && !occupied(c[2], c[3]) && clear_of_food(c[0], c[1]) && clear_of_food(c[2], c[3]))
            break;
    }
    kind = (uint8_t)(game_rand16() % 6);
    take_away(s.creature_pic, s.creature_px, s.creature_py);
    s.creature_px = HIDDEN;
    s.creature_pic = s.icon_pic = PICTURE(SNAKE2_CREATURES, kind);
    if (!tries)
        return 0;
    move_creature(CELL_X(c[0]), CELL_Y(c[1]));
    return 1;
}

/* A random collection bit not yet set; with all ten set the phone would
   look for ever. */
static void collect(void)
{
    uint16_t bit;

    do
        bit = (uint16_t)(1u << (game_rand16() % 10));
    while (snake2_mask & bit);
    snake2_mask |= bit;
    s.big_eaten = 1;
}

/* The picture from nothing: frame, walls, segments, food, creature and
   what is above the board. */
void snake2_redraw(void)
{
    const uint8_t *maze = SNAKE2_ROM(SNAKE2_REF(SNAKE2_MAZES) + (uint16_t)(game->option - 1) * 16);
    uint16_t i;
    uint8_t cell;

    for (i = 0; i < sizeof sprite_screen; i++)
        sprite_screen[i] = 0;
    draw_walls(maze);
    fill(0, LINE_Y, LCD_WIDTH, 1);
    for (cell = 0; cell < CELLS; cell++)
        if (s.picture[cell] && !s.hidden)
            put_picture(s.picture[cell], CELL_X(cell % W), CELL_Y(cell / W), PUT_OPAQUE);
    draw_food();
    if (s.creature_px != HIDDEN)
        put_picture(s.creature_pic, s.creature_px, s.creature_py, PUT_SET);
    draw_score();
    draw_countdown(s.countdown);
}

static void new_game(void)
{
    const uint8_t *maze = SNAKE2_ROM(SNAKE2_REF(SNAKE2_MAZES) + (uint16_t)(game->option - 1) * 16);
    uint16_t i;
    uint8_t n;

    s.level = game->level;
    s.period = (uint16_t)(SNAKE2_ROM(SNAKE2_REF(SNAKE2_SPEEDS))[s.level - 1] * 10);
    game->period = s.period;
    game->one_shot = 0;
    game->score = 0;
    s.dir = s.prev_dir = RIGHT;
    s.head_x = s.tail_x = (int8_t)maze[8];
    s.head_y = s.tail_y = (int8_t)maze[9];
    s.head = s.tail = (uint16_t)maze[8];
    s.crash = CRASH_NONE;
    s.mode = 1;
    s.blink = 0;
    s.hidden = 0;
    s.big_out = s.big_frame = 0;
    s.swallow = 0;
    for (i = 0; i < sizeof s.occupied; i++)
        s.occupied[i] = 0;
    for (i = 0; i < RING; i++)
        s.ring[i] = 0;
    for (i = 0; i < CELLS; i++)
        s.picture[i] = 0;
    s.food_x = s.food_y = -1;
    clear_cells();
    s.creature_px = HIDDEN;
    s.creature_pic = s.icon_pic = PICTURE(SNAKE2_CREATURES, 0);
    s.countdown = 0;
    /* The picture starts blank but for the walls. */
    for (i = 0; i < sizeof sprite_screen; i++)
        sprite_screen[i] = 0;
    build_maze(maze);

    /* The first segment is made off the screen on the start cell; seven
       steps right and the first tail move leave seven segments to its
       right. */
    occupy(s.head_x, s.head_y, 1);
    s.ring[s.head] = NO_CELL;
    for (n = 0; n < 7; n++)
        head_step(0);
    tail_remove();
    s.pending = s.dir;
    s.grow = 0;

    /* The food starts in the middle and is then placed at random. */
    s.food_x = W / 2;
    s.food_y = H / 2;
    draw_food();
    place_food();
    s.counter = 0;
    s.creature_out = 0;
    draw_score();
    fill(0, LINE_Y, LCD_WIDTH, 1);
}

static int tick(struct game_context *ctx)
{
    uint8_t recovered, eaten = 0, food;

    if (s.crash == CRASH_DEAD) {
        ctx->sound = 0;
        games_vibrate();
        return GAME_RESULT_GAME_OVER;
    }
    if (s.mode != 1)
        return GAME_RESULT_NONE;
    if (s.big_out) {
        s.big_frame ^= 1;
        take_away(s.creature_pic, s.creature_px, s.creature_py);
        s.creature_pic = PICTURE(SNAKE2_BIG, s.big_frame);
        if (s.creature_px != HIDDEN)
            put_picture(s.creature_pic, s.creature_px, s.creature_py, PUT_SET);
        s.icon_pic = PICTURE(SNAKE2_BIG_ICON, s.big_frame);
        if (s.countdown)
            put_picture(s.icon_pic, ICON_X, ICON_Y, PUT_OPAQUE);
    }
    s.dir = s.pending;
    if (blocked()) {
        if (s.crash == CRASH_NONE) {
            s.crash = CRASH_BLOCKED;
            ctx->period = 100;
            return GAME_RESULT_RESTART_TICK;
        }
        games_vibrate();
        s.crash = CRASH_DEAD;
        ctx->period = 2100;
        ctx->sound = SNAKE2_SOUND_DEATH;
        return GAME_RESULT_ONE_SHOT_SOUND;
    }
    recovered = s.crash != CRASH_NONE;
    if (recovered) {
        s.crash = CRASH_NONE;
        ctx->period = s.period;
    }
    if (!s.grow)
        tail_remove();
    if (head_step(1)) {
        ctx->score += 100;
        draw_score();
        ctx->sound = SNAKE2_SOUND_FULL;
        return GAME_RESULT_GAME_OVER;
    }

    if (s.creature_out) {
        if (on_creature(s.head_x, s.head_y)) {
            eaten = 1;
        } else if (s.counter && s.cells[0] >= 0 && s.cells[1] >= 0) {
            if (!--s.counter) {
                clear_cells();
                hide_creature();
                s.creature_out = 0;
                s.big_out = 0;
            }
            draw_countdown(s.counter);
        }
    }
    food = (uint8_t)(s.head_x == s.food_x && s.head_y == s.food_y);
    s.grow = food;
    if (!food && !eaten)
        return recovered ? GAME_RESULT_RESTART_TICK : GAME_RESULT_REDRAW;

    games_vibrate();
    if (eaten) {
        ctx->score += (uint32_t)s.counter * 2 + (uint32_t)ctx->level * 5 + 5;
        clear_cells();
        hide_creature();
        s.counter = 0;
        s.creature_out = 0;
        if (s.big_out) {
            collect();
            s.big_out = 0;
            s.big_frame = 0;
        }
        draw_countdown(0);
    } else {
        ctx->score += ctx->level;
        place_food();
        if (!s.creature_out)
            s.counter = (uint8_t)(s.counter + 4);
        if (s.counter >= 20 && s.cells[0] < 0 && s.cells[1] < 0) {
            if (spawn_creature())
                s.creature_out = 1;
            else
                s.counter = 0;
            draw_countdown(s.counter);
        }
    }
    s.swallow = 1;
    draw_score();
    ctx->sound = SNAKE2_SOUND_EAT;
    s.crash = CRASH_NONE;
    return recovered ? GAME_RESULT_RESTART_TICK : GAME_RESULT_SOUND;
}

/* The dead snake blinks: every segment on or off, unless the ring has
   gone round so that the tail's place is past the head's. */
static int blink(struct game_context *ctx)
{
    uint8_t cell;

    if (s.blink > 10)
        return GAME_RESULT_ONE_SHOT;
    if (s.tail <= s.head) {
        s.hidden ^= 1;
        for (cell = 0; cell < CELLS; cell++)
            if (s.picture[cell])
                draw_cell(cell);
    }
    ctx->one_shot = 250;
    if (++s.blink == 10) {
        ctx->one_shot = 0;
        ctx->period = s.period;
        s.blink = 0;
        return GAME_RESULT_RESTART_TICK;
    }
    return GAME_RESULT_ONE_SHOT;
}

static uint8_t vertical(void)
{
    return (uint8_t)(s.dir == UP || s.dir == DOWN);
}

int snake2_handler(int event, struct game_context *ctx)
{
    game = ctx;
    switch (event) {
    case GAME_EVENT_START:
        new_game();
        return GAME_RESULT_NONE;
    case GAME_EVENT_TICK:
        return tick(ctx);
    case GAME_EVENT_TIMER:
        return blink(ctx);
    case GAME_EVENT_RESUME:
        ctx->period = s.crash == CRASH_DEAD ? 0 : s.period;
        ctx->one_shot = 0;
        snake2_redraw();
        return GAME_RESULT_NONE;
    case GAME_KEY_2:
        if (s.dir != DOWN)
            s.pending = UP;
        break;
    case GAME_KEY_8:
        if (s.dir != UP)
            s.pending = DOWN;
        break;
    case GAME_KEY_4:
        if (s.dir != RIGHT)
            s.pending = LEFT;
        break;
    case GAME_KEY_6:
        if (s.dir != LEFT)
            s.pending = RIGHT;
        break;
    case GAME_KEY_1:
        s.pending = vertical() ? LEFT : UP;
        break;
    case GAME_KEY_3:
        s.pending = vertical() ? RIGHT : UP;
        break;
    case GAME_KEY_7:
        s.pending = vertical() ? LEFT : DOWN;
        break;
    case GAME_KEY_9:
        s.pending = vertical() ? RIGHT : DOWN;
        break;
    case GAME_KEY_HASH:
    case GAME_KEY_SCROLL_DOWN:
        s.pending = (uint8_t)((s.dir + 1) & 3);
        break;
    case GAME_KEY_STAR:
    case GAME_KEY_SCROLL_UP:
        s.pending = (uint8_t)((s.dir + 3) & 3);
        break;
    default:
        break;
    }
    return GAME_RESULT_NONE;
}
