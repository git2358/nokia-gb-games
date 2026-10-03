/* The layer between a platform and the game: it turns keys and elapsed
   time into the game's events and acts on what the game's handler returns,
   as the phone's games application does. */
#ifndef CORE_GAMES_H
#define CORE_GAMES_H

#include <stdint.h>

#include "game.h"

/* The phone's timers count in units of 249/32 ms. */
#define GAMES_UNIT_US 7781

/* The settings the games follow: a sound is only played and the vibrator
   only run when its switch is on. menu.c loads and keeps them. */
extern struct game_options games_options;

/* Nonzero once the game has ended; games_score is then its score. */
extern uint8_t games_over;
extern uint32_t games_score;

/* Starts a new game of Space Impact. */
void games_start(void);

/* A key went down (an SI_KEY_ code) or the held key came up. One key is
   held at a time, as on the phone's keypad. Returns nonzero when the
   screen needs drawing. */
uint8_t games_key_down(uint8_t key);
void games_key_up(void);

/* Lets this many timer units pass. Returns nonzero when the screen needs
   drawing. */
uint8_t games_advance(uint16_t units);

/* Lets this much time pass, in microseconds, at most 50000; a platform
   calls it once per screen frame. Returns nonzero when the screen needs
   drawing. */
uint8_t games_elapse(uint16_t us);

/* Delivers one event straight to the game, for replaying a recorded
   sequence. Returns nonzero when the screen needs drawing. */
uint8_t games_event(int event);

/* Draws the game into the LCD view; with `all`, not just what changed. */
void games_draw(uint8_t all);

/* The phone's vibrator, as a game asks for it: a pulse of GAMES_VIBRATE_UNITS
   timer units, started afresh by every call, when Shakes is on. */
#define GAMES_VIBRATE_UNITS 62
void games_vibrate(void);

/* Lets this much time pass for the vibrator, in microseconds; a platform
   or the menus call it once per screen frame whether or not a game is
   being played, since the phone's vibration is a system timer that runs
   on through a pause. */
void games_rumble_elapse(uint16_t us);

/* Stops the vibrator at once: for starting over. */
void games_quiet(void);

/* The platform's vibrator or rumble motor, on or off. */
void platform_rumble(uint8_t on);

#endif
