/* Space Impact on the 3410. The phone's game is the 3310's rebuilt on the
   3410's graphics library, with its levels ("chapters") in a file; this
   follows the 3410's code function by function. The map is
   docs/games_si_3410.md in the MAME fork. */
#ifndef CORE_SI_H
#define CORE_SI_H

#include <stdint.h>

/* On the Game Boy the game is in banks of its own (si_int.h): what is
   called from another bank is reached through the compiler's banked
   calls. */
#ifdef SI_BANKED
#define SI_FAR __banked
#else
#define SI_FAR
#endif

/* The handler's events, as the 3410 numbers them. */
enum {
    SI_EVENT_TICK = 0x00,
    SI_EVENT_KEY_DOWN = 0x01, /* a: the key's code */
    SI_EVENT_KEY_UP = 0x02,
    SI_EVENT_PAUSE = 0x03,
    SI_EVENT_DEMO = 0x09,     /* a: SI_DEMO_ */
    SI_EVENT_NEW_GAME = 0x0a,
    SI_EVENT_HIGH_SCORES = 0x0b, /* the High scores page */
    SI_EVENT_CONTINUE = 0x0d,
    SI_EVENT_TITLE = 0x0e        /* chosen in Select game: the title */
};

/* The Instructions' demos (0x259a90): firing, the keys shown on a keypad,
   and picking up a bonus. Each closes the game when it is over
   (SI_DONE_CLOSE); a pause (SI_EVENT_PAUSE) ends one early. */
enum {
    SI_DEMO_FIRE = 1,
    SI_DEMO_KEYS = 2,
    SI_DEMO_BONUS = 3
};

/* Key codes: the digits, and these. */
#define SI_KEY_STAR 10
#define SI_KEY_HASH 11

/* What the handler asks of the framework. */
enum {
    SI_DONE_REDRAW = 1,  /* the picture changed */
    SI_DONE_CLOSE = 2    /* the game is over: back to its menu */
};

/* The sounds the game asks for (0x3b2510): codes in the phone's sound
   table, as the 3410 maps its ids 0xfa0.. onto it (0x3f7d0e, less 3). */
enum {
    SI_SOUND_SHIP_HIT = 0x13,  /* 0xfa7: the ship destroyed */
    SI_SOUND_SHOT = 0x14,      /* 0xfa5 */
    SI_SOUND_SPECIAL = 0x15,   /* 0xfa6: a wall or a missile */
    SI_SOUND_BEAM = 0x16,      /* 0xfa8 */
    SI_SOUND_BONUS = 0x1f,     /* 0xfa0 */
    SI_SOUND_GAME_OVER = 0x21, /* 0xfa2 */
    SI_SOUND_TOP_SCORE = 0x23  /* 0xfa4: game over with a new top score */
};

/* What the last event asked of the buzzer and the vibrator (0x3b25d4),
   for the caller to carry out and clear: a SI_SOUND_ code, the last if
   there were several, or 0; and SI_VIBRATE_ON or _OFF, or 0. */
enum {
    SI_VIBRATE_ON = 1,
    SI_VIBRATE_OFF = 2
};
extern uint8_t si_sound, si_vibrate;

/* The keys held, a bit per key code, as the framework would answer the
   game's poll (0x3b29d0); the caller keeps it up to date. */
extern uint16_t si_keys_held;
/* The tick period the game has asked for, in ms. */
extern uint16_t si_period;

/* What the High scores page shows (the phone's record, set up by the
   caller before SI_EVENT_HIGH_SCORES): the top score, and the last game's
   in a second box when `si_show_last`. The top score is also what a
   game's end is measured against for its sound: set it before a game. */
extern uint16_t si_top_score, si_last_score;
extern uint8_t si_show_last;

/* Returns SI_DONE_ bits. The title draws on the generator as it is: seed
   it before SI_EVENT_TITLE, as the phone does from its clock. */
uint8_t si_event(uint8_t event, uint8_t a) SI_FAR;

/* The score of the game being played or last played. */
uint16_t si_score(void) SI_FAR;

/* Draws the picture into sprite_screen. */
void si_render(void) SI_FAR;
/* Whether the High scores page is up: its caller draws the scores over
   the picture, in their boxes (title.h's draw_score_box), from
   si_top_score and so on. */
uint8_t si_scores_shown(void) SI_FAR;
/* For the replays: the lives as the autopilot set them. */
void si_debug_lives(int8_t lives) SI_FAR;
/* Draws the picture into si_rows' tiles instead (si_rows.h). */
void si_render_rows(void) SI_FAR;

#endif
