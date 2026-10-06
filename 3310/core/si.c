/* Space Impact. Each function follows one of the firmware's, in the same
   order of operations, because the order decides which sprite gets which
   id and when the random generators are drawn from; a frame-exact game
   needs both. The firmware map is docs/games_applications_3310.md in the
   MAME fork.

   Not done yet: suspend and resume. */
#include <stddef.h>
#include <string.h>

#include "rand.h"
#include "si_state.h"

static void shield_follow(void)
{
    if (si.shield)
        sprite_move(si.shield, sprites[si.ship].x - 2, sprites[si.ship].y - 2);
}

static void request_sound(struct game_context *ctx, uint16_t sound)
{
    ctx->sound = sound;
    if (!si.pending)
        si.pending = GAME_RESULT_SOUND;
}

static void key(int event, struct game_context *ctx)
{
    const struct sprite *ship = &sprites[si.ship];
    int repeat = event & GAME_KEY_REPEAT ? 1 : 0;
    int x = ship->x, y = ship->y;

    if (si.ship_lost == 1)
        return;
    switch (event & ~GAME_KEY_REPEAT) {
    case GAME_KEY_STAR: /* left */
        if (x <= repeat + 1)
            return;
        x = x - repeat - 1;
        break;
    case GAME_KEY_HASH: /* right */
        if (0x49 - repeat <= x)
            return;
        x = x + repeat + 1;
        break;
    case GAME_KEY_8: /* up */
        if (si.terrain.top == TERRAIN_TOP ? y <= repeat + 1 : si.terrain.top != 0 || y <= repeat + si.top)
            return;
        y = y - repeat - 1;
        break;
    case GAME_KEY_0: /* down */
        if (si.terrain.top == 0 ? si.bottom - repeat + 9 <= y : si.terrain.top != TERRAIN_TOP || si.bottom - repeat - 7 <= y)
            return;
        y = y + repeat + 1;
        break;
    case GAME_KEY_1:
    case GAME_KEY_3:
        if (repeat) {
            if (si.repeats > 4)
                return;
            if (!si.fire_cooldown) {
                si_object_spawn(si.shot_type, si.polarity, x + 6, y + 3);
                si.fire_cooldown = 1;
            }
            si.repeats++;
            return;
        }
        if (si.fire_cooldown)
            return;
        si_object_spawn(si.shot_type, si.polarity, x + 6, y + 3);
        si.fire_cooldown = 1;
        si.repeats = 0;
        request_sound(ctx, SI_SOUND_SHOT);
        return;
    case GAME_KEY_4:
    case GAME_KEY_6:
        if (repeat || si.fire_cooldown || si.specials < 1)
            return;
        if (si.special != TYPE_BEAM) {
            si_object_spawn(si.special, si.polarity, x + 6, y + 3);
            si.specials--;
            request_sound(ctx, SI_SOUND_SPECIAL);
        } else if (!si.beam_x) {
            si.beam_x = (uint8_t)(ship->image.w + ship->x);
            si.beam_sprite = sprite_create_line(si.polarity, 2, si.beam_x, si.top == 0x10 ? 0 : si.top, si.beam_x, si.bottom);
            si_set_template(si.beam_sprite, TYPE_BEAM);
            si.specials--;
            request_sound(ctx, SI_SOUND_BEAM);
        }
        si.fire_cooldown = 1;
        si_draw_number(si.special_digits, (unsigned)si.specials, 2);
        return;
    default:
        return;
    }
    sprite_move(si.ship, x, y);
    shield_follow();
}

static int is_boss(uint16_t id)
{
    switch (si.objects[id].type) {
    case 7: case 0x12: case 0x13: case 0x14: case 0x17: case 0x18: case 0x19: case TYPE_FINAL_BOSS:
        return 1;
    }
    return 0;
}

/* Runs the level's script: waits out the delay, then brings on the next
   entry's group; entries with no delay follow at once. */
static void spawn_step(void)
{
    uint8_t *counter = &si.spawn_delay;

    if (si.spawn_delay) {
    count_down:
        --*counter;
        if (si.spawn_delay)
            return;
    }
    for (;;) {
        const uint8_t *e;
        uint8_t mode;
        unsigned n, k;

        if (!si.spawns_left)
            return;
        for (k = 0; k < 4; k++)
            if (si.spawn_count - si.spawns_left == si.checkpoints[k])
                si.checkpoint = si.spawns_left;
        e = SI_ROM(si.spawn_list + 12 * (uint8_t)(si.spawn_count - si.spawns_left));
        if (e[1] == TYPE_FINAL_BOSS) {
            si.spawns_left--;
            return;
        }
        si.spawn_delay = e[5];
        mode = e[1] == 7 || e[1] == 0x13 || e[1] == 0x18 ? SPRITE_MODE_OPAQUE : si.polarity;
        for (n = 0; n < e[0]; n++) {
            unsigned y = e[6];
            uint16_t id;

            if (y == 0x3f) {
                int limit = si.bottom - SI_ROM(si_type_frames[e[1]])[9];

                y = (uint8_t)(si.top + game_rand() % si.bottom);
                if (y < si.top)
                    y = si.top;
                if (limit < (int)y)
                    y = (uint8_t)limit;
            }
            id = si_object_spawn(e[1], mode, e[4] == 5 ? -(int)(n * e[2]) : (int)(n * e[2]) + 84, (int)y);
            si.objects[id].pattern = e[4];
            si.objects[id].arg = e[3];
            si.objects[id].shots = e[8];
            si.objects[id].hp = e[9];
            si.objects[id].fire = e[7];
            if (is_boss(id)) {
                si.boss_state = 1;
                si.boss = id;
            }
        }
        counter = &si.spawns_left;
        goto count_down;
    }
}

static int fire_roll(uint16_t id)
{
    uint8_t chance = si.objects[id].fire;

    if (!chance || chance == 0x7f || game_rand16() % chance)
        return 0;
    if (si.objects[id].pattern == 0x14) {
        if (si_work.burst_pause > 0) {
            si_work.burst_pause--;
            return 0;
        }
        si_work.burst_pause = (int8_t)(game_rand16() % 3 + 2);
    }
    return 1;
}

static void enemy_fire(uint16_t id)
{
    const struct sprite *sp = &sprites[id];

    if ((is_boss(id) || si.objects[id].shots) && sp->x < 0x55) {
        si_object_spawn(TYPE_ENEMY_BULLET, si.polarity, sp->x - 2, (sp->image.h >> 1) + sp->y);
        si.objects[id].shots--;
    }
}

static void object_free(uint16_t id)
{
    sprite_free(id);
    if (si.objects_alive)
        si.objects_alive--;
    si.objects[id].type = TYPE_FREE;
    si.objects[id].no_score = 0;
    if (id == si.missile_target)
        si.missile_target = 0;
    if (id == si.boss_parts[0])
        si.boss_parts[0] = 0;
    if (id == si.boss_parts[1])
        si.boss_parts[1] = 0;
}

static int free_at_left(uint16_t id)
{
    uint8_t x = sprites[id].x;

    if (x && x < 0xfb)
        return 0;
    object_free(id);
    return 1;
}

static int free_at_right(uint16_t id)
{
    if (sprites[id].image.w + sprites[id].x <= 84)
        return 0;
    object_free(id);
    return 1;
}

/* Up and down between the limits of the play area, turning at the
   terrain. */
static void move_bounce(uint16_t id)
{
    const struct sprite *sp = &sprites[id];
    int x = sp->x, y = si_work.bounce + sp->y;

    if (tilemap_collide(&si.terrain, id, y))
        si_work.bounce = si.terrain.top == 0 ? -1 : 1;
    if ((si.terrain.top == 0 && y <= si.top) || (si.terrain.top == TERRAIN_TOP && si.bottom <= y + sp->image.h))
        si_work.bounce = (int8_t)-si_work.bounce;
    if (si.level == 0 && y + sp->image.h > 0x2f)
        si_work.bounce = (int8_t)-si_work.bounce;
    if (fire_roll(id))
        enemy_fire(id);
    if (is_boss(id))
        si.boss_state = 0x1e;
    sprite_move(id, x, y);
}

/* A boss comes on from the right until it is in place. */
static int boss_enter(uint16_t id)
{
    if (si.boss_state != 0x28 && si.boss_state != 0x14 && sprites[id].x > 0x38) {
        sprite_move(id, sprites[id].x - 1, sprites[id].y);
        return 0;
    }
    return 1;
}

static void sprite_mode_restore(uint16_t id)
{
    sprite_set_mode(id, si.polarity == 2 ? SPRITE_MODE_OPAQUE : SPRITE_MODE_SET);
}

/* A boss's own shot, of the given type, from a point on its picture. They
   come at least six ticks apart and give no score. */
static void boss_fire(uint16_t id, uint8_t type, int dx, int dy)
{
    int x = (int8_t)(sprites[id].x + dx - SI_ROM(si_type_frames[type])[8]);
    int y = (int8_t)(sprites[id].y + dy);

    if (!fire_roll(id) || si_work.boss_cooldown) {
        if (si_work.boss_cooldown > 0)
            si_work.boss_cooldown--;
        return;
    }
    si.objects[si_object_spawn(type, si.polarity, x, y)].no_score = 1;
    si_work.boss_cooldown = 6;
}

/* A boss that bounces in place and now and then charges across the screen:
   straight at the left edge and back (reach 100), or backing off to the
   right first (reach 150), which also makes the charge faster. */
static void boss_charge(uint16_t id, int reach)
{
    const struct sprite *sp = &sprites[id];
    int x = sp->x, y = sp->y;

    switch (si.boss_state) {
    case 10: /* coming back */
        if (0x4a - sp->image.w <= x)
            si.boss_state = 0x1e;
        else
            sprite_move(id, x + 1, y);
        return;
    case 0x14: /* charging */
        if (x < 9) {
            si.boss_state = 10;
            return;
        }
        x = (int16_t)(x + (reach == 100 ? -2 : -4));
        if (tilemap_collide(&si.terrain, id, y))
            y = (int16_t)(si.terrain.top == 0 ? y - 1 : y + 1);
        sprite_move(id, x, y);
        return;
    case 0x28: /* backing off */
        if (x < 0x54)
            sprite_move(id, x + 1, y);
        else
            si.boss_state = 0x14;
        return;
    }
    move_bounce(id);
    if (game_rand16() % 0x32 || si.bottom <= y || y <= si.top || x > 0x38)
        return;
    si.boss_state = reach == 100 ? 0x14 : 0x28;
}

/* The last boss once both its parts are gone: it fires for a while, backs
   off the screen, sends a wall of objects across and comes on again. */
static void final_boss_step(uint16_t id)
{
    int x = (int8_t)sprites[id].x;

    switch (si.boss_state) {
    case 0x32:
        if (x < 99)
            sprite_move(id, (int8_t)(x + 1), (int8_t)sprites[id].y);
        else
            si.boss_state = 0x3c;
        return;
    case 0x3c: {
        const uint8_t *d = SI_ROM(si_type_frames[0x1f]);
        int y;

        for (y = (int8_t)si.top; y < si.bottom - d[9]; y = (int8_t)(y + d[9] + 1)) {
            int wx = (int8_t)((d[8] + 0x2a) * 2), n;

            for (n = 0; n < 2; n++) {
                si.objects[si_object_spawn(0x1f, si.polarity, wx, y)].no_score = 1;
                wx = (int8_t)(wx - d[8]);
            }
        }
        si.boss_state = 0x46;
        return;
    }
    case 0x46:
        if (boss_enter(id))
            si.boss_state = 0x50;
        return;
    }
    boss_fire(id, 0x1d, 6, 0xe);
    if (++si_work.volley > 0x14) {
        si.boss_state = 0x32;
        si_work.volley = 0;
    }
    if (si.objects[si.boss].hp <= 1)
        si_work.volley = 0;
}

/* What a boss does once it is on the screen depends on the level. */
static void move_boss(uint16_t id)
{
    if (!si.boss_state || si.boss_state == 0x7f)
        return;
    if (si.level == 7) {
        if (!si.boss_parts[0] && !si.boss_parts[1]) {
            final_boss_step(id);
        } else {
            /* It comes on with its parts and fires from behind them. */
            if (84 - sprites[id].image.w < sprites[id].x) {
                sprite_move(id, sprites[id].x - 1, sprites[id].y);
                sprite_move(si.boss_parts[0], sprites[si.boss_parts[0]].x - 1, sprites[si.boss_parts[0]].y);
                sprite_move(si.boss_parts[1], sprites[si.boss_parts[1]].x - 1, sprites[si.boss_parts[1]].y);
            }
            boss_fire(id, 0x1d, 6, 0xe);
        }
    } else if (boss_enter(id)) {
        switch (si.level) {
        case 0:
        case 1:
            move_bounce(id);
            break;
        case 2:
            boss_charge(id, 100);
            break;
        case 3:
            move_bounce(id);
            boss_fire(id, TYPE_BOSS_SHOT, 7, 6);
            break;
        case 4:
            boss_charge(id, 100);
            if (si.boss_state == 10 || si.boss_state == 0x1e)
                boss_fire(id, 0x10, 0, 0xc);
            break;
        case 5:
            boss_charge(id, 150);
            if (si.boss_state != 0x28) {
                if (si.objects[id].frame < 2)
                    boss_fire(id, 0x0e, 4, 6);
                else
                    boss_fire(id, 0x0e, 1, 10);
            }
            break;
        case 6:
            boss_charge(id, 150);
            if (si.boss_state == 10 || si.boss_state == 0x1e)
                boss_fire(id, 6, 6, 0xe);
            break;
        }
    }
    sprite_mode_restore(id);
}

/* A part of the last boss bobs with the boss's animation. */
static void move_boss_part(uint16_t id)
{
    sprite_move(id, sprites[id].x, sprites[id].y + (si.objects[si.boss].frame < 2 ? 1 : -1));
}

/* The last level's boss is not in its script: it comes on, with two parts
   in front of it, when the terrain has scrolled to one of three places. */
static void final_boss_spawn(void)
{
    const uint8_t *e = SI_ROM(si.spawn_list + 12 * si.spawn_count - 12);
    int top = (int8_t)si.top;

    si.boss = si_object_spawn(TYPE_FINAL_BOSS, si.polarity, 0x5a, top - 1);
    si.boss_state = 1;
    si.objects[si.boss].fire = e[7];
    si.objects[si.boss].frame = 0;
    si.objects[si.boss].hp = e[9];
    si.objects[si.boss].shots = e[8];
    si.boss_parts[0] = si_object_spawn(TYPE_FINAL_BOSS_PART, si.polarity, 0x60, (int8_t)(top + 0xf));
    si.boss_parts[1] = si_object_spawn(TYPE_FINAL_BOSS_PART, si.polarity, 0x5e, (int8_t)(top + 0x16));
    si.backdrop = 0x2c;
}

/* Along the screen, ducking under the ceiling or over the floor. */
static void move_slope(uint16_t id, int speed, int side)
{
    int y = sprites[id].y, x = (int16_t)(sprites[id].x - speed), turned = 0;

    if (side == 0) {
        if (x > 84) {
            y = si.bottom;
        } else if (y >= 0x1c) {
            y--;
            turned = 1;
        }
    } else if (side == TERRAIN_TOP) {
        if (x > 84) {
            y = si.top;
        } else if (y <= 8) {
            y++;
            turned = 1;
        }
    }
    sprite_move(id, x, y);
    if (!free_at_left(id) && turned && fire_roll(id))
        enemy_fire(id);
}

/* Leftwards with y read from a table by x. */
static void move_path(uint16_t id, int speed, int relative, int offset)
{
    int top = si.top, base = relative ? (int8_t)offset : top == 0x10 ? 10 : 0;
    int x = (int16_t)(sprites[id].x - speed), y, step;
    uint8_t pattern;

    if (x < 0)
        x = 0;
    /* x modulo 84; it is under 256. */
    for (step = x; step >= 84; step -= 84)
        ;
    step = (int8_t)SI_ROM(si_work.ypath)[step];
    y = (int16_t)(base + step);
    if (y < top)
        y = top;
    sprite_move(id, x, y);
    if (free_at_left(id))
        return;
    pattern = si.objects[id].pattern;
    if (pattern == 1) {
        if (y != 0x1b && y != 9)
            return;
    } else if (pattern >= 10 && pattern <= 13) {
        if (step != 4 && step != -4)
            return;
    } else if (pattern != 15 && pattern != 16) {
        return;
    }
    if (fire_roll(id))
        enemy_fire(id);
}

/* The types that are the player's own or are only for show: the ship, its
   shots and specials, and the explosions. Nothing collides with these as
   it does with an enemy. */
const uint8_t si_player_side_types[TYPE_FINAL_EXPLOSION + 1] = {
    [TYPE_SHIP] = 1, [TYPE_SHOT] = 1, [TYPE_EXPLOSION] = 1, [TYPE_SHIELD] = 1, [TYPE_WALL] = 1,
    [TYPE_MISSILE] = 1, [TYPE_BEAM] = 1, [TYPE_FINAL_EXPLOSION] = 1,
};
#define TYPE_IS_PLAYER_SIDE(type) ((type) <= TYPE_FINAL_EXPLOSION && si_player_side_types[type])

static int is_player_side(uint16_t id)
{
    uint8_t type = si.objects[id].type;

    return TYPE_IS_PLAYER_SIDE(type);
}

/* The enemy with the most hit points. */
static uint16_t missile_pick_target(void)
{
    uint16_t id, best = 0;
    int most = -99;

    for (id = sprites[si.ship].next; id; id = sprites[id].next) {
        uint8_t type = si.objects[id].type;

        if (type != TYPE_FREE && type != TYPE_SHIP && !is_player_side(id) && most < si.objects[id].hp) {
            most = (int8_t)si.objects[id].hp;
            best = id;
        }
    }
    return best;
}

static void move_missile(uint16_t id)
{
    struct sprite *sp = &sprites[id];
    int x = sp->x, y = sp->y, aim;
    uint16_t target = si.missile_target;

    if (!target || sprites[target].x < sp->x)
        si.missile_target = target = missile_pick_target();
    if (si.objects[target].type == TYPE_FREE || si.objects[target].type == TYPE_SHIP)
        si.missile_target = target = missile_pick_target();
    if (!target) {
        sp->x++;
        return;
    }
    aim = (int16_t)((sprites[target].image.h >> 1) + sprites[target].y);
    if (y < aim && y < si.bottom)
        y++;
    else if (y > aim && y > si.top)
        y--;
    sprite_move(id, x + 2, y);
    free_at_right(id);
}

/* Leftwards, then down to the ship's row and a shot when level with it. */
static void move_dive(uint16_t id, int speed)
{
    const struct sprite *sp = &sprites[id];
    int x = sp->x, y = sp->y, limit;

    if (free_at_left(id))
        return;
    if (x < 0x2b) {
        if (x >= 0x28) {
            if (speed + si.top < y) {
                y = (int16_t)(y - speed);
                if (y == sprites[si.ship].y)
                    enemy_fire(id);
            } else {
                x = (int16_t)(x - speed);
            }
            goto done;
        }
        limit = si.bottom - speed;
    } else {
        if (84 - sp->image.w < x) {
            y = si.top;
            x = (int16_t)(x - speed);
            goto done;
        }
        limit = si.bottom - sp->image.h;
    }
    x = (int16_t)(x - speed);
    if (y < limit)
        y = (int16_t)(speed + y);
done:
    limit = si.bottom - sp->image.h;
    if (limit < y)
        y = (int16_t)limit;
    sprite_move(id, x, y);
}

static void move_left(uint16_t id, int speed)
{
    int x = sprites[id].x, y = sprites[id].y;

    if (!free_at_left(id))
        sprite_move(id, x - speed, y);
}

static void move_right(uint16_t id, int speed)
{
    int x = sprites[id].x, y = sprites[id].y;

    if (!free_at_right(id))
        sprite_move(id, x + speed, y);
    else
        object_free(id);
}

static void move_climb(uint16_t id, int speed)
{
    const struct sprite *sp = &sprites[id];
    int y = sp->y, x = (int16_t)(sp->x - speed), turned = 0;

    if (x >= 0x39) {
        y = (int16_t)(si.bottom - sp->image.h);
    } else if (x >= 0x1d && y > speed + si.top) {
        y = (int16_t)(y - speed);
        turned = 1;
    }
    sprite_move(id, x, y);
    if (!free_at_left(id) && turned && fire_roll(id))
        enemy_fire(id);
}

static void move_track_ship(uint16_t id, int speed)
{
    int y = sprites[id].y, ship_y = sprites[si.ship].y;

    if (y < ship_y)
        y++;
    else if (y > ship_y)
        y--;
    sprite_move(id, (int16_t)(sprites[id].x - speed), y);
    free_at_left(id);
}

static void move_descend(uint16_t id, int speed)
{
    const struct sprite *sp = &sprites[id];
    int y = sp->y, x = (int16_t)(sp->x - speed), turned = 0;

    if (x >= 0x39) {
        y = si.top;
    } else if (x > 0x1c && y < si.bottom - sp->image.h) {
        y = (int16_t)(speed + y);
        turned = 1;
    }
    sprite_move(id, x, y);
    if (!free_at_left(id) && turned && fire_roll(id))
        enemy_fire(id);
}

static void boss_destroyed(uint16_t id, struct game_context *ctx);

/* Bonuses and score. */
static void award(int kind, int amount, struct game_context *ctx)
{
    if (kind == TYPE_BONUS) {
        request_sound(ctx, SI_SOUND_BONUS);
        for (;;) {
            unsigned pick = game_rand16() & 3;

            if (pick == 1) {
                if (si.special == TYPE_MISSILE) {
                    si.specials += 3;
                    sprite_set_mode(si.special_icon, SPRITE_MODE_XOR);
                } else {
                    si.special = TYPE_MISSILE;
                    si_set_image(si.special_icon, SI_ICON_MISSILE);
                    si.specials = 3;
                }
                break;
            }
            if (pick == 2) {
                if (si.special != TYPE_WALL) {
                    si.special = TYPE_WALL;
                    si_set_image(si.special_icon, SI_ICON_WALL);
                    si.specials = 3;
                } else {
                    si.specials += 3;
                }
                break;
            }
            if (pick == 3) {
                if (si.special == TYPE_BEAM) {
                    si.specials++;
                } else {
                    si.special = TYPE_BEAM;
                    si_set_image(si.special_icon, SI_ICON_BEAM);
                    si.specials = 1;
                }
                break;
            }
            /* An extra life, unless there are five already. */
            if (si.lives <= 4) {
                si.lives++;
                break;
            }
        }
    } else if (kind == TYPE_BONUS_SPECIAL) {
        si.specials += 3;
    } else if (kind == 0x31) {
        if ((int32_t)si.score < 0x7b0c - amount)
            si.score += (uint32_t)amount;
        si_draw_number(si.score_digits, si.score & 0xffff, 5);
    }
    si_hud_refresh();
}

static void explode(uint16_t id)
{
    si_set_template(id, TYPE_EXPLOSION);
}

static void random_frame(uint16_t id)
{
    uint16_t r = game_rand16();

    /* No object was made when the table was full; the phone's division
       by zero gives 0. */
    si.objects[id].frame = si.objects[id].frames ? (uint8_t)(r % si.objects[id].frames) : 0;
}

static void boss_destroyed(uint16_t id, struct game_context *ctx)
{
    const struct sprite *sp = &sprites[id];
    int cx = (int16_t)(sp->x + (sp->image.w >> 1)), cy = (int16_t)(sp->y + (sp->image.h >> 1));
    uint8_t type = si.level == 7 ? TYPE_FINAL_EXPLOSION : TYPE_EXPLOSION;
    unsigned i;

    object_free(id);
    si.boss_state = 0x7f;
    si.phase = PHASE_LEVEL_EXIT;
    random_frame(si_object_spawn(type, si.polarity, cx, cy));
    for (i = 0; i < (unsigned)(si.level == 7) + 5; i++) {
        int dx, dy;

        if (si.level == 7) {
            dx = game_rand16() % 0x14;
            dy = (int8_t)(game_rand16() % 0xd + 1);
        } else {
            dx = game_rand16() & 3;
            dy = 6;
        }
        dx = (int8_t)(dx + 1);
        random_frame(si_object_spawn(type, si.polarity, (int16_t)(dx + cx), (int16_t)(dy + cy)));
        random_frame(si_object_spawn(type, si.polarity, (int16_t)(cx - dx), (int16_t)(cy - dy)));
    }
    if (si.level == 7) {
        if (si.boss_parts[0])
            object_free(si.boss_parts[0]);
        if (si.boss_parts[1])
            object_free(si.boss_parts[1]);
    }
    award(0x31, 100, ctx);
    games_vibrate();
}

/* The beam's sweep: everything within three columns of it is hit. */
static void beam_scan(struct game_context *ctx)
{
    uint16_t id, next;

    for (id = sprites[si.ship].next; id; id = next) {
        int d;

        next = sprites[id].next;
        if (is_player_side(id))
            continue;
        d = sprites[id].x - si.beam_x;
        if (d < -3 || d > 3)
            continue;
        if (si.objects[id].type == TYPE_BONUS || si.objects[id].type == TYPE_BONUS_SPECIAL)
            award(si.objects[id].type, 1, ctx);
        if (!is_boss(id) && id != si.boss_parts[0] && id != si.boss_parts[1]) {
            if (si.objects[id].no_score != 1)
                award(0x31, 10, ctx);
            explode(id);
        } else if (si.objects[id].hp < 3) {
            if (is_boss(id))
                boss_destroyed(id, ctx);
        } else {
            si.objects[id].hp -= 2;
            if (si.objects[id].no_score != 1)
                award(0x31, 10, ctx);
        }
    }
}

/* One step of every object after the terrain in the list: animation, the
   special cases, then the movement pattern. Returns early when the game
   is over. */
static void objects_step(struct game_context *ctx)
{
    uint16_t id = sprites[si.terrain_sprite].next, next;

    for (; id; id = next) {
        struct object *o = &si.objects[id];
        struct sprite *sp = &sprites[id];
        int last_frame;

        if (o->type == TYPE_FREE)
            return;
        next = sp->next;
        if (o->frames > 1) {
            /* The next frame, modulo the count; the frame is all but
               always under the count. */
            uint8_t frame = (uint8_t)(o->frame + 1);

            o->frame = frame < o->frames ? frame : frame == o->frames ? 0 : (uint8_t)(frame % o->frames);
            si_set_image(id, si_type_frames[o->type] + 12 * o->frame);
        }
        last_frame = o->frame == o->frames - 1;
        switch (o->type) {
        case TYPE_SHIP:
            continue;
        case TYPE_EXPLOSION:
            if (!last_frame)
                continue;
            if (id != si.ship) {
                object_free(id);
            } else if (--si.lives > 0) {
                si_ship_spawn(ctx, 0x14);
            } else if (si.continues >= 2) {
                si.continues--;
                si_continue_enter(ctx);
            } else {
                ctx->score = si.score;
                si.pending = GAME_RESULT_GAME_OVER;
                return;
            }
            continue;
        case TYPE_WALL: {
            const struct sprite *ship = &sprites[si.ship];

            sprite_move(id, (int16_t)(ship->image.w + ship->x + 2), (int16_t)(ship->y - (sp->image.h >> 1) + 3));
            if (last_frame)
                object_free(id);
            continue;
        }
        case TYPE_BEAM:
            if (si.beam_x >= 84) {
                sprite_free(id);
                o->type = TYPE_FREE;
                si.beam_x = 0;
            } else {
                if (si.top == 0x10)
                    sprite_set_line(id, si.beam_x, 0, si.beam_x, si.bottom);
                else
                    sprite_set_line(id, si.beam_x, si.top, si.beam_x, 0x30);
                beam_scan(ctx);
                si.beam_x += 2;
            }
            continue;
        case TYPE_FINAL_EXPLOSION:
            if (last_frame) {
                if (game_rand16() % 10 == 1) {
                    object_free(id);
                } else {
                    o->frame = (uint8_t)(game_rand16() % o->frames + 1);
                    games_vibrate();
                }
            }
            continue;
        }
        switch (o->pattern) {
        case 1:
            si_work.ypath = SI_YPATH_WAVE;
            move_path(id, o->arg, 0, 0x12);
            break;
        case 2:
            move_descend(id, o->arg);
            break;
        case 3:
            move_track_ship(id, o->arg);
            break;
        case 4:
            move_climb(id, o->arg);
            break;
        case 5:
            move_right(id, o->arg);
            break;
        case 6:
            move_left(id, o->arg);
            break;
        case 7:
            move_dive(id, o->arg);
            break;
        case 9:
            move_missile(id);
            break;
        case 10:
        case 11:
        case 12:
        case 13: {
            static const uint8_t offset[4] = { 0x23, 0x0b, 0x0e, 0x19 };

            si_work.ypath = SI_YPATH_RIPPLE;
            move_path(id, o->arg, 1, offset[o->pattern - 10]);
            break;
        }
        case 14:
            si_work.ypath = SI_YPATH_RIPPLE;
            move_path(id, o->arg, 1, 0x12);
            break;
        case 15:
            si_work.ypath = SI_YPATH_RISE;
            move_path(id, o->arg, 0, 0x12);
            break;
        case 16:
            si_work.ypath = SI_YPATH_FALL;
            move_path(id, o->arg, 0, 0x12);
            break;
        case 17:
            move_slope(id, o->arg, TERRAIN_TOP);
            break;
        case 18:
            move_slope(id, o->arg, 0);
            break;
        case 22:
            move_boss_part(id);
            sprite_mode_restore(id);
            break;
        case 23:
            move_boss(id);
            break;
        case 24:
            if (sp->x <= 0x38)
                move_bounce(id);
            else
                sprite_move(id, sp->x - 1, sp->y);
            if (si.boss_state) {
                o->pattern = 3;
                o->arg = 1;
            }
            break;
        }
    }
}

/* Whether two sprites' boxes touch. The second one's position and size are
   read as signed, so one leaving at the left still counts. */
static int sprites_overlap(uint16_t a, uint16_t b)
{
    const struct sprite *A = &sprites[a], *B = &sprites[b];
    int bx = (int8_t)B->x, by = (int8_t)B->y, bw = (int8_t)B->image.w, bh = (int8_t)B->image.h;

    return A->x <= bx + bw && bx <= A->x + A->image.w && A->y <= by + bh && by <= A->y + A->image.h;
}

/* The first object after the ship in the list that is not the player's own
   and whose box touches this sprite's: sprites_overlap for each, with this
   sprite's edges worked out once. A platform whose compiler makes slow
   work of the search may supply si_find_hit_from, which takes the edges in
   the si_find_ variables, and define SI_PLATFORM_FIND_HIT. */
#ifdef SI_PLATFORM_FIND_HIT
extern int si_find_left, si_find_right, si_find_top, si_find_bottom;
extern const uint8_t *si_find_types; /* the type of sprite 0's object */
uint16_t si_find_hit_from(uint16_t first);

/* What platform/gb/draw.s takes the records to be. */
typedef char find_hit_layout[offsetof(struct sprite, x) == 3 && offsetof(struct sprite, y) == 4
                             && offsetof(struct sprite, image.w) == 9 && offsetof(struct sprite, image.h) == 10
                             && sizeof(struct sprite) == 16 && sizeof(struct object) == 16 ? 1 : -1];

static uint16_t find_hit(uint16_t id)
{
    const struct sprite *A = &sprites[id];

    si_find_left = A->x;
    si_find_top = A->y;
    si_find_right = si_find_left + A->image.w;
    si_find_bottom = si_find_top + A->image.h;
    si_find_types = &si.objects[0].type;
    return si_find_hit_from(sprites[si.ship].next);
}
#else
static uint16_t find_hit(uint16_t id)
{
    const struct sprite *A = &sprites[id], *B;
    int left = A->x, top = A->y, right = left + A->image.w, bottom = top + A->image.h;
    uint16_t other;

    for (other = sprites[si.ship].next; other; other = B->next) {
        int bx, by;
        uint8_t type = si.objects[other].type;

        B = &sprites[other];
        if (TYPE_IS_PLAYER_SIDE(type))
            continue;
        bx = (int8_t)B->x;
        if (bx > right || left > bx + (int8_t)B->image.w)
            continue;
        by = (int8_t)B->y;
        if (by <= bottom && top <= by + (int8_t)B->image.h)
            return other;
    }
    return 0;
}
#endif

/* What counts as a solid pixel depends on how the level is drawn. */
static int solid_pair(unsigned a, unsigned b)
{
    return si.polarity == 2 ? !a && !b : si.polarity == 1 && a && b;
}

static int ship_pixel_collide(uint16_t other)
{
    const struct sprite *O = &sprites[other], *P = &sprites[si.ship];
    unsigned ship_row, other_row, ship_col, other_col, start, end, cols, rows;

    if (P->y < O->y) {
        ship_row = (uint8_t)(O->y - P->y);
        other_row = 0;
    } else {
        other_row = (uint8_t)(P->y - O->y);
        ship_row = 0;
    }
    if (P->x < O->x) {
        ship_col = (uint8_t)(O->x - P->x);
        other_col = 0;
    } else {
        other_col = (uint8_t)(P->x - O->x);
        ship_col = 0;
    }
    start = ship_col;
    end = P->image.w;
    if (O->x + O->image.w <= P->x + P->image.w) {
        start = other_col;
        end = O->image.w;
    }
    cols = (uint8_t)(end - start);
    if (P->y + P->image.h < O->y + O->image.h)
        rows = (uint8_t)(P->image.h - ship_row);
    else
        rows = (uint8_t)(O->image.h - other_row);
    for (; rows; rows--) {
        unsigned a = ship_col, b = other_col, n;

        for (n = cols; n; n--) {
            unsigned ship_bit = P->image.bitmap[P->image.w * (ship_row >> 3) + a] & 1u << (ship_row & 7);
            unsigned other_bit = O->image.bitmap[O->image.w * (other_row >> 3) + b] & 1u << (other_row & 7);

            if (solid_pair(ship_bit, other_bit))
                return 1;
            a = (a + 1) & 0xff;
            b = (b + 1) & 0xff;
        }
        ship_row = (ship_row + 1) & 0xff;
        other_row = (other_row + 1) & 0xff;
    }
    return 0;
}

/* A shot against a boss's picture along the shot's row: the row distance
   when it meets a solid pixel, else 0. */
static int boss_pixel_hit(uint16_t shot, uint16_t boss)
{
    const struct sprite *B = &sprites[boss], *A = &sprites[shot];
    int dy = (int8_t)(A->y - B->y), i;

    if (dy < 0)
        dy = -dy;
    for (i = 0; i <= A->image.w; i = (int8_t)(i + 1)) {
        int dx = A->x - B->x;
        unsigned bit;

        if (dx < 0)
            dx = -dx;
        bit = 1u << (dy & 7) & B->image.bitmap[((dy & 0xff) >> 3) * B->image.w + ((i + dx) & 0xff)];
        if (si.polarity == 2 ? !bit : si.polarity == 1 && bit)
            return dy & 0xff;
    }
    return 0;
}

static void boss_flash(uint16_t id)
{
    if (si_work.flashed == id) {
        si_work.flashed = 0;
        return;
    }
    sprite_set_mode(id, SPRITE_MODE_HIDDEN);
    si_work.flashed = id;
}

static void ship_destroy(struct game_context *ctx)
{
    explode(si.ship);
    sprite_set_mode(si.ship, si.polarity);
    si_set_image(si.ship, si_type_frames[TYPE_EXPLOSION]);
    si.ship_lost = 1;
    games_vibrate();
    request_sound(ctx, SI_SOUND_SHIP_HIT);
}

static void enemy_destroyed(uint16_t id, struct game_context *ctx)
{
    if (si.objects[id].no_score != 1)
        award(0x31, 10, ctx);
    explode(id);
}

static void collisions(struct game_context *ctx)
{
    uint16_t id, next, hit;

    if (si.ship_lost != 1) {
        if (si.level != 1 && tilemap_collide(&si.terrain, si.ship, sprites[si.ship].y))
            ship_destroy(ctx);
        if (!si.shield && si.phase != PHASE_LEVEL_EXIT && (hit = find_hit(si.ship)) != 0) {
            uint8_t type = si.objects[hit].type;

            if (type == TYPE_ENEMY_BULLET || ship_pixel_collide(hit)) {
                if (type == TYPE_BONUS || type == TYPE_BONUS_SPECIAL) {
                    award(type, 1, ctx);
                    object_free(hit);
                } else {
                    if (--si.objects[hit].hp == 0) {
                        if (is_boss(hit))
                            boss_destroyed(hit, ctx);
                        else
                            enemy_destroyed(hit, ctx);
                    }
                    ship_destroy(ctx);
                }
            }
        }
    }

    /* The player's things and loose bullets, from the sprite stored just
       before the ship: that is the terrain, so the walk starts at whatever
       follows it in the list, the shield when there is one. */
    for (id = sprites[si.ship - 1].next; id; id = next) {
        struct object *o = &si.objects[id];
        int boss, pixel_hit;

        next = sprites[id].next;
        if (o->type != TYPE_SHOT && o->type != TYPE_SHIELD && o->type != TYPE_WALL && o->type != TYPE_MISSILE
            && o->type != TYPE_BOSS_SHOT && o->type != TYPE_ENEMY_BULLET)
            continue;
        if (si.level != 1 && tilemap_collide(&si.terrain, id, sprites[id].y)) {
            int push;

            if (id != si.shield) {
                object_free(id);
                continue;
            }
            /* The shield rides over the terrain, taking the ship along. */
            push = si.terrain.top == 0 ? -2 : 2;
            sprite_move(si.ship, sprites[si.ship].x, (int16_t)(push + sprites[si.ship].y));
            sprite_move(si.shield, sprites[si.shield].x, (int16_t)(push + sprites[si.shield].y));
            continue;
        }
        if (o->type == TYPE_ENEMY_BULLET || o->type == TYPE_BOSS_SHOT || (hit = find_hit(id)) == 0)
            continue;

        boss = is_boss(hit);
        if (boss && (boss_pixel_hit(id, hit) == 0 || si.level == 7)
            && (si.level != 7 || boss_pixel_hit(id, hit) > 0x17 || boss_pixel_hit(id, hit) < 0x13)) {
            /* The shot is inside a boss's box but not on its picture, or
               on the last boss away from its weak rows. */
            pixel_hit = boss_pixel_hit(id, hit);
            if (!pixel_hit && si.level == 7) {
                int k;

                for (k = 0; k < 2; k++) {
                    uint16_t part = si.boss_parts[k];
                    struct object *p = &si.objects[part];

                    if (p->type != TYPE_FINAL_BOSS_PART || !sprites_overlap(id, part))
                        continue;
                    p->hp--;
                    object_free(id);
                    if (p->pattern == 0x16)
                        boss_flash(part);
                    if (!p->hp) {
                        p->hp = 0x32;
                        p->pattern = 6;
                        p->arg = 3;
                        sprite_mode_restore(part);
                    }
                }
            }
            if (boss_pixel_hit(id, hit) && si.level == 7)
                object_free(id);
            continue;
        }
        if (boss)
            boss_flash(hit);

        if (o->type == TYPE_SHOT || (o->type == TYPE_SHIELD && boss)) {
            si.objects[hit].hp--;
        } else if (o->type == TYPE_SHIELD) {
            si.objects[hit].hp = 0;
        } else if (o->type == TYPE_WALL || o->type == TYPE_MISSILE) {
            si.objects[hit].hp = si.objects[hit].hp < 4 ? 0 : (uint8_t)(si.objects[hit].hp - 4);
            if (hit == si.missile_target)
                o->type = TYPE_EXPLOSION;
        }
        if (si.objects[hit].hp) {
            if (si.objects[hit].no_score != 1)
                award(0x31, 5, ctx);
        } else if (boss) {
            boss_destroyed(hit, ctx);
        } else if (si.objects[hit].type == TYPE_BONUS || si.objects[hit].type == TYPE_BONUS_SPECIAL) {
            award(si.objects[hit].type, 1, ctx);
            if (next == hit)
                next = sprites[hit].next;
            object_free(hit);
        } else {
            enemy_destroyed(hit, ctx);
        }
        if (id != si.shield && o->type != TYPE_WALL && o->type != TYPE_MISSILE)
            object_free(id);
    }
}

/* After a boss: line the ship up with the top of the play area, fly it
   off to the right ever faster, then start the next level. Returns
   nonzero when that was the last level. */
static int level_exit_step(struct game_context *ctx)
{
    struct sprite *ship = &sprites[si.ship];

    if (!si.exit_step) {
        if (si.top < ship->y)
            sprite_move(si.ship, ship->x, ship->y - 1);
        else if (si.top > ship->y)
            sprite_move(si.ship, ship->x, ship->y + 1);
        else
            si.exit_step = 1;
        return 0;
    }
    sprite_move(si.ship, si.exit_step + ship->x, ship->y);
    if (ship->x % 3 == 0)
        si.exit_step++;
    if (ship->x > 0x68) {
        if (si.level > 6)
            return 1;
        si.level++;
        si_level_load(si_level_table[si.level]);
        si_ship_spawn(ctx, 10);
    }
    return 0;
}

static int tick(struct game_context *ctx)
{
    if (si.phase == PHASE_CONTINUE) {
        if (--si.countdown >= 0) {
            si_draw_number(si.terrain_sprite, (unsigned)si.countdown, 2);
            return GAME_RESULT_REDRAW;
        }
        ctx->score = si.score;
        return GAME_RESULT_GAME_OVER;
    }
    if (si.phase == PHASE_LEVEL_EXIT) {
        if (level_exit_step(ctx)) {
            ctx->score = si.score;
            return GAME_RESULT_GAME_OVER;
        }
        shield_follow();
    }
    tilemap_render(&si.terrain, si.scroll);
    if (si.spawn_delay && si.spawns_left && si.level != 7) {
        si.scroll++;
        if (si.scroll >= (uint16_t)(si.terrain.width << 5))
            si.scroll %= (uint16_t)(si.terrain.width << 5);
    } else if (si.level == 7) {
        /* The last level scrolls on to where its boss appears, and a little
           further while the boss comes on. */
        if (!si.boss_state) {
            if (si.scroll == 0x7a || si.scroll == 0x13a || si.scroll == 0x1ba)
                final_boss_spawn();
            else
                si.scroll++;
        }
        if (si.boss_state == 1) {
            if (!si.backdrop) {
                si.boss_state++;
            } else {
                si.scroll++;
                si.backdrop--;
            }
        }
    }
    spawn_step();
    objects_step(ctx);
    collisions(ctx);
    if (si.fire_cooldown)
        si.fire_cooldown = si.fire_cooldown == 2 ? 0 : (uint8_t)(si.fire_cooldown + 1);
    return GAME_RESULT_REDRAW;
}

int si_handler(int event, struct game_context *ctx)
{
    uint8_t pending;

    if (event == GAME_EVENT_START || event == 0x24) {
        ctx->period = 100;
        ctx->one_shot = 0;
        return si_new_game(ctx);
    }
    if (si.phase == PHASE_PLAY)
        key(event, ctx);
    else if (si.phase == PHASE_CONTINUE && si_continue_key(event, ctx))
        si.countdown = 5;
    /* A result left by the last event goes out now, and this event is not
       looked at further: a tick that finds one does not move the game. */
    pending = si.pending;
    if (pending) {
        si.pending = 0;
        return pending;
    }
    switch (event) {
    case GAME_EVENT_TICK:
        return tick(ctx);
    case GAME_EVENT_TIMER:
        if (si.shield) {
            object_free(si.shield);
            si.shield = 0;
        }
        return GAME_RESULT_NONE;
    case 0x13: case 0x15: case 0x17: case 0x18: case 0x1a: case 0x1c: case 0x1d:
        return GAME_RESULT_UNUSED;
    }
    return GAME_RESULT_NONE;
}

