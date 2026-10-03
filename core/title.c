#include "title.h"

#include <string.h>

#include "game.h"
#include "game_assets.h"
#include "games.h"
#include "lcd.h"
#include "rand.h"
#include "sprite.h"

/* The pictures, by the firmware address of their 12-byte descriptors. */
#define SNAKE_BACKGROUND 0x318318ul /* 84x48 */
#define SNAKE_FRAMES 0x318324ul     /* five, about 48x33, at x 6 */
#define BANTUMI_BACKGROUND 0x3185c4ul
#define BANTUMI_PICTURES 0x3185d0ul /* six small ones */
#define SI_SHIP 0x31889cul          /* the ship, then the three it turns into */
#define SI_LOGO_TOP 0x3188ccul
#define SI_LOGO_BOTTOM 0x3188d8ul
#define SI_STAR 0x3188e4ul          /* one pixel */
#define PAIRS_BACKGROUND 0x318f20ul /* two rows of card backs */
#define PAIRS_CARDS 0x318f2cul      /* eight, each turning a card */

/* The y of each of Snake II's frames. */
static const uint8_t snake_frame_y[5] = { 16, 14, 13, 15, 15 };
/* Where Bantumi's six pictures go. */
static const uint8_t bantumi_at[6][2] = { { 23, 18 }, { 34, 23 }, { 37, 33 }, { 45, 19 }, { 53, 15 }, { 62, 27 } };
/* Where Space Impact's ship's three later pictures go. */
static const uint8_t si_ship_at[3][2] = { { 31, 18 }, { 41, 18 }, { 50, 19 } };
/* Where Pairs II's eight pictures go, one a step. */
static const uint8_t pairs_at[8][2] = { { 6, 7 },  { 3, 1 },  { 3, 1 },  { 19, 1 },
                                        { 35, 1 }, { 38, 1 }, { 35, 1 }, { 35, 25 } };

#define STARS 30

static struct {
    uint8_t game;
    uint8_t step;          /* ticks so far */
    uint8_t round;         /* Bantumi plays its pictures twice */
    uint8_t shown;         /* Snake II's frame or Bantumi's picture shown, 0 for none */
    uint16_t period;       /* ms */
    uint16_t left;         /* timer units to the next tick */
    uint16_t us;           /* microseconds not yet turned into units */
    uint8_t stars[STARS][2];
} t;

/* The phone's timers count milliseconds as units of 255/32 ms, dropping
   the fraction. */
static uint16_t units(uint16_t ms)
{
    return (uint16_t)(ms / 255 * 32 + ms % 255 * 32 / 255);
}

static void draw(uint32_t descriptor, uint8_t mode, uint8_t x, uint8_t y)
{
    const uint8_t *d = title_data + (uint16_t)(descriptor - TITLE_DATA_BASE);
    /* The bitmaps lie within 64 KiB of the data's start. */
    uint16_t bitmap = (uint16_t)(((uint16_t)d[2] << 8 | d[3]) - (uint16_t)TITLE_DATA_BASE);
    struct sprite s;

    memset(&s, 0, sizeof s);
    s.flags = (uint8_t)(mode << 3 | SPRITE_BITMAP);
    s.x = x;
    s.y = y;
    s.image.bitmap = title_data + bitmap;
    s.image.w = d[8];
    s.image.h = d[9];
    sprite_draw(&s);
}

void title_start(uint8_t game, uint8_t reseed, uint16_t seed)
{
    uint8_t i;

    memset(&t, 0, sizeof t);
    t.game = game;
    switch (game) {
    case GAME_SNAKE:
    case GAME_BANTUMI:
        t.period = 250;
        break;
    case GAME_SPACE_IMPACT:
        t.period = 210;
        if (reseed)
            game_rand16_seed = seed;
        for (i = 0; i < STARS; i++) {
            t.stars[i][0] = (uint8_t)(game_rand16() % LCD_WIDTH);
            t.stars[i][1] = (uint8_t)(game_rand16() % LCD_HEIGHT);
        }
        break;
    default:
        t.period = 200;
        break;
    }
    t.left = units(t.period);
}

/* One tick of the title. Returns nonzero when it is over. */
static uint8_t step(void)
{
    uint8_t s = ++t.step;

    switch (t.game) {
    case GAME_SNAKE:
        /* Frames 1 to 4 one at a time, then none: the phone's index is
           one ahead, so the first frame is never shown and the fifth
           step shows nothing. */
        if (s < 6)
            t.shown = s < 5 ? s : 0;
        else if (s == 6)
            t.period = 1200;
        else
            return 1;
        break;
    case GAME_BANTUMI:
        if (s < 7) {
            t.shown = s;
            if (s == 3)
                t.period = 70;
            else if (s == 4)
                t.period = 250;
        } else if (s == 7) {
            if (t.round) {
                t.period = 700;
            } else {
                t.round = 1;
                t.step = 1;
                t.shown = 1;
            }
        } else {
            return 1;
        }
        break;
    case GAME_SPACE_IMPACT:
        if (s == 9)
            t.period = 700;
        else if (s > 9)
            return 1;
        break;
    default:
        if (s == 9)
            t.period = 1400;
        else if (s > 9)
            return 1;
        break;
    }
    return 0;
}

uint8_t title_elapse(uint16_t us)
{
    uint8_t changed = 0;

    for (t.us += us; t.us >= GAMES_UNIT_US; t.us -= GAMES_UNIT_US) {
        if (--t.left)
            continue;
        if (step())
            return TITLE_OVER;
        changed = TITLE_CHANGED;
        t.left = units(t.period);
    }
    return changed;
}

void title_draw(void)
{
    uint8_t s = t.step, i;

    memset(sprite_screen, 0, sizeof sprite_screen);
    switch (t.game) {
    case GAME_SNAKE:
        draw(SNAKE_BACKGROUND, SPRITE_MODE_OPAQUE, 0, 0);
        if (t.shown)
            draw(SNAKE_FRAMES + 12 * t.shown, SPRITE_MODE_OPAQUE, 6, snake_frame_y[t.shown]);
        break;
    case GAME_BANTUMI:
        draw(BANTUMI_BACKGROUND, SPRITE_MODE_OPAQUE, 0, 0);
        if (t.shown)
            draw(BANTUMI_PICTURES + 12 * (t.shown - 1), SPRITE_MODE_OPAQUE, bantumi_at[t.shown - 1][0],
                 bantumi_at[t.shown - 1][1]);
        break;
    case GAME_SPACE_IMPACT: {
        /* Drawn as the phone's list has them: the lower layer (the screen
           filled, the stars cut out of it, the logo's halves closing in),
           then the ship, which flies in and then turns into each of the
           other three pictures in turn. */
        struct sprite fill;
        uint8_t moved = s < 8 ? s : 8;

        memset(&fill, 0, sizeof fill);
        fill.flags = (uint8_t)(SPRITE_MODE_OPAQUE << 3 | SPRITE_FILL);
        fill.x2 = LCD_WIDTH;
        fill.y2 = LCD_HEIGHT;
        sprite_draw(&fill);
        for (i = 0; i < STARS; i++)
            draw(SI_STAR, SPRITE_MODE_INVERSE, t.stars[i][0], t.stars[i][1]);
        draw(SI_LOGO_TOP, SPRITE_MODE_OPAQUE, 7, (uint8_t)(1 + moved));
        draw(SI_LOGO_BOTTOM, SPRITE_MODE_OPAQUE, 3, (uint8_t)(32 - moved));
        if (s < 6)
            draw(SI_SHIP, SPRITE_MODE_OPAQUE, (uint8_t)(4 * s), 18);
        else
            draw(SI_SHIP + 12 * (s < 8 ? s - 5 : 3), SPRITE_MODE_OPAQUE, si_ship_at[s < 8 ? s - 6 : 2][0],
                 si_ship_at[s < 8 ? s - 6 : 2][1]);
        break;
    }
    default:
        draw(PAIRS_BACKGROUND, SPRITE_MODE_OPAQUE, 0, 0);
        for (i = 0; i < s && i < 8; i++)
            draw(PAIRS_CARDS + 12 * i, SPRITE_MODE_OPAQUE, pairs_at[i][0], pairs_at[i][1]);
        break;
    }
}
