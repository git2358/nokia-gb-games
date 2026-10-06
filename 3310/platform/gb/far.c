#include "far.h"

#define SI_SETUP_IMPL /* the real functions' names, not the far_ ones */
#include "si_state.h"
#include "bantumi.h"
#include "fireworks.h"
#include "pairs2.h"
#include "snake2.h"
#include "strip.h"
#include "title.h"
#include "native_gb.h"

#include <string.h>

#include "game_assets.h"

#define MBC_ROM_BANK (*(volatile uint8_t *)0x2000)

static uint8_t mapped = BANK_MENU;

void far_bank(uint8_t bank)
{
    mapped = bank;
    MBC_ROM_BANK = bank;
}

int far_si_handler(int event, struct game_context *ctx)
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

void far_si_ship_spawn(struct game_context *ctx, int how)
{
    uint8_t was = mapped;

    far_bank(BANK_SETUP);
    si_ship_spawn(ctx, how);
    far_bank(was);
}

int far_si_new_game(struct game_context *ctx)
{
    uint8_t was = mapped;
    int result;

    far_bank(BANK_SETUP);
    result = si_new_game(ctx);
    far_bank(was);
    return result;
}

int far_si_continue_key(int event, struct game_context *ctx)
{
    uint8_t was = mapped;
    int result;

    far_bank(BANK_SETUP);
    result = si_continue_key(event, ctx);
    far_bank(was);
    return result;
}

void far_si_continue_enter(struct game_context *ctx)
{
    uint8_t was = mapped;

    far_bank(BANK_SETUP);
    si_continue_enter(ctx);
    far_bank(was);
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

int far_pairs2_handler(int event, struct game_context *ctx)
{
    uint8_t was = mapped;
    int result;

    far_bank(BANK_PAIRS);
    result = pairs2_handler(event, ctx);
    far_bank(was);
    return result;
}

/* The sprites' pictures are Pairs II's, in its bank. */
void far_pairs2_render(void)
{
    uint8_t was = mapped;

    far_bank(BANK_PAIRS);
    pairs2_render();
    far_bank(was);
}

int far_bantumi_handler(int event, struct game_context *ctx)
{
    uint8_t was = mapped;
    int result;

    far_bank(BANK_BANTUMI);
    result = bantumi_handler(event, ctx);
    far_bank(was);
    return result;
}

/* The sprites' pictures are Bantumi's, in its bank. */
void far_bantumi_render(void)
{
    uint8_t was = mapped;

    far_bank(BANK_BANTUMI);
    bantumi_render();
    far_bank(was);
}

/* The fireworks' pictures are with Bantumi's. */
void far_fireworks_draw(uint8_t picture)
{
    uint8_t was = mapped;

    far_bank(BANK_BANTUMI);
    fireworks_draw(picture);
    far_bank(was);
}

void far_strip_present(uint8_t all)
{
    uint8_t was = mapped;

    far_bank(BANK_SNAKE);
    strip_present(all);
    far_bank(was);
}

void far_zoom_present(uint8_t all)
{
    uint8_t was = mapped;

    far_bank(BANK_SNAKE);
    zoom_present(all);
    far_bank(was);
}

void far_strip_leave(uint8_t scx, uint8_t write_map)
{
    uint8_t was = mapped;

    far_bank(BANK_SNAKE);
    strip_leave(scx, write_map);
    far_bank(was);
}

void far_title_start(uint8_t game, uint8_t reseed, uint16_t seed)
{
    uint8_t was = mapped;

    far_bank(BANK_SETUP);
    title_start(game, reseed, seed);
    far_bank(was);
}

uint8_t far_title_elapse(uint16_t us)
{
    uint8_t was = mapped, what;

    far_bank(BANK_SETUP);
    what = title_elapse(us);
    far_bank(was);
    return what;
}

void far_title_draw(void)
{
    uint8_t was = mapped;

    far_bank(BANK_SETUP);
    title_draw();
    far_bank(was);
}

/* Space Impact's data in cartridge RAM, from its image in bank 3. */
void far_si_data_restore(void)
{
    uint8_t was = mapped;

    far_bank(BANK_SETUP);
    memcpy((uint8_t *)SI_DATA_AT, si_data, SI_DATA_SIZE);
    far_bank(was);
}

/* Snake II's state lies over Space Impact's data in cartridge RAM. */
static uint8_t si_data_overwritten;

void platform_game_starts(uint8_t game)
{
    if (game == GAME_SNAKE) {
        si_data_overwritten = 1;
    } else if (game == GAME_SPACE_IMPACT && si_data_overwritten) {
        far_si_data_restore();
        si_data_overwritten = 0;
    }
}

void far_snake2_redraw(void)
{
    uint8_t was = mapped;

    far_bank(BANK_SNAKE);
    snake2_redraw();
    far_bank(was);
}

/* The full-screen menus made at build time: native_tiles.c and the
   screens in one bank, their tiles in the next. */
const uint8_t native_bank = BANK_NATIVE, native_tile_bank = BANK_NATIVE_TILES;

uint8_t far_native_show(uint16_t id)
{
    uint8_t was = mapped, result;

    far_bank(BANK_NATIVE);
    result = native_show(id);
    far_bank(was);
    return result;
}

void far_native_cursor(uint8_t row, uint8_t on)
{
    uint8_t was = mapped;

    far_bank(BANK_NATIVE);
    native_cursor(row, on);
    far_bank(was);
}

uint8_t far_native_leave(void)
{
    uint8_t was = mapped, result;

    far_bank(BANK_NATIVE);
    result = native_leave();
    far_bank(was);
    return result;
}

/* The palette, and on a Game Boy Color or Advance double speed at the
   first call (native_gb.h). */
void far_native_palette(uint8_t shades)
{
    uint8_t was = mapped;

    far_bank(BANK_NATIVE);
    native_palette(shades);
    far_bank(was);
}
