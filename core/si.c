/* Space Impact. Each function follows one of the firmware's, in the same
   order of operations, because the order decides which sprite gets which
   id and when the random generators are drawn from; a frame-exact game
   needs both. The firmware map is docs/games_applications_3310.md in the
   MAME fork.

   Not done yet: suspend and resume. */
#include "si.h"

#include <string.h>

#include "rand.h"
#include "si_data.h"
#include "sprite.h"

#define OBJECT_COUNT 60 /* records; only 40 objects are ever alive */
#define OBJECT_LIMIT 40
#define TYPE_FREE 0x32

/* Object types the code names. */
enum {
    TYPE_SHIP = 0,
    TYPE_SHOT = 1,
    TYPE_EXPLOSION = 2,
    TYPE_ENEMY_BULLET = 4,
    TYPE_SHIELD = 8,
    TYPE_WALL = 9,
    TYPE_MISSILE = 10,
    TYPE_BONUS = 0x11,
    TYPE_BOSS_SHOT = 0x15,
    TYPE_BEAM = 0x16,
    TYPE_BONUS_SPECIAL = 0x21,
    TYPE_FINAL_BOSS = 0x22,
    TYPE_FINAL_BOSS_PART = 0x23,
    TYPE_FINAL_EXPLOSION = 0x24
};

enum {
    PHASE_PLAY = 10,
    PHASE_CONTINUE = 0x14,
    PHASE_LEVEL_EXIT = 0x1e
};

#define TERRAIN_TOP 0x2b

/* Per sprite id, the firmware's 12-byte object record. */
struct object {
    uint8_t frames;  /* animation frames */
    uint8_t frame;
    uint8_t type;
    uint8_t hp;
    uint8_t shots;   /* bullets left */
    uint8_t pattern; /* how it moves */
    uint8_t arg;     /* the pattern's argument, mostly pixels per tick */
    uint8_t fire;    /* fires one time in this many; 0 and 0x7f never */
    uint8_t saved_x, saved_y;
    uint8_t no_score;
    uint8_t pad;
};

static struct {
    struct tilemap terrain;
    uint16_t terrain_sprite; /* the continue screen keeps its countdown digits here */
    uint16_t backdrop;       /* fill behind a dark level; a countdown in the last level */
    uint16_t beam_sprite;
    uint8_t objects_alive;
    uint8_t pending;         /* result to return at the next event */
    uint8_t level;
    uint16_t score_digits, special_digits, special_icon;
    uint16_t life_icons[5];
    uint16_t continue_icons[4];
    uint8_t beam_x;          /* the beam's column, 0 when there is none */
    uint8_t boss_state;
    struct object objects[OBJECT_COUNT + 1];
    uint8_t bottom, top;     /* limits of the play area */
    /* The level's header as the firmware copies it. */
    uint8_t spawn_count;
    uint32_t spawn_list;
    uint8_t checkpoints[4];
    uint8_t checkpoint;
    uint8_t polarity;        /* 2: light sprites on a dark screen; 1: dark on light */
    uint8_t spawns_left;
    uint8_t spawn_delay;
    uint8_t phase;
    uint16_t scroll;
    int8_t countdown;
    uint16_t boss_parts[2];
    uint16_t boss;
    uint32_t score;
    uint8_t ship_lost;
    uint8_t fire_cooldown;
    uint8_t repeats;
    int8_t lives;
    uint8_t shot_type;
    uint8_t special;
    int8_t specials;
    uint16_t ship, shield, missile_target;
    int8_t continues;
    uint8_t exit_step;
} s;

/* Outside the state on the phone, and never reset by a new game. */
static struct {
    int8_t bounce;        /* direction of the vertical bounce */
    int8_t volley;        /* ticks the last boss has been firing */
    int8_t burst_pause;
    uint16_t flashed;     /* sprite hidden for a hit flash */
    uint32_t ypath;       /* the y table the path patterns read */
    int32_t boss_cooldown; /* ticks until a boss may fire again */
} work = { -1, 0, 0, 0, SI_YPATH_RIPPLE, 0 };

/* The tile sets and tile maps of the eight levels: the firmware stores
   these addresses one by one when a game starts. */
static const uint32_t tiles_a[] = { 0x312760, 0x312780, 0x3127a0 };
static const uint32_t tiles_b[] = { 0x3127e4, 0x312804, 0x312824, 0x312844 };
static const uint32_t tiles_c[] = { 0x3124b8, 0x3124d8, 0x3124f8, 0x312518 };
static const uint32_t tiles_d[] = { 0x3126b8, 0x3126d8, 0x3126f8, 0x312718 };
static const uint32_t tiles_e[] = { 0x312254, 0x312274, 0x312294, 0x3122b4 };
static const uint32_t tiles_f[] = { 0x312864, 0x312884, 0x3128a4, 0x3128c4, 0x3128e4, 0x312904 };
static const uint32_t tiles_g[] = { 0x3128a4, 0x312984, 0x312924, 0x312944, 0x312964 };
static const uint32_t *const level_tiles[8] = { tiles_a, tiles_b, tiles_c, tiles_d, tiles_e, tiles_e, tiles_f, tiles_g };
static const uint32_t level_maps[8] = { 0x311740, 0x311780, 0x311760, 0x311760, 0x3117a0, 0x3117c0, 0x3117e0, 0x311800 };

static uint32_t rom32(uint32_t address)
{
    const uint8_t *p = SI_ROM(address);

    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

void si_image(struct sprite_image *out, uint32_t descriptor)
{
    out->bitmap = SI_ROM(rom32(descriptor));
    out->w = SI_ROM(descriptor)[8];
    out->h = SI_ROM(descriptor)[9];
}

const uint8_t *tilemap_tile(uint32_t address)
{
    return SI_ROM(address);
}

static void set_image(uint16_t id, uint32_t descriptor)
{
    struct sprite_image image;

    si_image(&image, descriptor);
    sprite_set_image(id, &image);
}

static uint16_t create(uint32_t descriptor, uint8_t mode, uint8_t layer, int x, int y)
{
    struct sprite_image image;

    si_image(&image, descriptor);
    return sprite_create(&image, mode, layer, x, y);
}

static void digit_image(struct sprite_image *out, unsigned digit)
{
    out->bitmap = si_digit_glyphs + 4 * digit;
    out->w = 4;
    out->h = 5;
}

static void set_template(uint16_t id, uint8_t type)
{
    memcpy(&s.objects[id], SI_ROM(SI_TEMPLATES + 12ul * type), 12);
}

static void objects_clear(void)
{
    int i;

    for (i = 0; i < OBJECT_COUNT; i++) {
        s.objects[i].type = TYPE_FREE;
        s.objects[i].frames = 0;
        s.objects[i].frame = 0;
        s.objects[i].no_score = 0;
    }
    s.objects_alive = 0;
}

/* Writes a number into a run of digit sprites, which must follow each
   other in the list. */
static void draw_number(uint16_t id, unsigned value, int digits)
{
    uint8_t buf[5] = { 0, 0, 0, 0, 0 };
    struct sprite_image image;
    int i;

    for (i = 4; i >= 0 && value; i--, value /= 10)
        buf[i] = (uint8_t)(value % 10);
    for (i = 5 - digits; i < 5; i++) {
        digit_image(&image, buf[i]);
        sprite_set_image(id, &image);
        id = sprites[id].next;
    }
}

static void set_play_bounds(uint8_t terrain_top)
{
    if (terrain_top == TERRAIN_TOP) {
        s.top = 0x10;
        s.bottom = 0x2a;
    } else {
        s.top = 6;
        s.bottom = 0x20;
    }
}

static uint32_t special_icon(void)
{
    return s.special == TYPE_MISSILE ? SI_ICON_MISSILE : s.special == TYPE_BEAM ? SI_ICON_BEAM : SI_ICON_WALL;
}

static void hud_create(void)
{
    uint8_t terrain_top = s.level == 4 || s.level == 5 ? TERRAIN_TOP : 0;
    struct sprite_image zero;
    unsigned i;

    if (s.polarity == 2)
        s.backdrop = sprite_create_fill(SPRITE_MODE_OPAQUE, 0, 0, 0, 84, 48);
    for (i = 0; i < 5; i++)
        s.life_icons[i] = create(SI_ICON_LIFE, SPRITE_MODE_XOR, 0, (int)(i * 6), terrain_top);
    for (i = 4; i > 2; i--)
        sprite_set_mode(s.life_icons[i], SPRITE_MODE_HIDDEN);
    s.special_icon = create(special_icon(), SPRITE_MODE_XOR, 0, 0x24, terrain_top);
    digit_image(&zero, 0);
    s.special_digits = sprite_create(&zero, SPRITE_MODE_XOR, 0, 0x2a, terrain_top);
    sprite_create(&zero, SPRITE_MODE_XOR, 0, 0x2e, terrain_top);
    draw_number(s.special_digits, (unsigned)s.specials, 2);
    s.score_digits = sprite_create(&zero, SPRITE_MODE_XOR, 0, 0x38, terrain_top);
    for (i = 1; i < 5; i++)
        sprite_create(&zero, SPRITE_MODE_XOR, 0, (int)((i + 0xe) * 4), terrain_top);
    draw_number(s.score_digits, s.score & 0xffff, 5);
    s.terrain_sprite = tilemap_init(&s.terrain, 16, 2, SI_ROM(level_maps[s.level]), terrain_top,
                                    level_tiles[s.level], s.polarity);
    tilemap_render(&s.terrain, s.scroll);
    set_play_bounds(terrain_top);
}

static void level_load(uint32_t header)
{
    const uint8_t *h = SI_ROM(header);

    s.spawn_count = h[0];
    s.spawn_list = rom32(header + 4);
    memcpy(s.checkpoints, h + 8, 4);
    s.polarity = h[13];
    s.spawns_left = s.spawn_count;
    s.checkpoint = 0;
    s.spawn_delay = 0x14;
    sprite_reset_all();
    objects_clear();
    s.beam_x = 0;
    s.scroll = 0;
    s.boss_parts[0] = s.boss_parts[1] = 0;
    hud_create();
    s.boss_state = 0;
    s.phase = PHASE_PLAY;
    s.pending = 0;
}

/* Restarting from the continue screen reloads the level from the state's
   own copy of the header. */
static void level_reload(void)
{
    s.spawns_left = s.spawn_count;
    s.checkpoint = 0;
    s.spawn_delay = 0x14;
    sprite_reset_all();
    objects_clear();
    s.beam_x = 0;
    s.scroll = 0;
    s.boss_parts[0] = s.boss_parts[1] = 0;
    hud_create();
    s.boss_state = 0;
    s.phase = PHASE_PLAY;
    s.pending = 0;
}

static uint16_t object_spawn(uint8_t type, uint8_t mode, int x, int y)
{
    uint16_t id;

    if (s.objects_alive >= OBJECT_LIMIT)
        return 0;
    id = create(si_type_frames[type], mode, (uint8_t)(2 - (type == TYPE_SHIELD)), x, y);
    set_template(id, type);
    s.objects_alive++;
    return id;
}

static void hud_refresh(void)
{
    int i;

    draw_number(s.special_digits, (unsigned)s.specials, 2);
    draw_number(s.score_digits, s.score & 0xffff, 5);
    for (i = 0; i < 5; i++)
        sprite_set_mode(s.life_icons[i], i < s.lives ? SPRITE_MODE_XOR : SPRITE_MODE_HIDDEN);
}

/* A new ship (how 10) or the next one after a loss (how 0x14), with its
   shield, which lasts as long as the one-shot timer. */
static void ship_spawn(struct si_context *ctx, int how)
{
    uint8_t shield_mode = s.polarity == 2 ? SPRITE_MODE_XOR : SPRITE_MODE_SET;

    s.ship_lost = 0;
    s.exit_step = 0;
    if (how == 10) {
        s.ship = object_spawn(TYPE_SHIP, s.polarity, 5, 0x14);
        s.shield = 0;
        s.missile_target = 0;
    } else {
        set_template(s.ship, TYPE_SHIP);
        set_image(s.ship, si_type_frames[TYPE_SHIP]);
        sprite_move(s.ship, 5, 0x14);
    }
    if (!s.shield) {
        s.shield = object_spawn(TYPE_SHIELD, shield_mode, 3, 0x12);
    } else {
        sprite_move(s.shield, 3, 0x12);
        sprite_set_mode(s.shield, shield_mode);
    }
    ctx->one_shot = 3000;
    ctx->period = 100;
    s.pending = SI_RESULT_RESTART_TIMERS;
    hud_refresh();
    if (s.level == 0) {
        set_image(s.ship, SI_SHIP_FIRST_LEVEL);
        sprite_set_mode(s.ship, SPRITE_MODE_OPAQUE);
    }
}

static int new_game(struct si_context *ctx)
{
    sprite_reset_all();
    s.pending = 0;
    s.countdown = 5;
    s.score = 0;
    s.fire_cooldown = 0;
    s.repeats = 0;
    s.lives = 3;
    s.continues = 4;
    s.missile_target = 0;
    s.shot_type = TYPE_SHOT;
    s.special = TYPE_MISSILE;
    s.specials = 3;
    s.level = 0;
    level_load(si_level_table[0]);
    ship_spawn(ctx, 10);
    return SI_RESULT_NONE;
}

/* The continue screen: fire or special starts the level again. */
static int continue_key(int event, struct si_context *ctx)
{
    if (event != SI_KEY_1 && event != SI_KEY_3 && event != SI_KEY_4 && event != SI_KEY_6)
        return 0;
    s.fire_cooldown = 0;
    s.repeats = 0;
    s.lives = 3;
    s.missile_target = 0;
    s.shot_type = TYPE_SHOT;
    s.special = TYPE_MISSILE;
    s.specials = 3;
    level_reload();
    ship_spawn(ctx, 10);
    return 1;
}

static void shield_follow(void)
{
    if (s.shield)
        sprite_move(s.shield, sprites[s.ship].x - 2, sprites[s.ship].y - 2);
}

static void request_sound(struct si_context *ctx, uint16_t sound)
{
    ctx->sound = sound;
    if (!s.pending)
        s.pending = SI_RESULT_SOUND;
}

static void key(int event, struct si_context *ctx)
{
    const struct sprite *ship = &sprites[s.ship];
    int repeat = event & SI_KEY_REPEAT ? 1 : 0;
    int x = ship->x, y = ship->y;

    if (s.ship_lost == 1)
        return;
    switch (event & ~SI_KEY_REPEAT) {
    case SI_KEY_STAR: /* left */
        if (x <= repeat + 1)
            return;
        x = x - repeat - 1;
        break;
    case SI_KEY_HASH: /* right */
        if (0x49 - repeat <= x)
            return;
        x = x + repeat + 1;
        break;
    case SI_KEY_8: /* up */
        if (s.terrain.top == TERRAIN_TOP ? y <= repeat + 1 : s.terrain.top != 0 || y <= repeat + s.top)
            return;
        y = y - repeat - 1;
        break;
    case SI_KEY_0: /* down */
        if (s.terrain.top == 0 ? s.bottom - repeat + 9 <= y : s.terrain.top != TERRAIN_TOP || s.bottom - repeat - 7 <= y)
            return;
        y = y + repeat + 1;
        break;
    case SI_KEY_1:
    case SI_KEY_3:
        if (repeat) {
            if (s.repeats > 4)
                return;
            if (!s.fire_cooldown) {
                object_spawn(s.shot_type, s.polarity, x + 6, y + 3);
                s.fire_cooldown = 1;
            }
            s.repeats++;
            return;
        }
        if (s.fire_cooldown)
            return;
        object_spawn(s.shot_type, s.polarity, x + 6, y + 3);
        s.fire_cooldown = 1;
        s.repeats = 0;
        request_sound(ctx, SI_SOUND_SHOT);
        return;
    case SI_KEY_4:
    case SI_KEY_6:
        if (repeat || s.fire_cooldown || s.specials < 1)
            return;
        if (s.special != TYPE_BEAM) {
            object_spawn(s.special, s.polarity, x + 6, y + 3);
            s.specials--;
            request_sound(ctx, SI_SOUND_SPECIAL);
        } else if (!s.beam_x) {
            s.beam_x = (uint8_t)(ship->image.w + ship->x);
            s.beam_sprite = sprite_create_line(s.polarity, 2, s.beam_x, s.top == 0x10 ? 0 : s.top, s.beam_x, s.bottom);
            set_template(s.beam_sprite, TYPE_BEAM);
            s.specials--;
            request_sound(ctx, SI_SOUND_BEAM);
        }
        s.fire_cooldown = 1;
        draw_number(s.special_digits, (unsigned)s.specials, 2);
        return;
    default:
        return;
    }
    sprite_move(s.ship, x, y);
    shield_follow();
}

/* The screen between losing the last ship and the end: one icon per
   continue left and a countdown. */
static void continue_enter(struct si_context *ctx)
{
    struct sprite_image zero;
    int i;

    sprite_reset_all();
    objects_clear();
    if (s.polarity == 2)
        s.backdrop = sprite_create_fill(SPRITE_MODE_OPAQUE, 0, 0, 0, 84, 48);
    for (i = 0; i < 4; i++)
        s.continue_icons[i] = create(SI_ICON_BEAM, SPRITE_MODE_XOR, 3, i * 6, 0);
    if (s.continues < 4) {
        i = 3;
        do
            sprite_set_mode(s.continue_icons[i], SPRITE_MODE_HIDDEN);
        while (--i >= s.continues);
    }
    digit_image(&zero, 0);
    s.terrain_sprite = sprite_create(&zero, SPRITE_MODE_XOR, 3, 0x24, 0x14);
    sprite_create(&zero, SPRITE_MODE_XOR, 3, 0x28, 0x14);
    draw_number(s.terrain_sprite, 5, 2);
    s.phase = PHASE_CONTINUE;
    ctx->period = 800;
    s.pending = SI_RESULT_RESTART_TICK;
}

static int is_boss(uint16_t id)
{
    switch (s.objects[id].type) {
    case 7: case 0x12: case 0x13: case 0x14: case 0x17: case 0x18: case 0x19: case TYPE_FINAL_BOSS:
        return 1;
    }
    return 0;
}

/* Runs the level's script: waits out the delay, then brings on the next
   entry's group; entries with no delay follow at once. */
static void spawn_step(void)
{
    uint8_t *counter = &s.spawn_delay;

    if (s.spawn_delay) {
    count_down:
        --*counter;
        if (s.spawn_delay)
            return;
    }
    for (;;) {
        const uint8_t *e;
        uint8_t mode;
        unsigned n, k;

        if (!s.spawns_left)
            return;
        for (k = 0; k < 4; k++)
            if (s.spawn_count - s.spawns_left == s.checkpoints[k])
                s.checkpoint = s.spawns_left;
        e = SI_ROM(s.spawn_list + 12ul * (uint8_t)(s.spawn_count - s.spawns_left));
        if (e[1] == TYPE_FINAL_BOSS) {
            s.spawns_left--;
            return;
        }
        s.spawn_delay = e[5];
        mode = e[1] == 7 || e[1] == 0x13 || e[1] == 0x18 ? SPRITE_MODE_OPAQUE : s.polarity;
        for (n = 0; n < e[0]; n++) {
            unsigned y = e[6];
            uint16_t id;

            if (y == 0x3f) {
                int limit = s.bottom - SI_ROM(si_type_frames[e[1]])[9];

                y = (uint8_t)(s.top + game_rand() % s.bottom);
                if (y < s.top)
                    y = s.top;
                if (limit < (int)y)
                    y = (uint8_t)limit;
            }
            id = object_spawn(e[1], mode, e[4] == 5 ? -(int)(n * e[2]) : (int)(n * e[2]) + 84, (int)y);
            s.objects[id].pattern = e[4];
            s.objects[id].arg = e[3];
            s.objects[id].shots = e[8];
            s.objects[id].hp = e[9];
            s.objects[id].fire = e[7];
            if (is_boss(id)) {
                s.boss_state = 1;
                s.boss = id;
            }
        }
        counter = &s.spawns_left;
        goto count_down;
    }
}

static int fire_roll(uint16_t id)
{
    uint8_t chance = s.objects[id].fire;

    if (!chance || chance == 0x7f || game_rand16() % chance)
        return 0;
    if (s.objects[id].pattern == 0x14) {
        if (work.burst_pause > 0) {
            work.burst_pause--;
            return 0;
        }
        work.burst_pause = (int8_t)(game_rand16() % 3 + 2);
    }
    return 1;
}

static void enemy_fire(uint16_t id)
{
    const struct sprite *sp = &sprites[id];

    if ((is_boss(id) || s.objects[id].shots) && sp->x < 0x55) {
        object_spawn(TYPE_ENEMY_BULLET, s.polarity, sp->x - 2, (sp->image.h >> 1) + sp->y);
        s.objects[id].shots--;
    }
}

static void object_free(uint16_t id)
{
    sprite_free(id);
    if (s.objects_alive)
        s.objects_alive--;
    s.objects[id].type = TYPE_FREE;
    s.objects[id].no_score = 0;
    if (id == s.missile_target)
        s.missile_target = 0;
    if (id == s.boss_parts[0])
        s.boss_parts[0] = 0;
    if (id == s.boss_parts[1])
        s.boss_parts[1] = 0;
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
    int x = sp->x, y = work.bounce + sp->y;

    if (tilemap_collide(&s.terrain, id, y))
        work.bounce = s.terrain.top == 0 ? -1 : 1;
    if ((s.terrain.top == 0 && y <= s.top) || (s.terrain.top == TERRAIN_TOP && s.bottom <= y + sp->image.h))
        work.bounce = (int8_t)-work.bounce;
    if (s.level == 0 && y + sp->image.h > 0x2f)
        work.bounce = (int8_t)-work.bounce;
    if (fire_roll(id))
        enemy_fire(id);
    if (is_boss(id))
        s.boss_state = 0x1e;
    sprite_move(id, x, y);
}

/* A boss comes on from the right until it is in place. */
static int boss_enter(uint16_t id)
{
    if (s.boss_state != 0x28 && s.boss_state != 0x14 && sprites[id].x > 0x38) {
        sprite_move(id, sprites[id].x - 1, sprites[id].y);
        return 0;
    }
    return 1;
}

static void sprite_mode_restore(uint16_t id)
{
    sprite_set_mode(id, s.polarity == 2 ? SPRITE_MODE_OPAQUE : SPRITE_MODE_SET);
}

/* A boss's own shot, of the given type, from a point on its picture. They
   come at least six ticks apart and give no score. */
static void boss_fire(uint16_t id, uint8_t type, int dx, int dy)
{
    int x = (int8_t)(sprites[id].x + dx - SI_ROM(si_type_frames[type])[8]);
    int y = (int8_t)(sprites[id].y + dy);

    if (!fire_roll(id) || work.boss_cooldown) {
        if (work.boss_cooldown > 0)
            work.boss_cooldown--;
        return;
    }
    s.objects[object_spawn(type, s.polarity, x, y)].no_score = 1;
    work.boss_cooldown = 6;
}

/* A boss that bounces in place and now and then charges across the screen:
   straight at the left edge and back (reach 100), or backing off to the
   right first (reach 150), which also makes the charge faster. */
static void boss_charge(uint16_t id, int reach)
{
    const struct sprite *sp = &sprites[id];
    int x = sp->x, y = sp->y;

    switch (s.boss_state) {
    case 10: /* coming back */
        if (0x4a - sp->image.w <= x)
            s.boss_state = 0x1e;
        else
            sprite_move(id, x + 1, y);
        return;
    case 0x14: /* charging */
        if (x < 9) {
            s.boss_state = 10;
            return;
        }
        x = (int16_t)(x + (reach == 100 ? -2 : -4));
        if (tilemap_collide(&s.terrain, id, y))
            y = (int16_t)(s.terrain.top == 0 ? y - 1 : y + 1);
        sprite_move(id, x, y);
        return;
    case 0x28: /* backing off */
        if (x < 0x54)
            sprite_move(id, x + 1, y);
        else
            s.boss_state = 0x14;
        return;
    }
    move_bounce(id);
    if (game_rand16() % 0x32 || s.bottom <= y || y <= s.top || x > 0x38)
        return;
    s.boss_state = reach == 100 ? 0x14 : 0x28;
}

/* The last boss once both its parts are gone: it fires for a while, backs
   off the screen, sends a wall of objects across and comes on again. */
static void final_boss_step(uint16_t id)
{
    int x = (int8_t)sprites[id].x;

    switch (s.boss_state) {
    case 0x32:
        if (x < 99)
            sprite_move(id, (int8_t)(x + 1), (int8_t)sprites[id].y);
        else
            s.boss_state = 0x3c;
        return;
    case 0x3c: {
        const uint8_t *d = SI_ROM(si_type_frames[0x1f]);
        int y;

        for (y = (int8_t)s.top; y < s.bottom - d[9]; y = (int8_t)(y + d[9] + 1)) {
            int wx = (int8_t)((d[8] + 0x2a) * 2), n;

            for (n = 0; n < 2; n++) {
                s.objects[object_spawn(0x1f, s.polarity, wx, y)].no_score = 1;
                wx = (int8_t)(wx - d[8]);
            }
        }
        s.boss_state = 0x46;
        return;
    }
    case 0x46:
        if (boss_enter(id))
            s.boss_state = 0x50;
        return;
    }
    boss_fire(id, 0x1d, 6, 0xe);
    if (++work.volley > 0x14) {
        s.boss_state = 0x32;
        work.volley = 0;
    }
    if (s.objects[s.boss].hp <= 1)
        work.volley = 0;
}

/* What a boss does once it is on the screen depends on the level. */
static void move_boss(uint16_t id)
{
    if (!s.boss_state || s.boss_state == 0x7f)
        return;
    if (s.level == 7) {
        if (!s.boss_parts[0] && !s.boss_parts[1]) {
            final_boss_step(id);
        } else {
            /* It comes on with its parts and fires from behind them. */
            if (84 - sprites[id].image.w < sprites[id].x) {
                sprite_move(id, sprites[id].x - 1, sprites[id].y);
                sprite_move(s.boss_parts[0], sprites[s.boss_parts[0]].x - 1, sprites[s.boss_parts[0]].y);
                sprite_move(s.boss_parts[1], sprites[s.boss_parts[1]].x - 1, sprites[s.boss_parts[1]].y);
            }
            boss_fire(id, 0x1d, 6, 0xe);
        }
    } else if (boss_enter(id)) {
        switch (s.level) {
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
            if (s.boss_state == 10 || s.boss_state == 0x1e)
                boss_fire(id, 0x10, 0, 0xc);
            break;
        case 5:
            boss_charge(id, 150);
            if (s.boss_state != 0x28) {
                if (s.objects[id].frame < 2)
                    boss_fire(id, 0x0e, 4, 6);
                else
                    boss_fire(id, 0x0e, 1, 10);
            }
            break;
        case 6:
            boss_charge(id, 150);
            if (s.boss_state == 10 || s.boss_state == 0x1e)
                boss_fire(id, 6, 6, 0xe);
            break;
        }
    }
    sprite_mode_restore(id);
}

/* A part of the last boss bobs with the boss's animation. */
static void move_boss_part(uint16_t id)
{
    sprite_move(id, sprites[id].x, sprites[id].y + (s.objects[s.boss].frame < 2 ? 1 : -1));
}

/* The last level's boss is not in its script: it comes on, with two parts
   in front of it, when the terrain has scrolled to one of three places. */
static void final_boss_spawn(void)
{
    const uint8_t *e = SI_ROM(s.spawn_list + 12ul * s.spawn_count - 12);
    int top = (int8_t)s.top;

    s.boss = object_spawn(TYPE_FINAL_BOSS, s.polarity, 0x5a, top - 1);
    s.boss_state = 1;
    s.objects[s.boss].fire = e[7];
    s.objects[s.boss].frame = 0;
    s.objects[s.boss].hp = e[9];
    s.objects[s.boss].shots = e[8];
    s.boss_parts[0] = object_spawn(TYPE_FINAL_BOSS_PART, s.polarity, 0x60, (int8_t)(top + 0xf));
    s.boss_parts[1] = object_spawn(TYPE_FINAL_BOSS_PART, s.polarity, 0x5e, (int8_t)(top + 0x16));
    s.backdrop = 0x2c;
}

/* Along the screen, ducking under the ceiling or over the floor. */
static void move_slope(uint16_t id, int speed, int side)
{
    int y = sprites[id].y, x = (int16_t)(sprites[id].x - speed), turned = 0;

    if (side == 0) {
        if (x > 84) {
            y = s.bottom;
        } else if (y >= 0x1c) {
            y--;
            turned = 1;
        }
    } else if (side == TERRAIN_TOP) {
        if (x > 84) {
            y = s.top;
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
    int top = s.top, base = relative ? (int8_t)offset : top == 0x10 ? 10 : 0;
    int x = (int16_t)(sprites[id].x - speed), y, step;
    uint8_t pattern;

    if (x < 0)
        x = 0;
    step = (int8_t)SI_ROM(work.ypath)[x % 84];
    y = (int16_t)(base + step);
    if (y < top)
        y = top;
    sprite_move(id, x, y);
    if (free_at_left(id))
        return;
    pattern = s.objects[id].pattern;
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

static int is_player_side(uint16_t id)
{
    uint8_t type = s.objects[id].type;

    return type == TYPE_SHIP || type == TYPE_EXPLOSION || type == TYPE_SHOT || type == TYPE_SHIELD || type == TYPE_WALL
           || type == TYPE_MISSILE || type == TYPE_BEAM || type == TYPE_FINAL_EXPLOSION;
}

/* The enemy with the most hit points. */
static uint16_t missile_pick_target(void)
{
    uint16_t id, best = 0;
    int most = -99;

    for (id = sprites[s.ship].next; id; id = sprites[id].next) {
        uint8_t type = s.objects[id].type;

        if (type != TYPE_FREE && type != TYPE_SHIP && !is_player_side(id) && most < s.objects[id].hp) {
            most = (int8_t)s.objects[id].hp;
            best = id;
        }
    }
    return best;
}

static void move_missile(uint16_t id)
{
    struct sprite *sp = &sprites[id];
    int x = sp->x, y = sp->y, aim;
    uint16_t target = s.missile_target;

    if (!target || sprites[target].x < sp->x)
        s.missile_target = target = missile_pick_target();
    if (s.objects[target].type == TYPE_FREE || s.objects[target].type == TYPE_SHIP)
        s.missile_target = target = missile_pick_target();
    if (!target) {
        sp->x++;
        return;
    }
    aim = (int16_t)((sprites[target].image.h >> 1) + sprites[target].y);
    if (y < aim && y < s.bottom)
        y++;
    else if (y > aim && y > s.top)
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
            if (speed + s.top < y) {
                y = (int16_t)(y - speed);
                if (y == sprites[s.ship].y)
                    enemy_fire(id);
            } else {
                x = (int16_t)(x - speed);
            }
            goto done;
        }
        limit = s.bottom - speed;
    } else {
        if (84 - sp->image.w < x) {
            y = s.top;
            x = (int16_t)(x - speed);
            goto done;
        }
        limit = s.bottom - sp->image.h;
    }
    x = (int16_t)(x - speed);
    if (y < limit)
        y = (int16_t)(speed + y);
done:
    limit = s.bottom - sp->image.h;
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
        y = (int16_t)(s.bottom - sp->image.h);
    } else if (x >= 0x1d && y > speed + s.top) {
        y = (int16_t)(y - speed);
        turned = 1;
    }
    sprite_move(id, x, y);
    if (!free_at_left(id) && turned && fire_roll(id))
        enemy_fire(id);
}

static void move_track_ship(uint16_t id, int speed)
{
    int y = sprites[id].y, ship_y = sprites[s.ship].y;

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
        y = s.top;
    } else if (x > 0x1c && y < s.bottom - sp->image.h) {
        y = (int16_t)(speed + y);
        turned = 1;
    }
    sprite_move(id, x, y);
    if (!free_at_left(id) && turned && fire_roll(id))
        enemy_fire(id);
}

static void boss_destroyed(uint16_t id, struct si_context *ctx);

/* Bonuses and score. */
static void award(int kind, int amount, struct si_context *ctx)
{
    if (kind == TYPE_BONUS) {
        request_sound(ctx, SI_SOUND_BONUS);
        for (;;) {
            unsigned pick = game_rand16() & 3;

            if (pick == 1) {
                if (s.special == TYPE_MISSILE) {
                    s.specials += 3;
                    sprite_set_mode(s.special_icon, SPRITE_MODE_XOR);
                } else {
                    s.special = TYPE_MISSILE;
                    set_image(s.special_icon, SI_ICON_MISSILE);
                    s.specials = 3;
                }
                break;
            }
            if (pick == 2) {
                if (s.special != TYPE_WALL) {
                    s.special = TYPE_WALL;
                    set_image(s.special_icon, SI_ICON_WALL);
                    s.specials = 3;
                } else {
                    s.specials += 3;
                }
                break;
            }
            if (pick == 3) {
                if (s.special == TYPE_BEAM) {
                    s.specials++;
                } else {
                    s.special = TYPE_BEAM;
                    set_image(s.special_icon, SI_ICON_BEAM);
                    s.specials = 1;
                }
                break;
            }
            /* An extra life, unless there are five already. */
            if (s.lives <= 4) {
                s.lives++;
                break;
            }
        }
    } else if (kind == TYPE_BONUS_SPECIAL) {
        s.specials += 3;
    } else if (kind == 0x31) {
        if ((int32_t)s.score < 0x7b0c - amount)
            s.score += (uint32_t)amount;
        draw_number(s.score_digits, s.score & 0xffff, 5);
    }
    hud_refresh();
}

static void explode(uint16_t id)
{
    set_template(id, TYPE_EXPLOSION);
}

static void random_frame(uint16_t id)
{
    uint16_t r = game_rand16();

    /* No object was made when the table was full; the phone's division
       by zero gives 0. */
    s.objects[id].frame = s.objects[id].frames ? (uint8_t)(r % s.objects[id].frames) : 0;
}

static void boss_destroyed(uint16_t id, struct si_context *ctx)
{
    const struct sprite *sp = &sprites[id];
    int cx = (int16_t)(sp->x + (sp->image.w >> 1)), cy = (int16_t)(sp->y + (sp->image.h >> 1));
    uint8_t type = s.level == 7 ? TYPE_FINAL_EXPLOSION : TYPE_EXPLOSION;
    unsigned i;

    object_free(id);
    s.boss_state = 0x7f;
    s.phase = PHASE_LEVEL_EXIT;
    random_frame(object_spawn(type, s.polarity, cx, cy));
    for (i = 0; i < (unsigned)(s.level == 7) + 5; i++) {
        int dx, dy;

        if (s.level == 7) {
            dx = game_rand16() % 0x14;
            dy = (int8_t)(game_rand16() % 0xd + 1);
        } else {
            dx = game_rand16() & 3;
            dy = 6;
        }
        dx = (int8_t)(dx + 1);
        random_frame(object_spawn(type, s.polarity, (int16_t)(dx + cx), (int16_t)(dy + cy)));
        random_frame(object_spawn(type, s.polarity, (int16_t)(cx - dx), (int16_t)(cy - dy)));
    }
    if (s.level == 7) {
        if (s.boss_parts[0])
            object_free(s.boss_parts[0]);
        if (s.boss_parts[1])
            object_free(s.boss_parts[1]);
    }
    award(0x31, 100, ctx);
    platform_vibrate();
}

/* The beam's sweep: everything within three columns of it is hit. */
static void beam_scan(struct si_context *ctx)
{
    uint16_t id, next;

    for (id = sprites[s.ship].next; id; id = next) {
        int d;

        next = sprites[id].next;
        if (is_player_side(id))
            continue;
        d = sprites[id].x - s.beam_x;
        if (d < -3 || d > 3)
            continue;
        if (s.objects[id].type == TYPE_BONUS || s.objects[id].type == TYPE_BONUS_SPECIAL)
            award(s.objects[id].type, 1, ctx);
        if (!is_boss(id) && id != s.boss_parts[0] && id != s.boss_parts[1]) {
            if (s.objects[id].no_score != 1)
                award(0x31, 10, ctx);
            explode(id);
        } else if (s.objects[id].hp < 3) {
            if (is_boss(id))
                boss_destroyed(id, ctx);
        } else {
            s.objects[id].hp -= 2;
            if (s.objects[id].no_score != 1)
                award(0x31, 10, ctx);
        }
    }
}

/* One step of every object after the terrain in the list: animation, the
   special cases, then the movement pattern. Returns early when the game
   is over. */
static void objects_step(struct si_context *ctx)
{
    uint16_t id = sprites[s.terrain_sprite].next, next;

    for (; id; id = next) {
        struct object *o = &s.objects[id];
        struct sprite *sp = &sprites[id];
        int last_frame;

        if (o->type == TYPE_FREE)
            return;
        next = sp->next;
        if (o->frames > 1) {
            o->frame = (uint8_t)((o->frame + 1) % o->frames);
            set_image(id, si_type_frames[o->type] + 12ul * o->frame);
        }
        last_frame = o->frame == o->frames - 1;
        switch (o->type) {
        case TYPE_SHIP:
            continue;
        case TYPE_EXPLOSION:
            if (!last_frame)
                continue;
            if (id != s.ship) {
                object_free(id);
            } else if (--s.lives > 0) {
                ship_spawn(ctx, 0x14);
            } else if (s.continues >= 2) {
                s.continues--;
                continue_enter(ctx);
            } else {
                ctx->score = s.score;
                s.pending = SI_RESULT_GAME_OVER;
                return;
            }
            continue;
        case TYPE_WALL: {
            const struct sprite *ship = &sprites[s.ship];

            sprite_move(id, (int16_t)(ship->image.w + ship->x + 2), (int16_t)(ship->y - (sp->image.h >> 1) + 3));
            if (last_frame)
                object_free(id);
            continue;
        }
        case TYPE_BEAM:
            if (s.beam_x >= 84) {
                sprite_free(id);
                o->type = TYPE_FREE;
                s.beam_x = 0;
            } else {
                if (s.top == 0x10)
                    sprite_set_line(id, s.beam_x, 0, s.beam_x, s.bottom);
                else
                    sprite_set_line(id, s.beam_x, s.top, s.beam_x, 0x30);
                beam_scan(ctx);
                s.beam_x += 2;
            }
            continue;
        case TYPE_FINAL_EXPLOSION:
            if (last_frame) {
                if (game_rand16() % 10 == 1) {
                    object_free(id);
                } else {
                    o->frame = (uint8_t)(game_rand16() % o->frames + 1);
                    platform_vibrate();
                }
            }
            continue;
        }
        switch (o->pattern) {
        case 1:
            work.ypath = SI_YPATH_WAVE;
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

            work.ypath = SI_YPATH_RIPPLE;
            move_path(id, o->arg, 1, offset[o->pattern - 10]);
            break;
        }
        case 14:
            work.ypath = SI_YPATH_RIPPLE;
            move_path(id, o->arg, 1, 0x12);
            break;
        case 15:
            work.ypath = SI_YPATH_RISE;
            move_path(id, o->arg, 0, 0x12);
            break;
        case 16:
            work.ypath = SI_YPATH_FALL;
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
            if (s.boss_state) {
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

static uint16_t find_hit(uint16_t id)
{
    uint16_t other;

    for (other = sprites[s.ship].next; other; other = sprites[other].next)
        if (!is_player_side(other) && sprites_overlap(id, other))
            return other;
    return 0;
}

/* What counts as a solid pixel depends on how the level is drawn. */
static int solid_pair(unsigned a, unsigned b)
{
    return s.polarity == 2 ? !a && !b : s.polarity == 1 && a && b;
}

static int ship_pixel_collide(uint16_t other)
{
    const struct sprite *O = &sprites[other], *P = &sprites[s.ship];
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
        if (s.polarity == 2 ? !bit : s.polarity == 1 && bit)
            return dy & 0xff;
    }
    return 0;
}

static void boss_flash(uint16_t id)
{
    if (work.flashed == id) {
        work.flashed = 0;
        return;
    }
    sprite_set_mode(id, SPRITE_MODE_HIDDEN);
    work.flashed = id;
}

static void ship_destroy(struct si_context *ctx)
{
    explode(s.ship);
    sprite_set_mode(s.ship, s.polarity);
    set_image(s.ship, si_type_frames[TYPE_EXPLOSION]);
    s.ship_lost = 1;
    platform_vibrate();
    request_sound(ctx, SI_SOUND_SHIP_HIT);
}

static void enemy_destroyed(uint16_t id, struct si_context *ctx)
{
    if (s.objects[id].no_score != 1)
        award(0x31, 10, ctx);
    explode(id);
}

static void collisions(struct si_context *ctx)
{
    uint16_t id, next, hit;

    if (s.ship_lost != 1) {
        if (s.level != 1 && tilemap_collide(&s.terrain, s.ship, sprites[s.ship].y))
            ship_destroy(ctx);
        if (!s.shield && s.phase != PHASE_LEVEL_EXIT && (hit = find_hit(s.ship)) != 0) {
            uint8_t type = s.objects[hit].type;

            if (type == TYPE_ENEMY_BULLET || ship_pixel_collide(hit)) {
                if (type == TYPE_BONUS || type == TYPE_BONUS_SPECIAL) {
                    award(type, 1, ctx);
                    object_free(hit);
                } else {
                    if (--s.objects[hit].hp == 0) {
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
    for (id = sprites[s.ship - 1].next; id; id = next) {
        struct object *o = &s.objects[id];
        int boss, pixel_hit;

        next = sprites[id].next;
        if (o->type != TYPE_SHOT && o->type != TYPE_SHIELD && o->type != TYPE_WALL && o->type != TYPE_MISSILE
            && o->type != TYPE_BOSS_SHOT && o->type != TYPE_ENEMY_BULLET)
            continue;
        if (s.level != 1 && tilemap_collide(&s.terrain, id, sprites[id].y)) {
            int push;

            if (id != s.shield) {
                object_free(id);
                continue;
            }
            /* The shield rides over the terrain, taking the ship along. */
            push = s.terrain.top == 0 ? -2 : 2;
            sprite_move(s.ship, sprites[s.ship].x, (int16_t)(push + sprites[s.ship].y));
            sprite_move(s.shield, sprites[s.shield].x, (int16_t)(push + sprites[s.shield].y));
            continue;
        }
        if (o->type == TYPE_ENEMY_BULLET || o->type == TYPE_BOSS_SHOT || (hit = find_hit(id)) == 0)
            continue;

        boss = is_boss(hit);
        if (boss && (boss_pixel_hit(id, hit) == 0 || s.level == 7)
            && (s.level != 7 || boss_pixel_hit(id, hit) > 0x17 || boss_pixel_hit(id, hit) < 0x13)) {
            /* The shot is inside a boss's box but not on its picture, or
               on the last boss away from its weak rows. */
            pixel_hit = boss_pixel_hit(id, hit);
            if (!pixel_hit && s.level == 7) {
                int k;

                for (k = 0; k < 2; k++) {
                    uint16_t part = s.boss_parts[k];
                    struct object *p = &s.objects[part];

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
            if (boss_pixel_hit(id, hit) && s.level == 7)
                object_free(id);
            continue;
        }
        if (boss)
            boss_flash(hit);

        if (o->type == TYPE_SHOT || (o->type == TYPE_SHIELD && boss)) {
            s.objects[hit].hp--;
        } else if (o->type == TYPE_SHIELD) {
            s.objects[hit].hp = 0;
        } else if (o->type == TYPE_WALL || o->type == TYPE_MISSILE) {
            s.objects[hit].hp = s.objects[hit].hp < 4 ? 0 : (uint8_t)(s.objects[hit].hp - 4);
            if (hit == s.missile_target)
                o->type = TYPE_EXPLOSION;
        }
        if (s.objects[hit].hp) {
            if (s.objects[hit].no_score != 1)
                award(0x31, 5, ctx);
        } else if (boss) {
            boss_destroyed(hit, ctx);
        } else if (s.objects[hit].type == TYPE_BONUS || s.objects[hit].type == TYPE_BONUS_SPECIAL) {
            award(s.objects[hit].type, 1, ctx);
            if (next == hit)
                next = sprites[hit].next;
            object_free(hit);
        } else {
            enemy_destroyed(hit, ctx);
        }
        if (id != s.shield && o->type != TYPE_WALL && o->type != TYPE_MISSILE)
            object_free(id);
    }
}

/* After a boss: line the ship up with the top of the play area, fly it
   off to the right ever faster, then start the next level. Returns
   nonzero when that was the last level. */
static int level_exit_step(struct si_context *ctx)
{
    struct sprite *ship = &sprites[s.ship];

    if (!s.exit_step) {
        if (s.top < ship->y)
            sprite_move(s.ship, ship->x, ship->y - 1);
        else if (s.top > ship->y)
            sprite_move(s.ship, ship->x, ship->y + 1);
        else
            s.exit_step = 1;
        return 0;
    }
    sprite_move(s.ship, s.exit_step + ship->x, ship->y);
    if (ship->x % 3 == 0)
        s.exit_step++;
    if (ship->x > 0x68) {
        if (s.level > 6)
            return 1;
        s.level++;
        level_load(si_level_table[s.level]);
        ship_spawn(ctx, 10);
    }
    return 0;
}

static int tick(struct si_context *ctx)
{
    if (s.phase == PHASE_CONTINUE) {
        if (--s.countdown >= 0) {
            draw_number(s.terrain_sprite, (unsigned)s.countdown, 2);
            return SI_RESULT_REDRAW;
        }
        ctx->score = s.score;
        return SI_RESULT_GAME_OVER;
    }
    if (s.phase == PHASE_LEVEL_EXIT) {
        if (level_exit_step(ctx)) {
            ctx->score = s.score;
            return SI_RESULT_GAME_OVER;
        }
        shield_follow();
    }
    tilemap_render(&s.terrain, s.scroll);
    if (s.spawn_delay && s.spawns_left && s.level != 7) {
        s.scroll = (uint16_t)((s.scroll + 1) % (s.terrain.width << 5));
    } else if (s.level == 7) {
        /* The last level scrolls on to where its boss appears, and a little
           further while the boss comes on. */
        if (!s.boss_state) {
            if (s.scroll == 0x7a || s.scroll == 0x13a || s.scroll == 0x1ba)
                final_boss_spawn();
            else
                s.scroll++;
        }
        if (s.boss_state == 1) {
            if (!s.backdrop) {
                s.boss_state++;
            } else {
                s.scroll++;
                s.backdrop--;
            }
        }
    }
    spawn_step();
    objects_step(ctx);
    collisions(ctx);
    if (s.fire_cooldown)
        s.fire_cooldown = s.fire_cooldown == 2 ? 0 : (uint8_t)(s.fire_cooldown + 1);
    return SI_RESULT_REDRAW;
}

int si_handler(int event, struct si_context *ctx)
{
    uint8_t pending;

    if (event == SI_EVENT_START || event == 0x24) {
        ctx->period = 100;
        ctx->one_shot = 0;
        return new_game(ctx);
    }
    if (s.phase == PHASE_PLAY)
        key(event, ctx);
    else if (s.phase == PHASE_CONTINUE && continue_key(event, ctx))
        s.countdown = 5;
    /* A result left by the last event goes out now, and this event is not
       looked at further: a tick that finds one does not move the game. */
    pending = s.pending;
    if (pending) {
        s.pending = 0;
        return pending;
    }
    switch (event) {
    case SI_EVENT_TICK:
        return tick(ctx);
    case SI_EVENT_TIMER:
        if (s.shield) {
            object_free(s.shield);
            s.shield = 0;
        }
        return SI_RESULT_NONE;
    case 0x13: case 0x15: case 0x17: case 0x18: case 0x1a: case 0x1c: case 0x1d:
        return SI_RESULT_UNUSED;
    }
    return SI_RESULT_NONE;
}
