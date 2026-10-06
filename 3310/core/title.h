/* The games' title animations, after the Nokia 3310's (NHM-5 v6.39): the
   short picture sequence the phone plays when a game is chosen from the
   list, before the game's menu. Any key ends one early. The pictures are
   read from the firmware's own data, extracted from the user's dump; the
   map is the Title screens section of docs/games_pairs2_3310.md in the
   MAME fork. */
#ifndef CORE_TITLE_H
#define CORE_TITLE_H

#include <stdint.h>

/* Starts the title of a game (a GAME_ code). Space Impact's stars draw on
   the games' random generator, which the phone reseeds from its clock
   first; `reseed` says whether to do that, and with what. */
void title_start(uint8_t game, uint8_t reseed, uint16_t seed);

/* What title_elapse reports. */
#define TITLE_CHANGED 1 /* the picture changed: call title_draw */
#define TITLE_OVER 2

/* Lets this much time pass, in microseconds, at most 50000. */
uint8_t title_elapse(uint16_t us);

/* Draws the picture into sprite_screen. */
void title_draw(void);

#endif
