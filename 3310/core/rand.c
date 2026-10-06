#include "rand.h"

static uint32_t seed = 1;

void game_srand(uint32_t s)
{
    seed = s;
}

int game_rand(void)
{
    seed = seed * 0x41c64e6du + 0x3039u;
    return (int)((seed & 0x7fffffffu) >> 16);
}

uint16_t game_rand16_seed = 1;

/* seed * 0x625f + 0x3623 modulo 0xfff1, without a 32-bit division: 0x10000
   is 15 more than 0xfff1, so the high half of a number counts 15 times. */
uint16_t game_rand16(void)
{
    uint32_t x = game_rand16_seed * 0x625ful + 0x3623ul;
    uint16_t high = (uint16_t)(x >> 16);

    x = (((uint32_t)high << 4) - high) + (uint16_t)x; /* under 0x100000 */
    high = (uint16_t)(x >> 16);
    x = (uint16_t)((high << 4) - high) + (uint32_t)(uint16_t)x; /* under 0x10100 */
    while (x >= 0xfff1ul)
        x -= 0xfff1ul;
    game_rand16_seed = (uint16_t)x;
    return game_rand16_seed;
}
