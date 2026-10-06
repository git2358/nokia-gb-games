/* The firmware's two generators. */
#ifndef CORE_RAND_H
#define CORE_RAND_H

#include <stdint.h>

/* rand/srand, the ANSI C example generator. Space Impact uses it only for
   spawn rows marked random. */
void game_srand(uint32_t seed);
int game_rand(void);

/* The games' own generator: seed = (seed * 0x625f + 0x3623) mod 0xfff1,
   returning the new seed. The phone leaves the seed at 1 at power-on and
   reseeds it from the clock, when the clock is set, each time a game
   starts. */
extern uint16_t game_rand16_seed;
uint16_t game_rand16(void);

#endif
