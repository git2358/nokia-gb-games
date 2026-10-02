#include "sound.h"

struct note {
    uint16_t hz; /* 0 for a rest */
    uint16_t ms;
};

/* Each sound ends with a zero-length rest. */
static const struct note eat[] = { { 2637, 7 }, { 0, 0 } };
static const struct note game_over[] = { { 440, 14 }, { 0, 16 }, { 440, 16 }, { 0, 16 }, { 440, 16 }, { 0, 0 } };
static const struct note top_score[] = {
    { 587, 145 }, { 0, 148 }, { 587, 117 }, { 0, 23 }, { 587, 117 }, { 0, 23 },
    { 880, 412 }, { 0, 23 },  { 587, 117 }, { 0, 23 }, { 880, 444 }, { 0, 0 },
};

static const struct note *playing; /* the note sounding now, or null */
static int32_t left_us;            /* time it still has to run */

void sound_play(uint8_t sound)
{
    playing = sound == SOUND_EAT ? eat : sound == SOUND_GAME_OVER ? game_over : top_score;
    left_us = (int32_t)playing->ms * 1000;
    platform_tone(playing->hz);
}

void sound_tick(uint16_t us)
{
    if (!playing)
        return;
    left_us -= us;
    while (left_us <= 0) {
        playing++;
        if (!playing->ms) {
            playing = 0;
            platform_tone(0);
            return;
        }
        left_us += (int32_t)playing->ms * 1000;
        platform_tone(playing->hz);
    }
}
