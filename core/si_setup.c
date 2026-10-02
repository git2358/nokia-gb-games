/* Space Impact: how a game, a level and a ship begin, and the continue
   screen. Each function follows one of the firmware's; see si.c. */
#define SI_SETUP_IMPL
#include <string.h>

#include "rand.h"
#include "si_state.h"

/* The tile maps of the eight levels: the firmware stores these addresses
   one by one when a game starts. */
static const si_ref level_maps[8] = { SI_REF(0x311740ul), SI_REF(0x311780ul), SI_REF(0x311760ul), SI_REF(0x311760ul), SI_REF(0x3117a0ul), SI_REF(0x3117c0ul), SI_REF(0x3117e0ul), SI_REF(0x311800ul) };

static void set_play_bounds(uint8_t terrain_top)
{
    if (terrain_top == TERRAIN_TOP) {
        si.top = 0x10;
        si.bottom = 0x2a;
    } else {
        si.top = 6;
        si.bottom = 0x20;
    }
}

static void hud_create(void)
{
    uint8_t terrain_top = si.level == 4 || si.level == 5 ? TERRAIN_TOP : 0;
    struct sprite_image zero;
    unsigned i;

    if (si.polarity == 2)
        si.backdrop = sprite_create_fill(SPRITE_MODE_OPAQUE, 0, 0, 0, 84, 48);
    for (i = 0; i < 5; i++)
        si.life_icons[i] = si_create(SI_ICON_LIFE, SPRITE_MODE_XOR, 0, (int)(i * 6), terrain_top);
    for (i = 4; i > 2; i--)
        sprite_set_mode(si.life_icons[i], SPRITE_MODE_HIDDEN);
    si.special_icon = si_create(si_special_icon(), SPRITE_MODE_XOR, 0, 0x24, terrain_top);
    si_digit_image(&zero, 0);
    si.special_digits = sprite_create(&zero, SPRITE_MODE_XOR, 0, 0x2a, terrain_top);
    sprite_create(&zero, SPRITE_MODE_XOR, 0, 0x2e, terrain_top);
    si_draw_number(si.special_digits, (unsigned)si.specials, 2);
    si.score_digits = sprite_create(&zero, SPRITE_MODE_XOR, 0, 0x38, terrain_top);
    for (i = 1; i < 5; i++)
        sprite_create(&zero, SPRITE_MODE_XOR, 0, (int)((i + 0xe) * 4), terrain_top);
    si_draw_number(si.score_digits, si.score & 0xffff, 5);
    si.terrain_sprite = tilemap_init(&si.terrain, 16, 2, SI_ROM(level_maps[si.level]), terrain_top,
                                    si_level_tiles[si.level], si.polarity);
    tilemap_render(&si.terrain, si.scroll);
    set_play_bounds(terrain_top);
}

void si_level_load(si_ref header)
{
    const uint8_t *h = SI_ROM(header);

    si.spawn_count = h[0];
    si.spawn_list = si_rom_ref(header + 4);
    memcpy(si.checkpoints, h + 8, 4);
    si.polarity = h[13];
    si.spawns_left = si.spawn_count;
    si.checkpoint = 0;
    si.spawn_delay = 0x14;
    sprite_reset_all();
    si_objects_clear();
    si.beam_x = 0;
    si.scroll = 0;
    si.boss_parts[0] = si.boss_parts[1] = 0;
    hud_create();
    si.boss_state = 0;
    si.phase = PHASE_PLAY;
    si.pending = 0;
}

/* Restarting from the continue screen reloads the level from the state's
   own copy of the header. */
static void level_reload(void)
{
    si.spawns_left = si.spawn_count;
    si.checkpoint = 0;
    si.spawn_delay = 0x14;
    sprite_reset_all();
    si_objects_clear();
    si.beam_x = 0;
    si.scroll = 0;
    si.boss_parts[0] = si.boss_parts[1] = 0;
    hud_create();
    si.boss_state = 0;
    si.phase = PHASE_PLAY;
    si.pending = 0;
}

/* A new ship (how 10) or the next one after a loss (how 0x14), with its
   shield, which lasts as long as the one-shot timer. */
void si_ship_spawn(struct si_context *ctx, int how)
{
    uint8_t shield_mode = si.polarity == 2 ? SPRITE_MODE_XOR : SPRITE_MODE_SET;

    si.ship_lost = 0;
    si.exit_step = 0;
    if (how == 10) {
        si.ship = si_object_spawn(TYPE_SHIP, si.polarity, 5, 0x14);
        si.shield = 0;
        si.missile_target = 0;
    } else {
        si_set_template(si.ship, TYPE_SHIP);
        si_set_image(si.ship, si_type_frames[TYPE_SHIP]);
        sprite_move(si.ship, 5, 0x14);
    }
    if (!si.shield) {
        si.shield = si_object_spawn(TYPE_SHIELD, shield_mode, 3, 0x12);
    } else {
        sprite_move(si.shield, 3, 0x12);
        sprite_set_mode(si.shield, shield_mode);
    }
    ctx->one_shot = 3000;
    ctx->period = 100;
    si.pending = SI_RESULT_RESTART_TIMERS;
    si_hud_refresh();
    if (si.level == 0) {
        si_set_image(si.ship, SI_SHIP_FIRST_LEVEL);
        sprite_set_mode(si.ship, SPRITE_MODE_OPAQUE);
    }
}

int si_new_game(struct si_context *ctx)
{
    sprite_reset_all();
    si.pending = 0;
    si.countdown = 5;
    si.score = 0;
    si.fire_cooldown = 0;
    si.repeats = 0;
    si.lives = 3;
    si.continues = 4;
    si.missile_target = 0;
    si.shot_type = TYPE_SHOT;
    si.special = TYPE_MISSILE;
    si.specials = 3;
    si.level = si_first_level;
    si_level_load(si_level_table[si_first_level]);
    si_ship_spawn(ctx, 10);
    return SI_RESULT_NONE;
}

/* The continue screen: fire or special starts the level again. */
int si_continue_key(int event, struct si_context *ctx)
{
    if (event != SI_KEY_1 && event != SI_KEY_3 && event != SI_KEY_4 && event != SI_KEY_6)
        return 0;
    si.fire_cooldown = 0;
    si.repeats = 0;
    si.lives = 3;
    si.missile_target = 0;
    si.shot_type = TYPE_SHOT;
    si.special = TYPE_MISSILE;
    si.specials = 3;
    level_reload();
    si_ship_spawn(ctx, 10);
    return 1;
}

/* The screen between losing the last ship and the end: one icon per
   continue left and a countdown. */
void si_continue_enter(struct si_context *ctx)
{
    struct sprite_image zero;
    int i;

    sprite_reset_all();
    si_objects_clear();
    if (si.polarity == 2)
        si.backdrop = sprite_create_fill(SPRITE_MODE_OPAQUE, 0, 0, 0, 84, 48);
    for (i = 0; i < 4; i++)
        si.continue_icons[i] = si_create(SI_ICON_BEAM, SPRITE_MODE_XOR, 3, i * 6, 0);
    if (si.continues < 4) {
        i = 3;
        do
            sprite_set_mode(si.continue_icons[i], SPRITE_MODE_HIDDEN);
        while (--i >= si.continues);
    }
    si_digit_image(&zero, 0);
    si.terrain_sprite = sprite_create(&zero, SPRITE_MODE_XOR, 3, 0x24, 0x14);
    sprite_create(&zero, SPRITE_MODE_XOR, 3, 0x28, 0x14);
    si_draw_number(si.terrain_sprite, 5, 2);
    si.phase = PHASE_CONTINUE;
    ctx->period = 800;
    si.pending = SI_RESULT_RESTART_TICK;
}
