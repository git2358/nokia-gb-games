/* Space Impact, after the Nokia 3310's (NHM-5 v6.39).

   The game is one handler taking an event and a context, as in the
   firmware (the events, results and context are in core/game.h);
   core/games.c is the layer that turns keys and time into
   events and acts on what the handler returns. The level data, sprites and
   object templates are read from the firmware's own tables, extracted from
   the user's dump at build time. */
#ifndef CORE_SI_H
#define CORE_SI_H

#include <stdint.h>

#include "game.h"

/* Sounds the game asks for. */
enum {
    SI_SOUND_SHIP_HIT = 0x17,
    SI_SOUND_SHOT = 0x18,
    SI_SOUND_SPECIAL = 0x19,
    SI_SOUND_BEAM = 0x1a,
    SI_SOUND_BONUS = 0x1f
};


int si_handler(int event, struct game_context *ctx);

/* The level new games start at, 0 to 7: 0 on the phone, and here unless a
   scripted test asks for a later one. */
extern uint8_t si_first_level;

/* The phone's vibrator, pulsed on hits; games.c has it. */
void games_vibrate(void);

#endif
