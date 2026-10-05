#include "snake2.h"

#include "game_assets.h"
#include "games.h"
#include "lcd.h"
#include "rand.h"
#include "snake2_data.h"
#include "sprite.h"

/* The board: 23 by 13 cells of 4x4 pixels, cell (x, y) at pixel
   (2 + 4x, 11 + 4y). The phone works the size and the offset out from the
   screen's: w = (96 - 6) / 4 + 1, h = (65 - 14) / 4 + 1, and the board is
   centred a pixel down in what is left. */
#define W 23
#define H 13
#define CELLS (W * H)
#define CELL_X(x) ((uint8_t)(2 + 4 * (x)))
#define CELL_Y(y) ((uint8_t)(11 + 4 * (y)))
#define FRAME_Y 9

/* The ring of segments, tail to head, counted modulo the board's cells as
   the phone's is. */
#define RING CELLS
#define NO_CELL 0xffff /* no segment */

/* A cell's number, x + W * y; more than a byte holds. */
typedef uint16_t cell_t;

/* Where the phone draws what is not on the board. Its digit pictures are
   the three lit columns of the 4x5 digits; here the digits are drawn whole,
   their blank first column a pixel to the left. */
#define DIGIT_W 4
#define DIGIT_H 5
#define SCORE_DIGITS 4
#define COUNTDOWN_UNITS_X 91
#define COUNTDOWN_TENS_X 87
#define ICON_X 78
#define ICON_Y 1
#define LINE_Y 6
#define HIDDEN 0xff /* a sprite's x when it is off the screen */

/* The dead snake blinks every BLINK_PERIOD ms, BLINKS times, and the game
   is over at the last. */
#define BLINK_PERIOD 500
#define BLINKS 8

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
    uint8_t blinking;      /* the dead snake's ticks blink it */
    uint8_t blink;
    uint8_t hidden;        /* the segments are blinked off */
    uint16_t period;       /* the level's tick, ms */
    uint8_t level;
    int8_t food_x, food_y; /* -1 when there is none */
    int8_t cells[4];       /* the creature's two cells, x and y; -1 when unused */
    /* The creature's sprite and the countdown's icon: picture and place. */
    uint8_t creature_pic, creature_px, creature_py;
    uint8_t icon_pic, countdown;
    uint8_t occupied[(H + 7) / 8 * W]; /* a bit per cell, walls and segments, in bands as the LCD */
    cell_t ring[RING];     /* cell of each segment */
    uint8_t picture[CELLS];
} s;

static struct game_context *game;

/* Drawing into sprite_screen, which is LCD_WIDTH columns of 8-row bands. */

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
    return SNAKE2_PICTURE_AT(SNAKE2_PICTURES_FIRST) + (uint16_t)(picture - 1) * SNAKE2_DESCRIPTOR;
}

/* A descriptor's width and height, and its bitmap. */
#define PICTURE_W(d) ((d)[1])
#define PICTURE_H(d) ((d)[3])

static void put_picture(uint8_t picture, uint8_t x, uint8_t y, uint8_t how)
{
    const uint8_t *d = descriptor(picture);
    /* The bitmaps lie within 64 KiB of the pictures' start. */
    const uint8_t *bitmap = snake2_pictures + (uint16_t)(((uint16_t)d[10] << 8 | d[11]) - (uint16_t)SNAKE2_PICTURES_BASE);

    put(x, y, PICTURE_W(d), PICTURE_H(d), bitmap, how);
}

/* A picture by set and index. */
#define PICTURE(set, index) ((uint8_t)(SNAKE2_PICTURE(set) + (index)))

static cell_t cell_of(int8_t x, int8_t y)
{
    return (cell_t)(x + W * y);
}

static void draw_cell(cell_t cell)
{
    uint8_t x = CELL_X(cell % W), y = CELL_Y(cell / W);

    put(x, y, 4, 4, 0, PUT_CLEAR);
    if (s.picture[cell] && !s.hidden)
        put_picture(s.picture[cell], x, y, PUT_OPAQUE);
}

static void set_picture(cell_t cell, uint8_t picture)
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
    put(px, py, PICTURE_W(d), PICTURE_H(d), 0, PUT_CLEAR);
    for (y = 0; y < PICTURE_H(d); y = (uint8_t)(y + 4))
        for (x = 0; x < PICTURE_W(d); x = (uint8_t)(x + 4))
            draw_cell(cell_of((int8_t)((px + x - 2) / 4), (int8_t)((py + y - 11) / 4)));
}

static void draw_digit(uint8_t x, uint8_t y, uint8_t digit)
{
    put(x, y, DIGIT_W, DIGIT_H, game_digit_glyphs + 4 * digit, PUT_OPAQUE);
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

    for (i = 0; i < 4; i++)
        s.cells[i] = -1;
}

/* A maze record and its runs of wall. The pointer lies within 64 KiB of
   the mazes' start. */
static const uint8_t *maze_record(void)
{
    return SNAKE2_MAZE_AT(SNAKE2_MAZES) + (uint16_t)(game->option - 1) * SNAKE2_MAZE_RECORD;
}

static const uint8_t *maze_runs(const uint8_t *maze)
{
    return snake2_mazes + (uint16_t)(((uint16_t)maze[2] << 8 | maze[3]) - (uint16_t)SNAKE2_MAZES_BASE);
}

#define MAZE_RUNS(maze) ((maze)[10])

/* The frame round the board, then a 2-pixel line through the middle of
   the cells of each run of wall. */
static void draw_walls(const uint8_t *maze)
{
    const uint8_t *run;
    uint8_t n;

    fill(0, FRAME_Y, 4 * W + 4, 1);
    fill(0, FRAME_Y, 1, 4 * H + 4);
    fill(4 * W + 3, FRAME_Y, 1, 4 * H + 4);
    fill(0, FRAME_Y + 4 * H + 3, 4 * W + 4, 1);
    run = maze_runs(maze);
    for (n = MAZE_RUNS(maze); n; n--, run += 4) {
        int8_t x1 = (int8_t)run[0], y1 = (int8_t)run[1], x2 = (int8_t)run[2], y2 = (int8_t)run[3];

        if (y1 == y2)
            fill((uint8_t)(4 * x1 + 3), (uint8_t)(4 * y1 + 12), (uint8_t)(4 * (x2 - x1) + 2), 2);
        else if (x1 == x2)
            fill((uint8_t)(4 * x1 + 3), (uint8_t)(4 * y1 + 12), 2, (uint8_t)(4 * (y2 - y1) + 2));
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

    for (n = MAZE_RUNS(maze); n; n--, run += 4) {
        int8_t x1 = (int8_t)run[0], y1 = (int8_t)run[1], x2 = (int8_t)run[2], y2 = (int8_t)run[3];

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

static uint8_t pixel_x(cell_t cell)
{
    return cell == NO_CELL ? HIDDEN : CELL_X(cell % W);
}

static uint8_t pixel_y(cell_t cell)
{
    return cell == NO_CELL ? HIDDEN : CELL_Y(cell / W);
}

/* The way from one segment to the next, as the phone works it out from
   where their sprites are: a jump of more than a cell is the way round
   the edge. The segment it leaves off the screen therefore counts as left
   of everything. */
static uint8_t way(cell_t from, cell_t to)
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
    cell_t old = s.ring[s.tail], now;

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
    return (uint8_t)((s.cells[0] == x && s.cells[1] == y) || (s.cells[2] == x && s.cells[3] == y));
}

/* The mouth opens when the cell ahead holds the food or the creature. */
static uint8_t mouth_open(void)
{
    int8_t x = step_x(s.head_x, s.dir), y = step_y(s.head_y, s.dir);

    return (uint8_t)((x == s.food_x && y == s.food_y) || on_creature(x, y));
}

/* The head moves a cell. `objects` is 0 for the steps a new game makes
   before there is food. Returns 100 when the ring is full. */
static uint8_t head_step(uint8_t objects)
{
    uint8_t result = 0, prev = s.prev_dir, dir = s.dir, picture;
    cell_t cell, neck;

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
        /* The cell on the far side, going across, is let go. */
        if (dir == LEFT)
            s.cells[0] = s.cells[1] = -1;
        else if (dir == RIGHT)
            s.cells[2] = s.cells[3] = -1;
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

/* Food and creatures. The phone draws on the ANSI C generator here. */

#define FOOD_TRIES 50
#define CREATURE_TRIES 50

static uint8_t random_below(uint8_t n)
{
    return (uint8_t)((unsigned)game_rand() % n);
}

static uint8_t clear_of_creature(int8_t x, int8_t y)
{
    return (uint8_t)(x != s.cells[0] && x != s.cells[2] && y != s.cells[1] && y != s.cells[3]);
}

static void place_food(void)
{
    uint8_t tries;
    int8_t x, y;

    if (s.food_x >= 0) {
        put(CELL_X(s.food_x), CELL_Y(s.food_y), 3, 3, 0, PUT_CLEAR);
        draw_cell(cell_of(s.food_x, s.food_y));
    }
    for (tries = FOOD_TRIES; tries; tries--) {
        x = (int8_t)random_below(W);
        y = (int8_t)random_below(H);
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

/* A new creature, two cells across. Returns 1 when it found a place. When
   it finds none its cells keep the last place tried, as on the phone, and
   no creature comes again in that game. */
static uint8_t spawn_creature(void)
{
    uint8_t tries;
    int8_t *c = s.cells;

    for (tries = CREATURE_TRIES; tries; tries--) {
        c[0] = (int8_t)random_below(W);
        c[1] = (int8_t)random_below(H);
        c[2] = (int8_t)(c[0] + 1);
        c[3] = c[1];
        if (c[2] != W && !occupied(c[0], c[1]) && !occupied(c[2], c[3]) && c[0] != s.food_x && c[1] != s.food_y
            && c[2] != s.food_x)
            break;
    }
    if (!tries) {
        hide_creature();
        return 0;
    }
    take_away(s.creature_pic, s.creature_px, s.creature_py);
    s.creature_px = HIDDEN;
    s.creature_pic = s.icon_pic = PICTURE(SNAKE2_CREATURES, random_below(6));
    move_creature(CELL_X(c[0]), CELL_Y(c[1]));
    return 1;
}

/* The picture from nothing: frame, walls, segments, food, creature and
   what is above the board. */
void snake2_redraw(void)
{
    uint16_t i;
    cell_t cell;

    for (i = 0; i < sizeof sprite_screen; i++)
        sprite_screen[i] = 0;
    draw_walls(maze_record());
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
    const uint8_t *maze = maze_record();
    uint16_t i;
    uint8_t n;
    cell_t start;

    s.level = game->level;
    s.period = (uint16_t)(snake2_speeds[s.level - 1] * 10);
    game->period = s.period;
    game->one_shot = 0;
    game->score = 0;
    s.dir = s.prev_dir = RIGHT;
    s.head_x = s.tail_x = (int8_t)maze[4];
    s.head_y = s.tail_y = (int8_t)maze[5];
    s.head = s.tail = 0;
    s.crash = CRASH_NONE;
    s.mode = 1;
    s.blinking = 0;
    s.blink = 0;
    s.hidden = 0;
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

    /* The first segment is made on the start cell; seven steps right and
       the first tail move leave seven segments to its right. */
    occupy(s.head_x, s.head_y, 1);
    start = cell_of(s.head_x, s.head_y);
    s.ring[s.head] = start;
    set_picture(start, PICTURE(SNAKE2_CORNERS, 0));
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

/* The dead snake: every segment on or off, the ring gone round or not. */
static void blink_segments(void)
{
    cell_t cell;

    s.hidden ^= 1;
    for (cell = 0; cell < CELLS; cell++)
        if (s.picture[cell])
            draw_cell(cell);
}

static int tick(struct game_context *ctx)
{
    uint8_t recovered, eaten = 0;

    if (s.crash == CRASH_DEAD) {
        /* The tick after the death starts the blinking; the last blink
           ends the game. */
        if (!s.blinking) {
            s.blinking = 1;
            s.blink = 0;
            ctx->period = BLINK_PERIOD;
            return GAME_RESULT_RESTART_TICK;
        }
        /* The phone blinks the snake once more as the game ends, but puts
           the title up before the LCD shows it. */
        if (++s.blink == BLINKS) {
            ctx->sound = 0;
            return GAME_RESULT_GAME_OVER;
        }
        blink_segments();
        return GAME_RESULT_REDRAW;
    }
    if (s.mode != 1)
        return GAME_RESULT_NONE;
    s.dir = s.pending;
    if (blocked()) {
        if (s.crash == CRASH_NONE) {
            s.crash = CRASH_BLOCKED;
            ctx->period = 100;
            return GAME_RESULT_RESTART_TICK;
        }
        games_vibrate();
        s.crash = CRASH_DEAD;
        ctx->sound = SNAKE2_SOUND_DEATH;
        return GAME_RESULT_SOUND;
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
        s.crash = CRASH_DEAD;
        return GAME_RESULT_REDRAW;
    }

    if (s.creature_out) {
        if (on_creature(s.head_x, s.head_y)) {
            eaten = 1;
        } else if (s.counter && s.cells[0] >= 0 && s.cells[1] >= 0) {
            if (!--s.counter) {
                clear_cells();
                hide_creature();
                s.creature_out = 0;
            }
            draw_countdown(s.counter);
        }
    }
    /* A creature makes the snake grow as food does. */
    s.grow = (uint8_t)((s.head_x == s.food_x && s.head_y == s.food_y) || eaten);
    if (!s.grow)
        return recovered ? GAME_RESULT_RESTART_TICK : GAME_RESULT_REDRAW;

    if (eaten) {
        ctx->score += (uint32_t)s.counter * 2 + (uint32_t)ctx->level * 5 + 5;
        clear_cells();
        hide_creature();
        s.counter = 0;
        s.creature_out = 0;
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
    games_vibrate();
    ctx->sound = SNAKE2_SOUND_EAT;
    s.crash = CRASH_NONE;
    return recovered ? GAME_RESULT_RESTART_TICK : GAME_RESULT_SOUND;
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
    case GAME_EVENT_RESUME:
        ctx->period = s.crash == CRASH_DEAD && s.blinking ? BLINK_PERIOD : s.crash == CRASH_BLOCKED ? 100 : s.period;
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
