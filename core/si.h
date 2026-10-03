/* Space Impact, after the Nokia 3310's (NHM-5 v6.39).

   The game is one handler taking an event and a context, as in the
   firmware; core/games.c is the layer that turns keys and time into
   events and acts on what the handler returns. The level data, sprites and
   object templates are read from the firmware's own tables, extracted from
   the user's dump at build time. */
#ifndef CORE_SI_H
#define CORE_SI_H

#include <stdint.h>

/* Events. Keys are the phone's key codes; a held key repeats as its code
   plus SI_KEY_REPEAT. */
enum {
    SI_EVENT_TIMER = 0x00, /* the one-shot timer ran out */
    SI_EVENT_TICK = 0x01,
    SI_KEY_0 = 0x09,
    SI_KEY_1 = 0x0a,
    SI_KEY_3 = 0x0c,
    SI_KEY_4 = 0x0d,
    SI_KEY_6 = 0x0f,
    SI_KEY_8 = 0x11,
    SI_KEY_HASH = 0x14,
    SI_KEY_STAR = 0x16,
    SI_EVENT_START = 0x2b,
    SI_KEY_REPEAT = 0x80
};

/* What the handler returns. */
enum {
    SI_RESULT_NONE = 0x01,          /* nothing changed */
    SI_RESULT_FAILED = 0x02,
    SI_RESULT_UNUSED = 0x05,
    SI_RESULT_RESTART_TIMERS = 0x13, /* start the tick and the one-shot timer afresh */
    SI_RESULT_RESTART_TICK = 0x16,   /* start the tick afresh with the period now set */
    SI_RESULT_GAME_OVER = 0x18,      /* the score is in the context */
    SI_RESULT_SOUND = 0x1b,          /* play the context's sound */
    SI_RESULT_REDRAW = 0x21
};

/* Sounds the game asks for. */
enum {
    SI_SOUND_SHIP_HIT = 0x17,
    SI_SOUND_SHOT = 0x18,
    SI_SOUND_SPECIAL = 0x19,
    SI_SOUND_BEAM = 0x1a,
    SI_SOUND_BONUS = 0x1f
};

struct si_context {
    uint16_t period;   /* tick period, ms */
    uint16_t one_shot; /* one-shot timer, ms */
    uint32_t score;
    uint16_t sound;
};

int si_handler(int event, struct si_context *ctx);

/* The level new games start at, 0 to 7: 0 on the phone, and here unless a
   scripted test asks for a later one. */
extern uint8_t si_first_level;

/* The phone's vibrator, pulsed on hits; games.c has it. */
void games_vibrate(void);

#endif
