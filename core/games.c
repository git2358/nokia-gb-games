#include "games.h"

#include "si.h"
#include "sound.h"
#include "sprite.h"

uint8_t games_over;
uint32_t games_score;

static struct si_context ctx;

/* Units left on the phone's three game timers; 0 is stopped. */
static uint16_t tick_timer, one_shot_timer, repeat_timer;
static uint8_t held_key; /* 0 when none */

#define REPEAT_UNITS 12

enum {
    FROM_OTHER,
    FROM_TICK,
    FROM_TIMER,
    FROM_KEY
};

/* The phone divides milliseconds by 255/32 and drops the fraction, so the
   game's 100 ms tick is 12 units, about 93 ms. */
static uint16_t units(uint16_t ms)
{
    /* The game asks for the same period tick after tick. */
    static uint16_t last_ms, last_units;

    if (ms != last_ms) {
        last_ms = ms;
        /* ms * 32 / 255, kept within 16 bits. */
        last_units = (uint16_t)(ms / 255 * 32 + ms % 255 * 32 / 255);
    }
    return last_units;
}

static uint8_t deliver(int event, uint8_t from)
{
    int result = si_handler(event, &ctx);
    uint8_t draw = 1;

    switch (result) {
    case SI_RESULT_GAME_OVER:
        games_over = 1;
        games_score = ctx.score;
        tick_timer = one_shot_timer = repeat_timer = 0;
        return 1;
    case SI_RESULT_RESTART_TIMERS:
        tick_timer = units(ctx.period);
        one_shot_timer = units(ctx.one_shot);
        if (from == FROM_KEY)
            repeat_timer = REPEAT_UNITS;
        return 1;
    case SI_RESULT_RESTART_TICK:
        tick_timer = units(ctx.period);
        if (from == FROM_TICK)
            from = FROM_OTHER;
        break;
    case SI_RESULT_SOUND:
        sound_play((uint8_t)ctx.sound);
        break;
    case SI_RESULT_REDRAW:
        break;
    case SI_RESULT_NONE:
        draw = 0;
        break;
    default:
        /* Anything else and the phone leaves its timers as they are. */
        return 0;
    }
    if (from == FROM_TICK)
        tick_timer = units(ctx.period);
    else if (from == FROM_TIMER)
        one_shot_timer = units(ctx.one_shot);
    else if (from == FROM_KEY)
        repeat_timer = REPEAT_UNITS;
    return draw;
}

void games_start(void)
{
    games_over = 0;
    games_score = 0;
    held_key = 0;
    one_shot_timer = repeat_timer = 0;
    deliver(SI_EVENT_START, FROM_OTHER);
    tick_timer = units(ctx.period);
}

uint8_t games_key_down(uint8_t key)
{
    if (games_over)
        return 0;
    held_key = key;
    return deliver(key, FROM_KEY);
}

void games_key_up(void)
{
    held_key = 0;
    repeat_timer = 0;
}

uint8_t games_advance(uint16_t n)
{
    uint8_t draw = 0;

    for (; n && !games_over; n--) {
        if (tick_timer && !--tick_timer)
            draw |= deliver(SI_EVENT_TICK, FROM_TICK);
        if (one_shot_timer && !--one_shot_timer)
            draw |= deliver(SI_EVENT_TIMER, FROM_TIMER);
        if (repeat_timer && !--repeat_timer && held_key)
            draw |= deliver(held_key | SI_KEY_REPEAT, FROM_KEY);
    }
    return draw;
}

uint8_t games_elapse(uint16_t us)
{
    static uint16_t left; /* microseconds not yet turned into units */
    uint16_t n = 0;

    for (left += us; left >= GAMES_UNIT_US; left -= GAMES_UNIT_US)
        n++;
    return games_advance(n);
}

uint8_t games_event(int event)
{
    int result = si_handler(event, &ctx);

    if (result == SI_RESULT_GAME_OVER) {
        games_over = 1;
        games_score = ctx.score;
    }
    return result != SI_RESULT_NONE && result != SI_RESULT_UNUSED;
}

void games_draw(uint8_t all)
{
    sprite_render();
    sprite_present(all);
}
