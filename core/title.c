#include "title.h"

#include <string.h>

#include "game_assets.h"
#include "games.h"
#include "lcd.h"
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
