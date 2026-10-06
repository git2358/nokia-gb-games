/* Space Impact on the 3410. The phone's game is the 3310's rebuilt on the
   3410's graphics library, with its levels ("chapters") in a file; this
   follows the 3410's code function by function. The map is
   docs/games_si_3410.md in the MAME fork. */
#ifndef CORE_SI_H
#define CORE_SI_H

#include <stdint.h>

/* The handler's events, as the 3410 numbers them. */
enum {
    SI_EVENT_TICK = 0x00,
    SI_EVENT_KEY_DOWN = 0x01, /* a: the key's code */
    SI_EVENT_KEY_UP = 0x02,
    SI_EVENT_PAUSE = 0x03,
    SI_EVENT_NEW_GAME = 0x0a,
    SI_EVENT_CONTINUE = 0x0d
};

/* Key codes: the digits, and these. */
#define SI_KEY_STAR 10
#define SI_KEY_HASH 11

/* What the handler asks of the framework. */
enum {
    SI_DONE_REDRAW = 1,  /* the picture changed */
    SI_DONE_CLOSE = 2    /* the game is over: back to its menu */
};

/* The keys held, a bit per key code, as the framework would answer the
   game's poll (0x3b29d0); the caller keeps it up to date. */
extern uint16_t si_keys_held;
/* The tick period the game has asked for, in ms. */
extern uint16_t si_period;

/* Returns SI_DONE_ bits. */
uint8_t si_event(uint8_t event, uint8_t a);

#endif
