#include "far.h"

#include "snake2.h"

#define MBC_ROM_BANK (*(volatile uint8_t *)0x2000)

static uint8_t mapped = BANK_MENU;

void far_bank(uint8_t bank)
{
    mapped = bank;
    MBC_ROM_BANK = bank;
}

int far_snake2_handler(int event, struct game_context *ctx)
{
    uint8_t was = mapped;
    int result;

    far_bank(BANK_SNAKE);
    result = snake2_handler(event, ctx);
    far_bank(was);
    return result;
}
