#include "games.h"
#include "rumble.h"

#include "snake2.h"
#include "sound.h"
#include "sprite.h"

uint8_t games_over;
uint32_t games_score;
struct game_options games_options = { 1, 1, 1 };

static struct game_context ctx;
uint8_t games_playing = GAME_SNAKE;
#define playing games_playing
/* A continued game stands still until a key is pressed, as Snake II does
   on the phone. */
static uint8_t waiting;

/* Units left on the phone's three game timers; 0 is stopped. */
static uint16_t tick_timer, one_shot_timer, repeat_timer;
/* Units the vibrator still runs for; 0 is off. The phone's vibration is a
   system timer the games application does not touch, so it is not among
   the three above: it runs on through a pause, from games_rumble_elapse. */
static uint8_t vibrate_timer;
static uint16_t vibrate_us; /* microseconds not yet turned into units */
static uint8_t vibrate_frame; /* frames into the motor's on and off, rumble.h */
static uint8_t motor_on;
static uint8_t held_key; /* 0 when none */

#define REPEAT_UNITS 12

enum {
    FROM_OTHER,
    FROM_TICK,
    FROM_TIMER,
    FROM_KEY
};

/* The phone divides milliseconds by 255/32 and drops the fraction, so a
   100 ms tick is 12 units, about 93 ms. */
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

static int handle(int event)
{
    return snake2_handler(event, &ctx);
}

/* The one-shot timer from the context; 0 ms runs out at the next unit. */
static uint16_t one_shot_units(void)
{
    uint16_t n = units(ctx.one_shot);

    return n ? n : 1;
}

static uint8_t deliver(int event, uint8_t from)
{
    int result = handle(event);
    uint8_t draw = 1;

    switch (result) {
    case GAME_RESULT_GAME_OVER:
        games_over = 1;
        games_score = ctx.score;
        tick_timer = one_shot_timer = repeat_timer = 0;
        return 1;
    case GAME_RESULT_RESTART_TIMERS:
        tick_timer = units(ctx.period);
        one_shot_timer = units(ctx.one_shot);
        if (from == FROM_KEY)
            repeat_timer = REPEAT_UNITS;
        return 1;
    case GAME_RESULT_RESTART_TICK:
        tick_timer = units(ctx.period);
        if (from == FROM_TICK)
            from = FROM_OTHER;
        break;
    case GAME_RESULT_SOUND:
        if (games_options.sounds)
            sound_play((uint8_t)ctx.sound);
        break;
    case GAME_RESULT_ONE_SHOT_SOUND:
        if (games_options.sounds)
            sound_play((uint8_t)ctx.sound);
        /* fall through */
    case GAME_RESULT_ONE_SHOT:
        one_shot_timer = one_shot_units();
        if (from == FROM_TIMER)
            from = FROM_OTHER;
        break;
    case GAME_RESULT_REDRAW:
        break;
    case GAME_RESULT_NONE:
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

void games_setup(uint8_t game, uint8_t level, uint8_t option)
{
    playing = game;
    ctx.level = level;
    ctx.option = option;
}

void games_start(uint8_t game, uint8_t level, uint8_t option)
{
    games_setup(game, level, option);
    games_over = 0;
    games_score = 0;
    held_key = 0;
    waiting = 0;
    one_shot_timer = repeat_timer = 0;
    deliver(GAME_EVENT_START, FROM_OTHER);
    tick_timer = units(ctx.period);
}

void games_continue(void)
{
    deliver(GAME_EVENT_RESUME, FROM_OTHER);
    if (games_over)
        return;
    /* The phone stops the game's timers when it leaves it and starts the
       tick again at the first key. A snake that has died has no period to
       wait for, and its timers run on here. */
    if (ctx.period) {
        tick_timer = one_shot_timer = repeat_timer = 0;
        waiting = 1;
    }
}

uint8_t games_key_down(uint8_t key)
{
    if (games_over)
        return 0;
    if (waiting) {
        waiting = 0;
        tick_timer = units(ctx.period);
    }
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
            draw |= deliver(GAME_EVENT_TICK, FROM_TICK);
        if (one_shot_timer && !--one_shot_timer)
            draw |= deliver(GAME_EVENT_TIMER, FROM_TIMER);
        if (repeat_timer && !--repeat_timer && held_key)
            draw |= deliver(held_key | GAME_KEY_REPEAT, FROM_KEY);
    }
    return draw;
}

static void motor(uint8_t on)
{
    if (on != motor_on) {
        motor_on = on;
        platform_rumble(on);
    }
}

void games_vibrate_for(uint8_t units)
{
    if (!games_options.shakes || !(units = RUMBLE_UNITS(units)))
        return;
    if (!vibrate_timer) {
        vibrate_frame = 0;
        motor(1);
    }
    vibrate_timer = units;
}

void games_vibrate(void)
{
    games_vibrate_for(GAMES_VIBRATE_UNITS);
}

void games_rumble_elapse(uint16_t us)
{
    if (!vibrate_timer)
        return;
    for (vibrate_us += us; vibrate_us >= GAMES_UNIT_US; vibrate_us -= GAMES_UNIT_US)
        if (vibrate_timer && !--vibrate_timer)
            motor(0);
    if (vibrate_timer) {
        if (++vibrate_frame >= RUMBLE_ON_FRAMES + RUMBLE_OFF_FRAMES)
            vibrate_frame = 0;
        motor(vibrate_frame < RUMBLE_ON_FRAMES);
    }
}

void games_quiet(void)
{
    vibrate_timer = 0;
    motor(0);
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
    int result = handle(event);

    if (result == GAME_RESULT_GAME_OVER) {
        games_over = 1;
        games_score = ctx.score;
    }
    return result != GAME_RESULT_NONE && result != GAME_RESULT_UNUSED;
}

void games_render(void)
{
    /* Snake II draws into the picture as it goes. */
}

void games_draw(uint8_t all)
{
    games_render();
    /* The full-screen board is drawn into the LCD view as it goes. */
    if (snake2_full) {
        if (all)
            snake2_redraw();
        return;
    }
    sprite_present(all);
}
