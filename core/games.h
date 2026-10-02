/* The layer between a platform and the game: it turns keys and elapsed
   time into the game's events and acts on what the game's handler returns,
   as the phone's games application does. */
#ifndef CORE_GAMES_H
#define CORE_GAMES_H

#include <stdint.h>

/* The phone's timers count in units of 249/32 ms. */
#define GAMES_UNIT_US 7781

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

/* Lets this much time pass, in microseconds; a platform calls it once per
   screen frame. Returns nonzero when the screen needs drawing. */
uint8_t games_elapse(uint32_t us);

/* Delivers one event straight to the game, for replaying a recorded
   sequence. Returns nonzero when the screen needs drawing. */
uint8_t games_event(int event);

/* Draws the game into the LCD view; with `all`, not just what changed. */
void games_draw(uint8_t all);

/* The game asks for one of its sounds (an SI_SOUND_ code). */
void platform_sound(uint8_t sound);

#endif
