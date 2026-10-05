/* Snake II, after the Nokia 3410's (NHM-2 v5.46), which is the 3310's
   game on a bigger board.

   The game is one handler taking an event and a context (core/game.h);
   core/games.c drives it. The pictures, mazes and speeds are read from the
   firmware's own tables, extracted from the user's dump at build time. The
   maps it follows are docs/games_snake2_3310.md and
   docs/games_applications_3410.md in the MAME fork.

   The phone draws every piece of the snake as its own sprite. Here the
   pieces are drawn straight into sprite_screen, the picture the platforms
   present: no two of them ever overlap, so the picture is the same. */
#ifndef CORE_SNAKE2_H
#define CORE_SNAKE2_H

#include <stdint.h>

#include "game.h"

/* Sounds the game asks for. */
enum {
    SNAKE2_SOUND_EAT = 0x1f,
    SNAKE2_SOUND_DEATH = 0x20,
    SNAKE2_SOUND_FULL = 0x22 /* the snake fills its ring; cannot happen */
};

/* The levels and mazes the menus offer: ctx->level is 1 to
   SNAKE2_LEVELS, ctx->option 1 (no maze) to SNAKE2_MAZE_COUNT. */
#define SNAKE2_LEVELS 9
#define SNAKE2_MAZE_COUNT 6

int snake2_handler(int event, struct game_context *ctx);

/* Draws the whole picture into sprite_screen again. */
void snake2_redraw(void);

#endif
