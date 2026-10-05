/* Snake II, after the Nokia 3310's (NHM-5 v6.39).

   As Space Impact, the game is one handler taking an event and a context
   (core/game.h); core/games.c drives it. The pictures, mazes and speeds
   are read from the firmware's own tables, extracted from the user's dump
   at build time. The map it follows is docs/games_snake2_3310.md in the
   MAME fork.

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

/* The phone's collection mask: bit 15 lets the large creature come, bits
   0 to 9 are the ones collected. The phone loads bit 15 from a setting the
   port does not have, so it stays 0 and the large creature never comes. */
extern uint16_t snake2_mask;

/* Nonzero to have new games played on the full-screen variant's board:
   the phone's rule for its size applied to the console's screen, the
   pictures as they are and the mazes moved out to fit. That board is drawn
   straight into the LCD view, which the menus set to all of the screen
   (or the part of it a platform magnifies); the phone-sized one into
   sprite_screen. */
extern uint8_t snake2_full;

/* Draws the whole picture into sprite_screen again. */
void snake2_redraw(void);

#endif
