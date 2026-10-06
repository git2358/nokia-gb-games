/* Pairs II, after the Nokia 3310's (NHM-5 v6.39): Time trial and Puzzle.

   As the other games, one handler taking an event and a context
   (core/game.h); core/games.c drives it. It draws with the sprite layer
   (core/sprite.h) and makes and frees its sprites in the firmware's own
   order, which decides what is drawn over what. The pictures and the Time
   trial boards are read from the user's dump; the map it follows is
   docs/games_pairs2_3310.md in the MAME fork. */
#ifndef CORE_PAIRS2_H
#define CORE_PAIRS2_H

#include <stdint.h>

#include "game.h"

/* The two modes, ctx->option. */
enum {
    PAIRS2_TIME_TRIAL,
    PAIRS2_PUZZLE
};

/* The levels the menus offer: ctx->level is 1 to PAIRS2_LEVELS. */
#define PAIRS2_LEVELS 7

/* Sounds the game asks for. */
enum {
    PAIRS2_SOUND_PAIR = 0x1c,
    PAIRS2_SOUND_MISS = 0x1e
};

int pairs2_handler(int event, struct game_context *ctx);

/* Draws the sprites into sprite_screen; a platform whose pictures are not
   always in reach (the Game Boy's banks) calls it with them mapped. */
void pairs2_render(void);

#endif
