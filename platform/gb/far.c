#include "far.h"

#define SI_SETUP_IMPL /* the real functions' names, not the far_ ones */
#include "si_state.h"

#define MBC_ROM_BANK (*(volatile uint8_t *)0x2000)

static uint8_t mapped = BANK_MENU;

void far_bank(uint8_t bank)
{
    mapped = bank;
    MBC_ROM_BANK = bank;
}

int far_si_handler(int event, struct si_context *ctx)
{
    uint8_t was = mapped;
    int result;

    far_bank(BANK_PLAY);
    result = si_handler(event, ctx);
    far_bank(was);
    return result;
}

void far_si_level_load(si_ref header)
{
    uint8_t was = mapped;

    far_bank(BANK_SETUP);
    si_level_load(header);
    far_bank(was);
}

void far_si_ship_spawn(struct si_context *ctx, int how)
{
    uint8_t was = mapped;

    far_bank(BANK_SETUP);
    si_ship_spawn(ctx, how);
    far_bank(was);
}

int far_si_new_game(struct si_context *ctx)
{
    uint8_t was = mapped;
    int result;

    far_bank(BANK_SETUP);
    result = si_new_game(ctx);
    far_bank(was);
    return result;
}

int far_si_continue_key(int event, struct si_context *ctx)
{
    uint8_t was = mapped;
    int result;

    far_bank(BANK_SETUP);
    result = si_continue_key(event, ctx);
    far_bank(was);
    return result;
}

void far_si_continue_enter(struct si_context *ctx)
{
    uint8_t was = mapped;

    far_bank(BANK_SETUP);
    si_continue_enter(ctx);
    far_bank(was);
}
