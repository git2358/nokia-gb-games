/* Snake II's title animation and its game-over picture, after the Nokia
   3410's (NHM-2 v5.46): the title is a picture and five more laid over it
   one after another, and the game ends on the title with the score in a
   box in its bottom right corner. The pictures are read from the
   firmware's own data, extracted from the user's dump; the map is
   docs/games_snake2_3410.md in the MAME fork. Both draw into
   sprite_screen, as the game does. */
#ifndef CORE_TITLE_H
#define CORE_TITLE_H

#include <stdint.h>

/* What title_elapse and over_elapse report. */
#define TITLE_CHANGED 1 /* the picture changed: draw it */
#define TITLE_OVER 2

void title_start(void);
/* Lets this much time pass, in microseconds, at most 50000. */
uint8_t title_elapse(uint16_t us);
void title_draw(void);

/* The game-over picture for a score; with a new top score its digits
   blink. */
void over_start(uint16_t score, uint8_t blink);
uint8_t over_elapse(uint16_t us);
void over_draw(void);

#endif
