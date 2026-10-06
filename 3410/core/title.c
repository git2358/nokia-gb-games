#include "title.h"

#include <string.h>

#include "game_assets.h"
#include "games.h"
#include "lcd.h"
#include "snake2_data.h"
#include "sprite.h"

/* The title's pictures: 96x65 in the LCD's band layout, the first at
   SNAKE2_TITLE_BASE and the five laid over it from TITLE_FRAMES on, 0x300
   bytes apart. A picture is nine bands, so the last row of each is read
   from the next, as on the phone. */
#define TITLE_FRAMES 0x4977a0ul
#define TITLE_FRAME_STEP 0x300u
#define TITLE_FRAME_COUNT 5
#define PICTURE(address) (snake2_title + (uint16_t)((address) - SNAKE2_TITLE_BASE))

/* Every 200 ms of the phone's timer the title moves on a step: steps 1 to
   5 lay the five pictures over it in turn, and at step 9 the game's menu
   comes. 200 ms is 25 of the timer's units. */
#define TITLE_STEP_UNITS 25
#define TITLE_STEPS 9

/* The game-over picture: the title with a box over its bottom right
   corner, x 51 to 95 and y 50 to 61, dark, its two ends drawn inverted
   from the firmware's pictures, light lines along the top and the bottom
   between them, and the score's digits, from the right, in light slots
   8 columns apart. Measured in MAME: the slots are shown empty first; with
   a new top score the digits come 64 units later and then go and come
   every 62; the menu comes 422 units after the game ended. */
#define BOX_X 51
#define BOX_Y 50
#define BOX_RIGHT 95
#define BOX_H 12
#define BOX_END_W 6
#define BOX_RIGHT_END_X 88
#define BOX_UNITS_X 81
#define BOX_DIGIT_PITCH 8
#define BOX_DIGIT_Y 51
#define BOX_DIGIT_W 6
#define BOX_DIGIT_H 8
#define BOX_DIGITS_MAX 4
#define BOX_LEFT_END 0  /* places in snake2_box */
#define BOX_RIGHT_END 12
#define BOX_DIGIT_0 24
#define OVER_FIRST_UNITS 64
#define OVER_BLINK_UNITS 62
#define OVER_UNITS 422

static struct {
    uint16_t units;   /* timer units since the start */
    uint16_t us;      /* time not yet turned into units */
    uint8_t step;     /* the title's */
    uint16_t score;
    uint8_t blink;
    uint8_t shown;    /* the digits are shown */
} t;

static void put_pixel(uint8_t x, uint8_t y, uint8_t on)
{
    uint8_t *p = &sprite_screen[x + LCD_WIDTH * (y >> 3)], bit = (uint8_t)(1 << (y & 7));

    if (on)
        *p |= bit;
    else
        *p &= (uint8_t)~bit;
}

static uint8_t bitmap_pixel(const uint8_t *bits, uint8_t w, uint8_t x, uint8_t y)
{
    return (uint8_t)(bits[x + w * (y >> 3)] >> (y & 7) & 1);
}

static void show_picture(const uint8_t *picture)
{
    memcpy(sprite_screen, picture, sizeof sprite_screen);
}

/* Lets time pass in the timer's units; returns how many went by. */
static uint16_t elapse(uint16_t us)
{
    uint16_t n = 0;

    for (t.us += us; t.us >= GAMES_UNIT_US; t.us -= GAMES_UNIT_US)
        n++;
    t.units += n;
    return n;
}

void title_start(void)
{
    memset(&t, 0, sizeof t);
}

uint8_t title_elapse(uint16_t us)
{
    uint8_t step;

    elapse(us);
    step = (uint8_t)(t.units / TITLE_STEP_UNITS);
    if (step == t.step)
        return 0;
    t.step = step;
    if (step >= TITLE_STEPS)
        return TITLE_OVER;
    return step <= TITLE_FRAME_COUNT ? TITLE_CHANGED : 0;
}

void title_draw(void)
{
    uint8_t frame = t.step > TITLE_FRAME_COUNT ? TITLE_FRAME_COUNT : t.step;

    if (!frame)
        show_picture(PICTURE(SNAKE2_TITLE_BASE));
    else
        show_picture(PICTURE(TITLE_FRAMES + (frame - 1) * TITLE_FRAME_STEP));
}

void over_start(uint16_t score, uint8_t blink)
{
    memset(&t, 0, sizeof t);
    t.score = score;
    t.blink = blink;
    t.shown = !blink;
}

uint8_t over_elapse(uint16_t us)
{
    uint8_t shown;

    elapse(us);
    if (t.units >= OVER_UNITS)
        return TITLE_OVER;
    if (!t.blink)
        return 0;
    shown = t.units >= OVER_FIRST_UNITS && (t.units - OVER_FIRST_UNITS) / OVER_BLINK_UNITS % 2 == 0;
    if (shown == t.shown)
        return 0;
    t.shown = shown;
    return TITLE_CHANGED;
}

void over_draw(void)
{
    const uint8_t *digit;
    uint16_t value = t.score;
    uint8_t x, y, i = 0;

    show_picture(PICTURE(SNAKE2_TITLE_BASE));
    for (y = 0; y < BOX_H; y++) {
        for (x = BOX_X; x <= BOX_RIGHT; x++)
            put_pixel(x, (uint8_t)(BOX_Y + y), 1);
        for (x = BOX_X + BOX_END_W; x < BOX_RIGHT_END_X; x++)
            put_pixel(x, (uint8_t)(BOX_Y + y), (uint8_t)(y != 0 && y < BOX_H - 2));
        for (x = 0; x < BOX_END_W; x++) {
            put_pixel((uint8_t)(BOX_X + x), (uint8_t)(BOX_Y + y), !bitmap_pixel(snake2_box + BOX_LEFT_END, BOX_END_W, x, y));
            put_pixel((uint8_t)(BOX_RIGHT_END_X + x), (uint8_t)(BOX_Y + y),
                      !bitmap_pixel(snake2_box + BOX_RIGHT_END, BOX_END_W, x, y));
        }
    }
    /* The digits from the right, no leading zeros, at least one. */
    do {
        digit = snake2_box + BOX_DIGIT_0 + (value % 10) * BOX_DIGIT_W;
        for (y = 0; y < BOX_DIGIT_H; y++)
            for (x = 0; x < BOX_DIGIT_W; x++)
                put_pixel((uint8_t)(BOX_UNITS_X - BOX_DIGIT_PITCH * i + x), (uint8_t)(BOX_DIGIT_Y + y),
                          t.shown && bitmap_pixel(digit, BOX_DIGIT_W, x, y));
        value /= 10;
    } while (value && ++i < BOX_DIGITS_MAX);
}

/* The High scores page: the chosen maze's top score in a box at the top,
   between two medals, and the last game's score in another at the bottom
   when it was played on that maze; and a snake that crawls in from the
   left along the middle, eats a creature that bobs up and down, swallows
   it down to its tail and crawls out on the right. Every 200 ms of the
   phone's timer it moves on a step, as the title does; then the page
   stands until a key. */
#define HS_SEGMENTS 7
#define HS_HEAD 0x4b302cul      /* the head, going right */
#define HS_HEAD_OPEN 0x4b31acul /* its mouth open */
#define HS_BODY 0x4b30ecul
#define HS_BODY_FAT 0x4b314cul
#define HS_TAIL 0x4b308cul
#define HS_CREATURES 0x4b2f0cul
#define HS_DESCRIPTOR 24
#define HS_MEDAL 0x4b4ab0ul    /* 11x11, in snake2_box */
#define HS_MEDAL_W 11
#define HS_DIGITS 5
#define HS_BOX_W (HS_DIGITS * 8 + 10)

static struct {
    uint16_t units, us;
    uint8_t step;
    uint8_t phase;
    int16_t x[HS_SEGMENTS];  /* the segments', head first */
    uint32_t picture[HS_SEGMENTS];
    uint8_t row;
    int16_t creature_x;
    uint8_t creature_y, bob, creature_shown, kind;
    uint16_t top, last;
    uint8_t show_last;
    uint8_t drawn;           /* the boxes are in sprite_screen */
} h;

/* A picture by its 24-byte descriptor, as the phone's sprites draw it, set
   and clear bits both, clipped to the screen. */
static void draw_picture(uint32_t descriptor, int16_t x, int16_t y)
{
    const uint8_t *d = snake2_pictures + (uint16_t)(descriptor - SNAKE2_PICTURES_BASE);
    const uint8_t *bits = snake2_pictures + (uint16_t)(((uint16_t)d[10] << 8 | d[11]) - (uint16_t)SNAKE2_PICTURES_BASE);
    uint8_t w = d[1], ht = d[3], i, j;

    for (i = 0; i < w; i++)
        for (j = 0; j < ht; j++)
            if (x + i >= 0 && x + i < LCD_WIDTH && y + j >= 0 && y + j < LCD_HEIGHT)
                put_pixel((uint8_t)(x + i), (uint8_t)(y + j), bitmap_pixel(bits, w, i, j));
}

/* A bitmap of snake2_box, set and clear bits both. */
static void draw_box_bitmap(uint16_t place, uint8_t w, uint8_t ht, uint8_t x, uint8_t y)
{
    uint8_t i, j;

    for (i = 0; i < w; i++)
        for (j = 0; j < ht; j++)
            put_pixel((uint8_t)(x + i), (uint8_t)(y + j), bitmap_pixel(snake2_box + place, w, i, j));
}

/* A score in its box of five digits, its ends from the game-over box's,
   lines along its top and bottom, and at the top a medal each side. */
static void draw_score_box(uint16_t value, uint8_t y, uint8_t medals)
{
    uint8_t x = (uint8_t)((LCD_WIDTH - HS_BOX_W) / 2), right = (uint8_t)(x + HS_BOX_W - BOX_END_W), i, j;
    uint16_t divisor = 10000;

    if (medals) {
        draw_box_bitmap((uint16_t)(HS_MEDAL - SNAKE2_BOX_BASE), HS_MEDAL_W, HS_MEDAL_W, 1, 1);
        draw_box_bitmap((uint16_t)(HS_MEDAL - SNAKE2_BOX_BASE), HS_MEDAL_W, HS_MEDAL_W, LCD_WIDTH - 12, 1);
    }
    draw_box_bitmap(BOX_LEFT_END, BOX_END_W, BOX_H, x, y);
    draw_box_bitmap(BOX_RIGHT_END, BOX_END_W, BOX_H, right, y);
    for (i = (uint8_t)(x + BOX_END_W); i < right; i++)
        for (j = 0; j < BOX_H; j++)
            put_pixel(i, (uint8_t)(y + j), (uint8_t)(j == 0 || j >= BOX_H - 2));
    for (i = 0; i < HS_DIGITS; i++, divisor /= 10)
        draw_box_bitmap((uint16_t)(BOX_DIGIT_0 + value / divisor % 10 * BOX_DIGIT_W), BOX_DIGIT_W, BOX_DIGIT_H,
                        (uint8_t)(x + BOX_END_W + i * BOX_DIGIT_PITCH), (uint8_t)(y + 1));
}

void scores_start(uint16_t top, uint16_t last, uint8_t show_last, uint8_t kind)
{
    uint8_t i;

    memset(&h, 0, sizeof h);
    h.top = top;
    h.last = last;
    h.show_last = show_last;
    h.row = show_last ? (uint8_t)(2 * LCD_HEIGHT / 5) : (uint8_t)(LCD_HEIGHT / 2);
    for (i = 0; i < HS_SEGMENTS; i++) {
        h.x[i] = (int16_t)(-4 - 4 * i);
        h.picture[i] = i == 0 ? HS_HEAD : i == HS_SEGMENTS - 1 ? HS_TAIL : HS_BODY;
    }
    h.creature_x = LCD_WIDTH / 2 & ~3;
    h.creature_y = h.row;
    h.bob = 1;
    h.creature_shown = 1;
    h.kind = kind;
}

static void scores_step(void)
{
    uint8_t i;

    if (h.phase == 0) {
        if (h.bob)
            h.creature_y++;
        else
            h.creature_y--;
        h.bob ^= 1;
    }
    for (i = 0; i < HS_SEGMENTS; i++)
        h.x[i] += 4;
    switch (h.phase) {
    case 0:
        /* The mouth opens as the head comes up to the creature. */
        if (h.creature_x - 4 <= h.x[0]) {
            h.picture[0] = HS_HEAD_OPEN;
            h.phase = 1;
        }
        break;
    case 1:
        h.picture[0] = HS_HEAD;
        h.creature_shown = 0;
        h.phase = 2;
        break;
    default:
        /* The swallowed creature goes down the body a segment a step. */
        if (h.phase < 8) {
            h.picture[h.phase - 1] = HS_BODY_FAT;
            if (h.phase > 2)
                h.picture[h.phase - 2] = HS_BODY;
            h.picture[HS_SEGMENTS - 1] = HS_TAIL;
            h.phase++;
        }
        break;
    }
}

uint8_t scores_elapse(uint16_t us)
{
    uint8_t step, changed = 0;

    for (h.us += us; h.us >= GAMES_UNIT_US; h.us -= GAMES_UNIT_US)
        h.units++;
    step = (uint8_t)(h.units / TITLE_STEP_UNITS);
    while (h.step != step) {
        h.step++;
        scores_step();
        changed = TITLE_CHANGED;
    }
    return changed;
}

/* Clears the rows the snake and the creature move in, the creature's one
   more as it bobs. */
#define HS_ROWS 5
static void clear_snake_rows(void)
{
    uint8_t y, x, band, mask;
    uint8_t *p;

    for (y = h.row; y < h.row + HS_ROWS; y = (uint8_t)((y | 7) + 1)) {
        band = y >> 3;
        mask = (uint8_t)(0xff << (y & 7));
        if ((uint8_t)(band * 8 + 8) > h.row + HS_ROWS)
            mask &= (uint8_t)(0xff >> ((band * 8 + 8) - (h.row + HS_ROWS)));
        p = sprite_screen + band * LCD_WIDTH;
        for (x = 0; x < LCD_WIDTH; x++)
            p[x] &= (uint8_t)~mask;
    }
}

void scores_draw(void)
{
    uint8_t i;

    /* The boxes are drawn once; each step only the snake's rows again. */
    if (!h.drawn) {
        memset(sprite_screen, 0, sizeof sprite_screen);
        draw_score_box(h.top, 1, 1);
        if (h.show_last)
            draw_score_box(h.last, LCD_HEIGHT - 19, 0);
        h.drawn = 1;
    } else {
        clear_snake_rows();
    }
    for (i = 0; i < HS_SEGMENTS; i++)
        draw_picture(h.picture[i], h.x[i], h.row);
    if (h.creature_shown)
        draw_picture(HS_CREATURES + h.kind * HS_DESCRIPTOR, h.creature_x, h.creature_y);
}
