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

uint16_t game_rand16(void)
{
    game_rand16_seed = (uint16_t)((game_rand16_seed * 0x625ful + 0x3623ul) % 0xfff1ul);
    return game_rand16_seed;
}
