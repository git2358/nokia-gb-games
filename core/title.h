/* Snake II's title animation, its game-over picture and its High scores
   page, after the Nokia 3410's (NHM-2 v5.46): the title is a picture and
   five more laid over it one after another, the game ends on the title
   with the score in a box in its bottom right corner, and the High scores
   page shows the top score in a box of its own over a snake eating a
   creature. The pictures are read from the firmware's own data, extracted
   from the user's dump; the map is docs/games_snake2_3410.md in the MAME
   fork. All draw into sprite_screen, as the game does. */
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

/* The High scores page: the chosen maze's top score, the last game's when
   show_last says it was played on that maze, and a snake eating a
   creature of the kind given (0 to 5) as it crosses the screen. */
void scores_start(uint16_t top, uint16_t last, uint8_t show_last, uint8_t kind);
uint8_t scores_elapse(uint16_t us);
void scores_draw(void);

#endif
