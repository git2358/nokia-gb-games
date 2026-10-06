/* Space Impact on the 3410. Each function follows one of the firmware's,
   named in its comment by address, in the same order of operations:
   objects are walked by their record's index and the generator is drawn
   from in the firmware's order, both of which a frame-exact game needs.

   Not done yet: the demos, the High scores page, sounds and the
   vibrator. */
#include "si.h"

#include <string.h>

#include "game_assets.h"
#include "rand.h"
#include "si_data.h"
#include "si_pic.h"

#define W 96
#define H 65
#define RECORDS 60
#define MAX_OBJECTS 40
#define FREE 0x7f
#define NO_RECORD 0xff

/* Object types: the game's own; 20 and up are the enemies. */
enum {
    TYPE_SHIP,
    TYPE_SHIELD,
    TYPE_EXPLOSION,
    TYPE_BONUS,
    TYPE_SHOT,
    TYPE_BULLET,
    TYPE_WALL,
    TYPE_MISSILE,
    TYPE_BEAM
};

#define SIDE_PLAYER 0x0a

enum {
    PHASE_PLAY = 10,
    PHASE_CONTINUE = 0x14,
    PHASE_EXIT = 0x1e,
    PHASE_CLOSE = 0x32,
    PHASE_OVER = 0x3c
};

#define FLOOR 0
#define CEILING 100

/* The game-over picture's score box, from the pieces of Snake II's
   (snake2_box): two 6x12 ends and five 6x8 digits 8 apart between them. */
#define BOX_X 23
#define BOX_Y 26
#define BOX_W 51
#define BOX_H 12
#define BOX_END_W 6
#define BOX_DIGIT_W 6
#define BOX_DIGIT_H 8
#define BOX_DIGIT_PITCH 8
#define BOX_LEFT_END 0 /* places in snake2_box */
#define BOX_RIGHT_END 12
#define BOX_DIGIT_0 24

/* An object, 20 bytes on the phone. */
struct object {
    uint8_t frames, frame, type, hp, pattern, speed, fire;
    uint8_t pic;
    uint8_t saved_x, saved_y; /* where it was when the game was paused */
    uint8_t no_score, side, boss;
};

static struct si_state {
    /* The terrain: tilemap, tiles, where it is, and its picture. */
    uint8_t map_w, map_rows, place; /* place: FLOOR or CEILING */
    const uint8_t *map, *tiles;
    uint8_t terrain[2 * W];
    struct sprite_image terrain_image;
    uint8_t terrain_pic;
    uint8_t fill, beam_line;
    uint8_t count;   /* objects alive, at most 40 */
    uint8_t chapter;
    uint8_t score_digits[5], count_digits[2], icon, hearts[5], continue_icons[4], countdown_digits[2];
    uint8_t beam_col, boss_state;
    struct object rec[RECORDS];
    uint8_t bottom, top;
    /* The chapter: its record and how far its script has got. */
    uint8_t entries, checkpoints[4], checkpoint, mode, entries_left, delay;
    const uint8_t *script;
    uint8_t phase;
    int16_t scroll;
    int8_t countdown;
    uint8_t boss;
    uint16_t score;
    uint8_t ship_lost, cooldown, fire_count, special_latch;
    int8_t lives;
    uint8_t shot_type, special;
    int8_t specials;
    uint8_t ship_pic;
    uint8_t shield, missile_target, continues, accel, ship;
    uint8_t timer, vibration; /* tick counters: the shield or the chapter's end; the vibrator */
    /* The tables' working bytes. */
    int8_t burst_pause, boss_cooldown, bounce_latch, bounce_dir;
    uint8_t bounce_age, bounce_last, flashed;
    const uint8_t *path;
    uint8_t logo_top, logo_bottom;
    uint8_t box[BOX_W * 2];
    struct sprite_image box_image;
} si;

/* The state as the game saved it when paused (0x3b2922), for Continue. */
static struct si_state si_saved;
static uint8_t si_have_saved;

uint16_t si_keys_held;
uint16_t si_period;

#define held(key) (si_keys_held >> (key) & 1)
#define IS_LAST_CHAPTER() (si.chapter == SI_CHAPTER_COUNT - 1)
#define PATH(address) (si_paths + ((address) - SI_PATHS_BASE))
#define SETTINGS(i) (si_chapter_settings[9 * si.chapter + (i)])

static const struct sprite_image *type_frames(uint8_t type)
{
    return &si_pictures[si_type_picture[type]];
}

/* Width and height of a record's current frame (0x259cc4, 0x25a598). */
static int width(uint8_t k)
{
    return type_frames(si.rec[k].type)[si.rec[k].frame].w;
}

static int height(uint8_t k)
{
    return type_frames(si.rec[k].type)[si.rec[k].frame].h;
}

#define PIC(k) (si.rec[k].pic)
#define X(k) (si_pics[PIC(k)].x)
#define Y(k) (si_pics[PIC(k)].y)

/* 0x258bb4: leading zeros shown. */
static void draw_number(const uint8_t *pics, unsigned value, uint8_t digits)
{
    uint8_t d[5], i;

    for (i = digits; i; i--) {
        d[i - 1] = (uint8_t)(value % 10);
        value /= 10;
    }
    for (i = 0; i < digits; i++)
        si_pic_set_frames(pics[i], &si_pictures[SI_PIC_DIGIT + d[i]], 1);
}

/* 0x258b94 */
static void objects_clear(void)
{
    uint8_t k;

    for (k = 0; k < RECORDS; k++) {
        si.rec[k].frames = 0;
        si.rec[k].frame = 0;
        si.rec[k].type = FREE;
        si.rec[k].no_score = 0;
        si.rec[k].side = 0;
    }
    si.count = 0;
}

/* 0x25975c: rows on a 96-column screen sit lower. */
static int row_adjust(int y)
{
    return (uint8_t)(y + (si.place == CEILING ? 10 : 6));
}

/* 0x2596b2 */
static uint8_t find_free(void)
{
    uint8_t k;

    for (k = 0; k < RECORDS; k++)
        if (si.rec[k].type == FREE)
            return k;
    return NO_RECORD;
}

/* The template a record starts from: the game's own types', or for the
   enemies the phone's table {frames, type, side, boss}. */
static void set_template(struct object *o, uint8_t type)
{
    memset(o, 0, sizeof *o);
    if (type < 20) {
        const uint8_t *t = si_templates + 20 * type;

        o->frames = t[0];
        o->frame = t[1];
        o->type = t[2];
        o->hp = t[3];
        o->pattern = t[4];
        o->speed = t[5];
        o->fire = t[6];
        o->no_score = t[0xe];
        o->side = t[0xf];
        o->boss = t[0x10];
    } else {
        const uint8_t *t = si_phone_types + 4 * (type - 20);

        o->frames = t[0];
        o->type = t[1];
        o->side = t[2];
        o->no_score = t[2] == 0x14;
        o->boss = t[3];
    }
}

/* 0x2596dc: the picture first, then the record; 0 when 40 objects are
   alive. */
static uint8_t spawn(uint8_t type, uint8_t mode, int x, int y)
{
    uint8_t pic, k;

    if (si.count >= MAX_OBJECTS)
        return 0;
    pic = si_pic_create(mode, x, y, type_frames(type), si_type_frames[type]);
    k = find_free();
    if (k == NO_RECORD)
        return 0;
    set_template(&si.rec[k], type);
    si.rec[k].pic = pic;
    si.count++;
    return k;
}

/* 0x259598 */
static void object_free(uint8_t k)
{
    si_pic_free(PIC(k));
    if (si.count)
        si.count--;
    si.rec[k].type = FREE;
    if (k == si.missile_target)
        si.missile_target = 0;
}

/* 0x258da0 */
static void terrain_create(uint8_t place)
{
    const uint8_t *size = si_map_size + 2 * si.chapter;

    si.map_w = size[0];
    si.map_rows = size[1];
    si.map = si_maps + si_map_first[si.chapter];
    si.tiles = si_tiles + 32 * si_tile_first[si.chapter];
    si.place = place;
    memset(si.terrain, 0, sizeof si.terrain);
    si.terrain_image.bitmap = si.terrain;
    si.terrain_image.w = W;
    si.terrain_image.h = (uint8_t)(si.map_rows << 3);
    si.terrain_pic = si_pic_create(si.mode, 0, place == FLOOR ? H - (si.map_rows << 3) - 11 : 10,
                                   &si.terrain_image, 1);
}

/* 0x258ec8 */
static void terrain_render(void)
{
    uint8_t row, col;

    for (row = 0; row < si.map_rows; row++) {
        uint8_t cell = (uint8_t)(row * si.map_w + (uint16_t)(si.scroll >> 5) % si.map_w);
        uint8_t in = si.scroll & 0x1f;

        for (col = 0; col < W; col++, in++) {
            uint8_t tile, b;

            if (in > 0x1f) {
                cell++;
                if (cell % si.map_w == 0)
                    cell = (uint8_t)(row * si.map_w);
                in = 0;
            }
            tile = si.map[cell];
            b = tile ? si.tiles[32 * (tile - 1) + in] : 0;
            si.terrain[row * W + col] = b;
        }
    }
}

/* 0x258fc8 */
static void set_bounds(void)
{
    if (si.place != CEILING) {
        si.top = 11;
        si.bottom = H - 27;
    } else {
        si.top = 26;
        si.bottom = 54;
    }
}

/* 0x2592aa */
static void hud_refresh(void)
{
    uint8_t i;

    draw_number(si.count_digits, (unsigned)(int)si.specials & 0xffff, 2);
    draw_number(si.score_digits, si.score, 5);
    for (i = 0; i < 5; i++)
        si_pic_set_mode(si.hearts[i], (int8_t)i < si.lives ? SI_MODE_XOR : SI_MODE_NONE);
}

static uint8_t icon_picture(void)
{
    return si.special == TYPE_MISSILE ? SI_PIC_ICON_MISSILE : si.special == TYPE_BEAM ? SI_PIC_ICON_BEAM
                                                                                        : SI_PIC_ICON_WALL;
}

/* 0x259030 */
static void hud_create(void)
{
    uint8_t i;

    si_pic_reset();
    si.fill = 0;
    if (si.mode == 0x12) {
        si.fill = si_pic_create_fill(SI_MODE_COPY, 0, 0, W, H);
        si_pic_move_after(si.fill, 0);
    }
    si_pic_create(SI_MODE_XOR, 0, 0, &si_pictures[SI_PIC_BAR_TOP], 1);
    si_pic_create(SI_MODE_XOR, 0, H - 11, &si_pictures[SI_PIC_BAR_BOTTOM], 1);
    for (i = 0; i < 5; i++) {
        si.hearts[i] = si_pic_create(SI_MODE_XOR, 16 + 6 * i, 3, &si_pictures[SI_PIC_HEART], 1);
        if (i >= 3)
            si_pic_set_mode(si.hearts[i], SI_MODE_NONE);
    }
    for (i = 0; i < 5; i++)
        si.score_digits[i] = si_pic_create(SI_MODE_XOR, 56 + 4 * i, 2, &si_pictures[SI_PIC_DIGIT], 1);
    draw_number(si.score_digits, si.score, 5);
    si.icon = si_pic_create(SI_MODE_XOR, 40, H - 7, &si_pictures[icon_picture()], 1);
    si.count_digits[0] = si_pic_create(SI_MODE_XOR, 51, H - 7, &si_pictures[SI_PIC_DIGIT], 1);
    si.count_digits[1] = si_pic_create(SI_MODE_XOR, 55, H - 7, &si_pictures[SI_PIC_DIGIT], 1);
    draw_number(si.count_digits, (unsigned)(int)si.specials & 0xffff, 2);
    terrain_create((SI_CEILING_CHAPTERS >> si.chapter & 1) ? CEILING : FLOOR);
    terrain_render();
    set_bounds();
    si_pic_move_after(si.terrain_pic, 0);
}

/* 0x259518: the chapter's record, its script from the start. */
static void chapter_load(void)
{
    const uint8_t *r = si_chapter_records + 6 * si.chapter;

    si.entries = r[0];
    memcpy(si.checkpoints, r + 1, 4);
    si.mode = r[5];
    si.script = si_scripts + 9 * si_script_first[si.chapter];
    si.entries_left = si.entries;
    si.checkpoint = 0;
    si.delay = 0x14;
    objects_clear();
    si.beam_col = 0;
    si.scroll = 0;
    si.shield = NO_RECORD;
    si.ship_pic = 0;
    si.vibration = 0;
    hud_create();
    si.boss_state = 0;
    si.phase = PHASE_PLAY;
}

/* 0x2595d8 */
static void respawn_place(int *x, int *y)
{
    *x = 2;
    *y = si.place == FLOOR ? 12 : H - 30;
}

/* 0x25978c: a new ship (10), or the next after one was lost (0x14), and
   the shield over it. */
static void ship_spawn(uint8_t how)
{
    uint8_t mode;

    si.ship_lost = 0;
    si.accel = 0;
    si.fire_count = 0;
    si.special_latch = 0;
    if (how == 10) {
        si.ship = spawn(TYPE_SHIP, si.mode, 5, row_adjust(0x14));
        si.ship_pic = PIC(si.ship);
        if (si.shield != NO_RECORD)
            object_free(si.shield);
        si.shield = NO_RECORD;
        si.missile_target = 0;
    } else {
        int x, y;

        object_free(si.ship);
        respawn_place(&x, &y);
        si.ship = spawn(TYPE_SHIP, si.mode, x, y);
        si.ship_pic = PIC(si.ship);
    }
    mode = si.mode == 0x12 ? 0x12 : SI_MODE_HIDDEN;
    if (si.shield == NO_RECORD) {
        si.shield = spawn(TYPE_SHIELD, mode, 3, 0x12);
    } else {
        si_pic_move(PIC(si.shield), si_pics[si.ship_pic].x - 2, si_pics[si.ship_pic].y - 2);
        si_pic_set_mode(PIC(si.shield), mode);
    }
    si.timer = 0x1e;
    si_period = 100;
    hud_refresh();
}

/* 0x259ff4: the high-score record is not kept here, but its generator
   draw is. */
static void high_score_save(void)
{
    game_rand();
}

/* 0x2598b0 */
static void new_game(void)
{
    memset(&si, 0, sizeof si);
    si.countdown = 5;
    si.lives = 3;
    si.continues = 4;
    si.shot_type = TYPE_SHOT;
    si.special = TYPE_WALL;
    si.specials = 3;
    si.shield = NO_RECORD;
    si.chapter = 0;
    chapter_load();
    ship_spawn(10);
}

/* 0x258c42 */
static void continue_enter(void)
{
    uint8_t i;

    si_pic_reset();
    objects_clear();
    si.fill = 0;
    if (si.mode == 0x12)
        si.fill = si_pic_create_fill(SI_MODE_COPY, 0, 0, W, H);
    for (i = 0; i < 4; i++)
        si.continue_icons[i] = si_pic_create(SI_MODE_XOR, 6 * i, 0, &si_pictures[SI_PIC_ICON_BEAM], 1);
    if (si.continues < 4)
        for (i = 3; (int)i >= si.continues; i--)
            si_pic_set_mode(si.continue_icons[i], SI_MODE_NONE);
    si.countdown_digits[0] = si_pic_create(SI_MODE_XOR, W / 2 - 6, H / 2 - 4, &si_pictures[SI_PIC_DIGIT], 1);
    si.countdown_digits[1] = si_pic_create(SI_MODE_XOR, W / 2 - 2, H / 2 - 4, &si_pictures[SI_PIC_DIGIT], 1);
    draw_number(si.countdown_digits, 5, 2);
    si.phase = PHASE_CONTINUE;
    si_period = 800;
}

/* 0x259f92 */
static int continue_key(uint8_t key)
{
    if (key != 1 && key != 3)
        return 0;
    si.cooldown = 0;
    si.fire_count = 0;
    si.lives = 3;
    si.missile_target = 0;
    si.shot_type = TYPE_SHOT;
    si.special = TYPE_WALL;
    si.specials = 3;
    chapter_load();
    ship_spawn(10);
    si.entries_left = si.checkpoint ? si.checkpoint : si.entries;
    si.delay = 0x14;
    si.phase = PHASE_PLAY;
    return 1;
}

/* 0x25a068 */
static void ship_clamp_y(void)
{
    int s = si.shield == NO_RECORD ? 0 : 2, y = si_pics[si.ship_pic].y;

    if ((H - s - 19) < y)
        y = H - s - 19;
    else if (y < s + 10)
        y = s + 10;
    si_pics[si.ship_pic].y = (int16_t)y;
}

/* 0x25a110: the keys held, every tick. */
static void keys_poll(void)
{
    int s = si.shield == NO_RECORD ? 0 : 2, x, y, step, moved = 0;

    if (si.ship_lost == 1)
        return;
    x = si_pics[si.ship_pic].x;
    y = si_pics[si.ship_pic].y;
    if (held(0)) {
        if (si.accel < 3 || (si.place == FLOOR && y == H - s - 9)
            || (si.place == CEILING && y == si.bottom - s - 9)) {
            si.accel++;
            step = 1;
        } else {
            step = 2;
        }
        si_pic_move_by(si.ship_pic, 0, step);
        moved = 1;
    }
    if (held(8)) {
        if (si.accel < 3 || (si.place == FLOOR && 5 < y && y < 8 && s == 0)) {
            si.accel++;
            step = 1;
        } else {
            step = 2;
        }
        si_pic_move_by(si.ship_pic, 0, -step);
        moved = 1;
    }
    if (held(SI_KEY_STAR)) {
        if (si.accel < 3 || (s == 0 && 0 < x && x < 3)) {
            si.accel++;
            step = 1;
        } else {
            step = 2;
        }
        if (step + s < x) {
            si_pic_move_by(si.ship_pic, -step, 0);
            moved = 1;
        }
    }
    if (held(SI_KEY_HASH)) {
        if (si.accel < 3) {
            si.accel++;
            step = 1;
        } else {
            step = 2;
        }
        if (x <= W - step - 10) {
            si_pic_move_by(si.ship_pic, step, 0);
            moved = 1;
        }
    }
    if ((held(1) || held(3)) && !si.cooldown && si.fire_count < 3) {
        spawn(si.shot_type, si.mode, x + 6, y + 3);
        si.cooldown = 1;
        si.fire_count++;
    }
    if (!held(4) && !held(6)) {
        si.special_latch = 0;
    } else if (!si.special_latch && (si.special_latch = 1, si.specials >= 1)) {
        if (si.special == TYPE_BEAM) {
            if (!si.beam_col) {
                uint8_t k;

                si.beam_col = (uint8_t)(width(si.ship) + x);
                if (si.top == 0x10)
                    si.beam_line = si_pic_create_line(si.mode, si.beam_col, 0, si.beam_col, si.bottom);
                else
                    si.beam_line = si_pic_create_line(si.mode, si.beam_col, si.top, si.beam_col, H - 1);
                k = find_free();
                set_template(&si.rec[k], TYPE_BEAM);
                si.rec[k].pic = si.beam_line;
                si.specials--;
                draw_number(si.count_digits, (unsigned)(int)si.specials & 0xffff, 2);
            }
        } else {
            spawn(si.special, si.mode, x + 6, y + 3);
            si.specials--;
            draw_number(si.count_digits, (unsigned)(int)si.specials & 0xffff, 2);
        }
    }
    if (moved)
        ship_clamp_y();
}

/* 0x25a3f4: the chapter's script. */
static void spawn_step(void)
{
    if (si.delay) {
        si.delay--;
        if (si.delay)
            return;
    }
    while (si.entries_left) {
        const uint8_t *e;
        uint8_t k, n, mode, boss = 0;

        for (k = 0; k < 4; k++)
            if (si.entries - si.entries_left == si.checkpoints[k])
                si.checkpoint = si.entries_left;
        e = si.script + 9 * (si.entries - si.entries_left);
        if (e[4] == 0x18)
            for (k = 0; k < RECORDS; k++)
                if (si.rec[k].type != FREE && si.rec[k].pattern == 0x18)
                    si.rec[k].pattern = 3;
        si.delay = e[5];
        mode = si.mode;
        if (si.entries_left == 1) {
            boss = 1;
            mode = si.mode == 0x12 ? SI_MODE_COPY : SI_MODE_HIDDEN;
        }
        for (n = 0; n < e[0]; n++) {
            uint8_t y = e[6];
            int x;

            if (y == 0x3f) {
                unsigned r = (unsigned)game_rand();
                int limit;

                y = (uint8_t)(si.top + r % si.bottom);
                if (y < si.top)
                    y = si.top;
                limit = si.bottom - type_frames(e[1])[0].h;
                if (limit < y)
                    y = (uint8_t)limit;
            }
            if (y == 0 && si.place == FLOOR)
                y = (uint8_t)(si.top + 1);
            y = (uint8_t)row_adjust(y);
            x = e[4] == 5 ? (int16_t)-(n * e[2]) : (int16_t)(n * e[2] + W);
            k = spawn(e[1], mode, x, y);
            si.rec[k].pattern = e[4];
            si.rec[k].speed = e[3];
            si.rec[k].hp = e[8];
            si.rec[k].fire = e[7];
            if (boss) {
                si.boss_state = 1;
                si.boss = k;
                si.rec[k].boss = 1;
            }
        }
        si.entries_left--;
        if (si.delay)
            return;
    }
}

/* 0x25a5d4: whether an object's picture touches the terrain's (frame 0's
   bitmap, the current frame's size). */
static int terrain_collide(uint8_t k)
{
    const uint8_t *bitmap = type_frames(si.rec[k].type)[0].bitmap;
    int w = width(k), h = height(k), x = X(k), y = Y(k);
    int ty = si_pics[si.terrain_pic].y, th = si.map_rows << 3, orow, trow, n, c;

    if (si.place == CEILING && y > ty + th)
        return 0;
    if (si.place == FLOOR && y + h < ty)
        return 0;
    if (ty < y && y + h < ty + th) {
        trow = y - ty;
        for (orow = 0, n = th - trow; n > 0; n--, orow++, trow++)
            for (c = 0; c < w; c++) {
                if (orow >= h || x + c >= W)
                    return 0;
                if (x + c >= 0 && (bitmap[c + w * (orow >> 3)] >> (orow & 7) & 1)
                    && (si.terrain[x + c + W * (trow >> 3)] >> (trow & 7) & 1))
                    return h / 2 < orow ? 2 : 1;
            }
    } else if (y < ty && ty < y + h) {
        orow = ty - y;
        for (trow = 0, n = h - orow; n > 0; n--, orow++, trow++)
            for (c = 0; c < w; c++) {
                if (orow >= h || x + c >= W)
                    return 0;
                if (x + c >= 0 && (bitmap[c + w * (orow >> 3)] >> (orow & 7) & 1)
                    && (si.terrain[x + c + W * (trow >> 3)] >> (trow & 7) & 1))
                    return 2;
            }
    } else if (ty < y && y < ty + th) {
        trow = y - ty;
        for (orow = 0; orow < h; orow++, trow++) {
            for (c = 0; c < w; c++) {
                if (x + c >= W)
                    return 0;
                if (x + c >= 0 && (bitmap[c + w * (orow >> 3)] >> (orow & 7) & 1)
                    && (si.terrain[x + c + W * (trow >> 3)] >> (trow & 7) & 1))
                    return 1;
            }
            if (trow + 1 >= th)
                return 0;
        }
    }
    return 0;
}

/* 0x25a8c2 */
static int fire_roll(uint8_t k)
{
    uint8_t chance = si.rec[k].fire;

    if (!chance || chance == 0x7f || (unsigned)game_rand() % chance)
        return 0;
    if (si.rec[k].pattern == 0x14) {
        if (si.burst_pause > 0) {
            si.burst_pause--;
            return 0;
        }
        si.burst_pause = (int8_t)((unsigned)game_rand() % 3 + 2);
    }
    return 1;
}

/* 0x25a920 */
static void enemy_fire(uint8_t k, uint8_t type)
{
    if (X(k) <= W)
        spawn(type, si.mode, X(k) - 2, (height(k) >> 1) + Y(k));
}

/* 0x25adfa */
static int free_at_left(uint8_t k)
{
    if (X(k) < 1 || X(k) > 250) {
        object_free(k);
        return 1;
    }
    return 0;
}

/* 0x25b0ae */
static int free_at_right(uint8_t k)
{
    if (width(k) + X(k) > W) {
        object_free(k);
        return 1;
    }
    return 0;
}

/* 0x25a96e: up and down, turning at the play area's limits and the
   terrain, with a direction kept per object. */
static void move_bounce(uint8_t k, uint8_t bullet)
{
    int y = Y(k), h = height(k);

    if (PIC(k) != si.bounce_last) {
        si.bounce_last = PIC(k);
        si.bounce_dir = -1;
        si.bounce_latch = 0;
        si.bounce_age = 0;
    }
    if ((si.place == FLOOR && y <= si.top) || (si.place == CEILING && si.bottom <= y + h)) {
        si.bounce_dir = si.place == FLOOR && y <= si.top ? 1 : -1;
        si.bounce_latch = 0;
    } else if (y + h < H && y >= 0) {
        if (terrain_collide(k)) {
            if (!si.bounce_latch) {
                si.bounce_dir = (int8_t)-si.bounce_dir;
                si.bounce_latch = si.bounce_dir;
                si.bounce_age = 0;
            } else if (++si.bounce_age > 10) {
                si.bounce_latch = 0;
            }
        }
    } else {
        si.bounce_dir = y < 0 ? 1 : -1;
        si.bounce_latch = si.bounce_dir;
        if (y < 0)
            y = 1;
        if (H <= y + h)
            y = H - h - 1;
    }
    if (fire_roll(k) && bullet)
        enemy_fire(k, bullet);
    if (si.rec[k].boss)
        si.boss_state = 0x1e;
    if (si.bounce_latch)
        si.bounce_dir = si.bounce_latch;
    Y(k) = (int16_t)(si.bounce_dir + y);
}

/* 0x25aaf2: a boss comes on until it is in place. */
static int boss_enter(uint8_t k)
{
    if (si.boss_state != 0x28 && si.boss_state != 0x14 && X(k) > (W / 3) * 2 - 5) {
        X(k)--;
        return 0;
    }
    return 1;
}

/* 0x25ab3e */
static void boss_charge(uint8_t k, uint8_t bullet, int reach)
{
    int x = X(k), y = Y(k);

    switch (si.boss_state) {
    case 10:
        if ((W / 3) * 2 - 5 <= x)
            si.boss_state = 0x1e;
        else
            X(k) = (int16_t)(x + 1);
        return;
    case 0x14:
        if (x <= W / 8) {
            si.boss_state = 10;
            return;
        }
        x += reach == 100 ? -2 : -4;
        if (terrain_collide(k))
            y += si.place == FLOOR ? -1 : 1;
        si_pic_move(PIC(k), x, y);
        return;
    case 0x28:
        if (x < W)
            X(k) = (int16_t)(x + 1);
        else
            si.boss_state = 0x14;
        return;
    }
    move_bounce(k, bullet);
    if ((unsigned)game_rand() % 0x32 || si.bottom <= y || y <= si.top || x > (W / 3) * 2)
        return;
    si.boss_state = reach == 100 ? 0x14 : 0x28;
}

/* 0x25ac44: the boss's own projectile, from the chapter's settings. */
static void boss_fire(uint8_t k)
{
    uint8_t type = SETTINGS(2);
    int dy = (int8_t)SETTINGS(3), dx = (int8_t)SETTINGS(4);

    if (width(k) < dx)
        dx = 0;
    if (height(k) < dy)
        dy = 0;
    if (!fire_roll(k)) {
        if (!si.boss_cooldown)
            return;
    } else if (!si.boss_cooldown) {
        uint8_t j = spawn(type, si.mode, (int8_t)(X(k) + dx - type_frames(type)[0].w), Y(k) + dy);

        si.rec[j].no_score = 1;
        si.rec[j].pattern = SETTINGS(5);
        si.rec[j].speed = SETTINGS(6);
        si.rec[j].hp = 3;
        si.boss_cooldown = 6;
        return;
    }
    si.boss_cooldown--;
}

/* 0x25ad28 */
static void move_boss(uint8_t k)
{
    uint8_t bullet = SETTINGS(1);

    if (!si.boss_state || si.boss_state == 0x7f)
        return;
    if (boss_enter(k)) {
        switch (SETTINGS(0)) {
        case 0:
            if (fire_roll(k) == 1 && bullet)
                enemy_fire(k, bullet);
            break;
        case 1:
            move_bounce(k, bullet);
            break;
        case 2:
            boss_charge(k, bullet, 100);
            break;
        case 3:
            boss_charge(k, bullet, 150);
            break;
        }
        if (SETTINGS(2))
            boss_fire(k);
    }
    si_pic_set_mode(PIC(k), SI_MODE_COPY);
}

/* 0x25ae30: along the screen, turning off the ceiling or the floor. */
static void move_slope(uint8_t k, int speed, int side)
{
    int x = X(k) - speed, y = Y(k), turned = 0;

    if (side == FLOOR) {
        if (x > W) {
            y = si.bottom;
        } else if (y > (H / 5) * 3) {
            y--;
            turned = 1;
        }
    } else {
        if (x > W) {
            y = si.top;
        } else if (y < H / 5) {
            y++;
            turned = 1;
        }
    }
    si_pic_move(PIC(k), x, y);
    if (!free_at_left(k) && turned && fire_roll(k))
        enemy_fire(k, TYPE_BULLET);
}

/* 0x25af08: leftwards with y read from a table by x. */
static void move_path(uint8_t k, int speed, int relative, int offset)
{
    int x = X(k) - speed, base, step, y;
    uint8_t pattern;

    base = relative ? offset : si.place == CEILING ? 10 : 0;
    base = row_adjust(base);
    if (x < 0)
        x = 0;
    step = (int8_t)si.path[x % W];
    y = (int16_t)(base + step);
    if (si.place == CEILING) {
        if (y + height(k) > H - 8 - 6)
            y = H - 8 - height(k) - 6;
    } else if (si.place == FLOOR && y < 10) {
        y = 10;
    }
    si_pic_move(PIC(k), x, y);
    if (free_at_left(k))
        return;
    pattern = si.rec[k].pattern;
    if (pattern == 1) {
        if (y != 0x1b && y != 9)
            return;
    } else if (pattern >= 10 && pattern <= 13) {
        if (step != 4 && step != -4)
            return;
    } else if (pattern != 15 && pattern != 16) {
        return;
    }
    if (fire_roll(k))
        enemy_fire(k, TYPE_BULLET);
}

/* 0x25b06c: the enemy with the most hit points, by record. */
static uint8_t missile_pick_target(void)
{
    uint8_t k, best = 0;
    int most = -99;

    for (k = 0; k < RECORDS; k++) {
        uint8_t t = si.rec[k].type;

        if (t != FREE && t != TYPE_SHIP && t != TYPE_EXPLOSION && si.rec[k].side != SIDE_PLAYER
            && most < (int8_t)si.rec[k].hp) {
            best = k;
            most = (int8_t)si.rec[k].hp;
        }
    }
    return best;
}

/* 0x25b0f0 */
static void move_missile(uint8_t k)
{
    int x = X(k), y = Y(k), aim;
    uint8_t t = si.missile_target;

    if (!t || X(t) < x)
        si.missile_target = t = missile_pick_target();
    if (!t) {
        si_pic_move_by(PIC(k), 1, 0);
        return;
    }
    aim = Y(t) + (height(t) >> 1);
    if (y < aim && y < si.bottom)
        y++;
    else if (y > aim && y > si.top)
        y--;
    si_pic_move(PIC(k), x + 2, y);
    free_at_right(k);
}

/* 0x25b1c4: leftwards, then down to the ship's row and a shot when level
   with it. */
static void move_dive(uint8_t k, int speed)
{
    uint8_t top = 5, bottom = 0x21, x, y;
    int h;

    if (si.place == CEILING && si.map_rows < 3) {
        top = 0xf;
        bottom = 0x2b;
    }
    top = (uint8_t)row_adjust(top);
    bottom = (uint8_t)row_adjust(bottom);
    x = (uint8_t)X(k);
    y = (uint8_t)Y(k);
    if (free_at_left(k))
        return;
    h = height(k);
    if (x > W / 2) {
        if (W - width(k) < x) {
            x = (uint8_t)(x - speed);
            y = top;
        } else {
            x = (uint8_t)(x - speed);
            if (y < bottom - h)
                y = (uint8_t)(y + speed);
        }
    } else if (x == W / 2 || x == W / 2 - 1 || x == W / 2 - 2) {
        if (top < y) {
            y = (uint8_t)(y - speed);
            if (y == si_pics[si.ship_pic].y)
                enemy_fire(k, TYPE_BULLET);
        } else {
            x = (uint8_t)(x - speed);
        }
    } else {
        x = (uint8_t)(x - speed);
        if (y < bottom)
            y = (uint8_t)(y + speed);
    }
    if (bottom - h < y)
        y = (uint8_t)(bottom - h);
    si_pic_move(PIC(k), x, y);
}

/* 0x25b37c */
static void move_track_ship(uint8_t k, int speed)
{
    int y = Y(k), ship_y = si_pics[si.ship_pic].y;

    if (y < ship_y)
        y++;
    else if (y > ship_y)
        y--;
    si_pic_move(PIC(k), X(k) - speed, y);
    free_at_left(k);
}

/* 0x25b3ec */
static void award(uint8_t kind, int amount)
{
    if (kind == TYPE_BONUS) {
        for (;;) {
            unsigned pick = (unsigned)game_rand() & 3;
            int x, y;

            if (pick == 0) {
                if (si.lives > 4)
                    continue;
                si.lives++;
                break;
            }
            if (pick == 1) {
                if (si.special == TYPE_MISSILE) {
                    si.specials += 3;
                    si_pic_set_mode(si.icon, SI_MODE_XOR);
                    break;
                }
                si.special = TYPE_MISSILE;
                si.specials = 3;
            } else if (pick == 2) {
                if (si.special == TYPE_WALL) {
                    si.specials += 3;
                    break;
                }
                si.special = TYPE_WALL;
                si.specials = 3;
            } else {
                if (si.special == TYPE_BEAM) {
                    si.specials++;
                    break;
                }
                si.special = TYPE_BEAM;
                si.specials = 1;
            }
            x = si_pics[si.icon].x;
            y = si_pics[si.icon].y;
            si_pic_free(si.icon);
            si.icon = si_pic_create(SI_MODE_XOR, x, y, &si_pictures[icon_picture()], 1);
            break;
        }
    } else if (kind == 0x7e) {
        si.score = (uint16_t)(si.score + amount);
        draw_number(si.score_digits, si.score, 5);
    }
    hud_refresh();
}

/* 0x25b4fa: the record keeps all but its type and picture. */
static void explode(uint8_t k)
{
    si.rec[k].type = TYPE_EXPLOSION;
    si_pic_set_frames(PIC(k), type_frames(TYPE_EXPLOSION), 5);
}

static void vibrate(void)
{
    if (!si.vibration)
        si.vibration = 3;
}

/* 0x25b520 */
static void boss_destroyed(uint8_t k)
{
    int w = width(k), h = height(k), cx = X(k) + (w >> 1), cy = Y(k) + (h >> 1);
    uint8_t frames = si.rec[k].frames, i, pairs = (uint8_t)(IS_LAST_CHAPTER() + 5);

    object_free(k);
    si.boss_state = 0x7f;
    si.timer = 0x1e;
    spawn(TYPE_EXPLOSION, si.mode, cx, cy);
    if (frames)
        si.rec[k].frame = (uint8_t)((unsigned)game_rand() % frames);
    else
        game_rand();
    for (i = 0; i < pairs; i++) {
        int dx, dy;

        if (IS_LAST_CHAPTER()) {
            dx = (int8_t)((unsigned)game_rand() % 0x14 + 1);
            dy = (int8_t)((unsigned)game_rand() % 0xd + 1);
        } else {
            dx = (int8_t)(((unsigned)game_rand() & 3) + 1);
            dy = 6;
        }
        spawn(TYPE_EXPLOSION, si.mode, dx + cx, dy + cy);
        if (frames)
            si.rec[k].frame = (uint8_t)((unsigned)game_rand() % frames);
        spawn(TYPE_EXPLOSION, si.mode, cx - dx, cy - dy);
        if (frames)
            si.rec[k].frame = (uint8_t)((unsigned)game_rand() % frames);
    }
    award(0x7e, 100);
    vibrate();
}

/* 0x25b680: everything within three columns of the beam. */
static void beam_scan(void)
{
    uint8_t k;

    for (k = 0; k < RECORDS; k++) {
        struct object *o = &si.rec[k];
        int d;

        if (o->type == FREE || o->type == TYPE_SHIP || o->type == TYPE_EXPLOSION || o->side == SIDE_PLAYER)
            continue;
        d = X(k) - si.beam_col;
        if (d < -3 || d > 3)
            continue;
        if (o->type == TYPE_BONUS)
            award(TYPE_BONUS, 1);
        if (!o->boss) {
            explode(k);
        } else if (o->hp < 3) {
            boss_destroyed(k);
            continue;
        } else {
            o->hp -= 2;
        }
        award(0x7e, 10);
    }
}

/* 0x25b750: records 0 to 58, in order. */
static void objects_step(void)
{
    uint8_t k;

    for (k = 0; k < RECORDS - 1; k++) {
        struct object *o = &si.rec[k];
        uint8_t pic = o->pic;

        if (o->type == FREE)
            continue;
        si_pic_next_frame(pic);
        o->frame = si_pics[pic].frame;
        switch (o->type) {
        case TYPE_SHIP:
            continue;
        case TYPE_EXPLOSION:
            if (o->frame != 4)
                continue;
            if (pic != si.ship_pic) {
                object_free(k);
                continue;
            }
            if (--si.lives < 1) {
                if (si.continues < 2) {
                    si.phase = PHASE_OVER;
                    si.countdown = 0x1e;
                    high_score_save();
                    return;
                }
                si.continues--;
                continue_enter();
                return;
            }
            ship_spawn(0x14);
            continue;
        case TYPE_WALL:
            si_pic_move(pic, width(si.ship) + si_pics[si.ship_pic].x + 2,
                        si_pics[si.ship_pic].y - (height(k) >> 1) + 3);
            if (o->frame == 6)
                object_free(k);
            continue;
        case TYPE_BEAM:
            if (si.beam_col < W) {
                si.beam_col += 2;
                si_pics[pic].x = si_pics[pic].x2 = si.beam_col;
                beam_scan();
            } else {
                si_pic_free(pic);
                o->type = FREE;
                si.beam_col = 0;
            }
            continue;
        }
        switch (o->pattern) {
        case 1:
            si.path = PATH(0x4ad094);
            move_path(k, o->speed, 0, 0);
            break;
        case 2:
            si.path = PATH(0x4ad0f4);
            move_path(k, o->speed, 0, 0);
            break;
        case 3:
            move_track_ship(k, o->speed);
            break;
        case 4:
            si.path = PATH(0x4ad154);
            move_path(k, o->speed, 0, 0);
            break;
        case 5:
            if (!free_at_right(k))
                si_pic_move_by(pic, o->speed, 0);
            break;
        case 6:
            if (!free_at_left(k))
                si_pic_move_by(pic, -o->speed, 0);
            break;
        case 7:
            move_dive(k, o->speed);
            break;
        case 9:
            move_missile(k);
            break;
        case 10:
        case 11:
        case 12:
        case 13: {
            static const uint8_t offset[4] = { 0x23, 0x0b, 0x0e, 0x19 };

            si.path = PATH(0x4ad1b4);
            move_path(k, o->speed, 0x80, offset[o->pattern - 10]);
            break;
        }
        case 14:
            si.path = PATH(0x4ad1b4);
            move_path(k, o->speed, 0x80, 0x12);
            break;
        case 15:
            si.path = PATH(0x4ad31c);
            move_path(k, o->speed, 0, 0);
            break;
        case 16:
            si.path = PATH(0x4ad2bc);
            move_path(k, o->speed, 0, 0);
            break;
        case 17:
            move_slope(k, o->speed, CEILING);
            break;
        case 18:
            move_slope(k, o->speed, FLOOR);
            break;
        case 23:
            move_boss(k);
            break;
        case 24:
            if (X(k) <= (W / 3) * 2)
                move_bounce(k, TYPE_BULLET);
            else
                X(k)--;
            if (si.boss_state) {
                o->pattern = 3;
                o->speed = 1;
            }
            break;
        }
    }
}

/* 0x25ba34: boxes, on the low bytes of the places. */
static int overlap(uint8_t a, uint8_t b)
{
    uint8_t ax = (uint8_t)X(a), ay = (uint8_t)Y(a), bx = (uint8_t)X(b), by = (uint8_t)Y(b);

    return by <= (uint8_t)(ay + height(a)) && ay <= (uint8_t)(by + height(b)) && bx <= (uint8_t)(ax + width(a))
           && ax <= (uint8_t)(bx + width(b));
}

/* 0x25bae6 */
static uint8_t find_hit(uint8_t a)
{
    uint8_t k;

    for (k = 0; k < RECORDS; k++) {
        uint8_t t = si.rec[k].type;

        if (t != FREE && t != TYPE_SHIP && t != TYPE_EXPLOSION && si.rec[k].side != SIDE_PLAYER && overlap(a, k))
            return k;
    }
    return 0;
}

static int solid(unsigned a, unsigned b)
{
    return si.mode == 0x12 ? !a && !b : si.mode == 0x20 && a && b;
}

/* 0x25bb38 */
static int ship_pixel_collide(uint8_t other)
{
    uint8_t P = si.ship;
    const uint8_t *pb = type_frames(si.rec[P].type)[0].bitmap, *ob = type_frames(si.rec[other].type)[0].bitmap;
    int pw = width(P), ow = width(other), px = X(P), py = Y(P), ox = X(other), oy = Y(other);
    unsigned ship_row, other_row, ship_col, other_col, start_ship, start_other, cols, rows;

    if (py < oy) {
        ship_row = (uint8_t)(oy - py);
        other_row = 0;
    } else {
        other_row = (uint8_t)(py - oy);
        ship_row = 0;
    }
    if (px < ox) {
        ship_col = (uint8_t)(ox - px);
        other_col = 0;
    } else {
        other_col = (uint8_t)(px - ox);
        ship_col = 0;
    }
    start_ship = ship_col;
    start_other = other_col;
    if (px + pw < ox + ow)
        cols = (uint8_t)(pw - ship_col);
    else
        cols = (uint8_t)(ow - other_col);
    if (height(P) + py < oy + height(other))
        rows = (uint8_t)(height(P) - ship_row);
    else
        rows = (uint8_t)(height(other) - other_row);
    for (; rows; rows--) {
        unsigned a = start_ship, b = start_other, n;

        for (n = cols; n; n--) {
            unsigned ship_bit = pb[a + pw * (ship_row >> 3)] & 1u << (ship_row & 7);
            unsigned other_bit = ob[b + ow * (other_row >> 3)] & 1u << (other_row & 7);

            if (solid(ship_bit, other_bit))
                return 1;
            a = (a + 1) & 0xff;
            b = (b + 1) & 0xff;
        }
        ship_row = (ship_row + 1) & 0xff;
        other_row = (other_row + 1) & 0xff;
    }
    return 0;
}

/* 0x25be40: the row distance when the shot meets the boss's picture along
   its row, else 0. */
static int boss_pixel_hit(uint8_t shot, uint8_t boss)
{
    const uint8_t *bitmap = type_frames(si.rec[boss].type)[0].bitmap;
    int dy = Y(shot) - Y(boss), bw = width(boss), i;

    if (dy < 0)
        dy = -dy;
    for (i = 0; i <= width(shot); i = (int8_t)(i + 1)) {
        int dx = (int8_t)(X(shot) - X(boss));
        unsigned bit;

        if (dx < 0)
            dx = -dx;
        bit = bitmap[(unsigned)(dy >> 3) * bw + ((i + (dx & 0xff)) & 0xff)] & 1u << (dy & 7);
        if (si.mode == 0x12 ? !bit : si.mode == 0x20 && bit)
            return dy;
    }
    return 0;
}

static void ship_destroyed(void)
{
    explode(si.ship);
    si.ship_lost = 1;
    vibrate();
}

static int kills(void)
{
    return SI_KILLING_CHAPTERS >> si.chapter & 1;
}

/* 0x25bd24 */
static void ship_collisions(void)
{
    uint8_t hit;

    if (si.ship_lost == 1)
        return;
    if (kills() && terrain_collide(si.ship)) {
        si.ship_lost = 1;
        explode(si.ship);
        vibrate();
    }
    if (si.shield == NO_RECORD && (hit = find_hit(si.ship)) != 0
        && (si.rec[hit].type == TYPE_BULLET || ship_pixel_collide(hit) == 1)) {
        struct object *o = &si.rec[hit];

        if (o->type == TYPE_BONUS) {
            award(TYPE_BONUS, 1);
            object_free(hit);
            return;
        }
        if (--o->hp == 0) {
            if (!o->boss) {
                explode(hit);
                if (!o->no_score)
                    award(0x7e, 10);
            } else {
                boss_destroyed(hit);
            }
        }
        ship_destroyed();
    }
}

static void boss_flash(uint8_t k)
{
    if (si.flashed == PIC(k)) {
        si.flashed = 0;
    } else {
        si_pic_set_mode(PIC(k), SI_MODE_NONE);
        si.flashed = PIC(k);
    }
}

/* 0x25bf64: the player's projectiles, and every projectile against the
   terrain. */
static void shot_collisions(void)
{
    uint8_t k, on_terrain = 0;

    for (k = 0; k < RECORDS; k++) {
        struct object *o = &si.rec[k];
        uint8_t hit, lo, hi;
        int pixel;

        if (o->type == FREE || !o->side || o->type == TYPE_BEAM)
            continue;
        if (kills() && terrain_collide(k) && k != si.shield) {
            object_free(k);
            on_terrain = 1;
            continue;
        }
        if (on_terrain || o->side != SIDE_PLAYER || (hit = find_hit(k)) == 0)
            continue;
        if (!si.rec[hit].boss) {
            pixel = 1;
        } else {
            int weak = SETTINGS(7) && SETTINGS(8);

            lo = SETTINGS(7);
            hi = SETTINGS(8);
            pixel = boss_pixel_hit(k, hit);
            if (!((!weak && pixel) || (pixel < hi && lo < pixel)))
                goto spent;
        }
        {
            struct object *t = &si.rec[hit];

            if (t->boss)
                boss_flash(hit);
            if (o->type == TYPE_SHIELD) {
                t->hp = t->boss ? (uint8_t)(t->hp - 1) : 0;
                if (t->type == TYPE_BONUS) {
                    award(TYPE_BONUS, 1);
                    object_free(hit);
                    t->hp = 1;
                }
                vibrate();
            } else if (o->type == TYPE_SHOT) {
                t->hp--;
            } else if (o->type == TYPE_WALL || o->type == TYPE_MISSILE) {
                t->hp = t->hp < 4 ? 0 : (uint8_t)(t->hp - 4);
                if (hit == si.missile_target)
                    explode(k);
            }
            if (!t->hp) {
                if (!t->boss) {
                    if (t->type != TYPE_BONUS) {
                        explode(hit);
                        if (!t->no_score)
                            award(0x7e, 10);
                    }
                } else {
                    boss_destroyed(hit);
                }
            } else if (!t->no_score) {
                award(0x7e, 5);
            }
        }
    spent:
        if (pixel && o->type != TYPE_SHIELD && o->type != TYPE_WALL)
            object_free(k);
    }
}

/* 0x25c1a4 */
static void terrain_kills(void)
{
    uint8_t k;

    for (k = 0; k < RECORDS; k++) {
        struct object *o = &si.rec[k];

        if (o->type != FREE && o->type != TYPE_SHIP && o->type != TYPE_EXPLOSION && o->side != SIDE_PLAYER
            && !o->boss && o->pattern != 0x18 && terrain_collide(k))
            explode(k);
    }
}

/* 0x25c218 */
static void next_chapter(void)
{
    if (si.fill) {
        si_pic_free(si.fill);
        si.fill = 0;
    }
    si.chapter++;
    chapter_load();
    ship_spawn(10);
}

static int box_bit(const uint8_t *bitmap, int w, int x, int y)
{
    return bitmap[(y >> 3) * w + x] >> (y & 7) & 1;
}

static void box_set(int x, int y, int on)
{
    uint8_t *b = &si.box[(y >> 3) * BOX_W + x];

    if (on)
        *b |= (uint8_t)(1u << (y & 7));
    else
        *b &= (uint8_t)~(1u << (y & 7));
}

/* The score in its box (0x25cb9c), set bits light on the black screen. */
static void box_make(void)
{
    unsigned value = si.score;
    int x, y, i;

    memset(si.box, 0, sizeof si.box);
    for (y = 0; y < BOX_H; y++) {
        for (x = BOX_END_W; x < BOX_W - BOX_END_W; x++)
            box_set(x, y, y == 0 || y >= BOX_H - 2);
        for (x = 0; x < BOX_END_W; x++) {
            box_set(x, y, box_bit(snake2_box + BOX_LEFT_END, BOX_END_W, x, y));
            box_set(BOX_W - BOX_END_W + x, y, box_bit(snake2_box + BOX_RIGHT_END, BOX_END_W, x, y));
        }
    }
    for (i = 4; i >= 0; i--, value /= 10) {
        const uint8_t *digit = snake2_box + BOX_DIGIT_0 + (value % 10) * BOX_DIGIT_W;

        for (y = 0; y < BOX_DIGIT_H; y++)
            for (x = 0; x < BOX_DIGIT_W; x++)
                box_set(BOX_END_W + BOX_DIGIT_PITCH * i + x, 1 + y, !box_bit(digit, BOX_DIGIT_W, x, y));
    }
    si.box_image.bitmap = si.box;
    si.box_image.w = BOX_W;
    si.box_image.h = BOX_H;
}

/* The game-over picture: stars, a box with the score, the title's halves
   parting. */
static void game_over_step(void)
{
    if (si.countdown == 0x1e) {
        uint8_t i;

        si_pic_reset();
        si.fill = si_pic_create_fill(SI_MODE_COPY, 0, 0, W, H);
        for (i = 0; i < 30; i++) {
            int x = (int)((unsigned)game_rand() % W);
            int y = (int)((unsigned)game_rand() % H);

            si_pic_create(SI_MODE_XOR, x, y, &si_pictures[SI_PIC_STAR], 1);
        }
        si_pic_create_fill(SI_MODE_COPY, W / 2 - 0x19, H / 2 - 6, 0x32, 0x14);
        box_make();
        si_pic_create(SI_MODE_XOR, BOX_X, BOX_Y, &si.box_image, 1);
        si.logo_top = si_pic_create(SI_MODE_COPY, (W - 0x50) / 2, 11, &si_pictures[SI_PIC_LOGO_TOP], 1);
        si.logo_bottom = si_pic_create(SI_MODE_COPY, (W - 0x59) / 2, H - 0x19, &si_pictures[SI_PIC_LOGO_BOTTOM], 1);
    }
    if (si.countdown > 0x16) {
        si_pic_move_by(si.logo_top, 0, -1);
        si_pic_move_by(si.logo_bottom, 0, 1);
    }
    if (!si.countdown) {
        si.phase = PHASE_CLOSE;
        return;
    }
    si.countdown--;
}

/* 0x25c274: one event of play. */
static uint8_t tick(uint8_t event, uint8_t a)
{
    uint8_t c;

    if (event == SI_EVENT_KEY_UP) {
        if (a == 1 || a == 3)
            si.fire_count = 0;
        if ((a == 4 || a == 6) && !held(4) && !held(6))
            si.special_latch = 0;
        if (a == 0 || a == 8 || a == SI_KEY_STAR || a == SI_KEY_HASH)
            si.accel = 0;
    }
    if (si.phase == PHASE_CONTINUE) {
        if (continue_key(a))
            si.countdown = 5;
    } else if (si.phase == PHASE_CLOSE) {
        if (event != SI_EVENT_TICK)
            return 0;
        return SI_DONE_CLOSE;
    }
    if (event != SI_EVENT_TICK)
        return 0;

    c = si.timer;
    if (c && (si.timer = (uint8_t)(c - 1), c == 1)) {
        if (si.shield != NO_RECORD) {
            object_free(si.shield);
            si.shield = NO_RECORD;
        }
        if (si.boss_state == 0x7f) {
            si.phase = PHASE_EXIT;
            if (IS_LAST_CHAPTER())
                high_score_save();
            si_pic_set_mode(si.terrain_pic, SI_MODE_NONE);
        }
    }
    if (si.vibration)
        si.vibration--;

    switch (si.phase) {
    case PHASE_CONTINUE:
        if (--si.countdown < 0) {
            si_period = 100;
            si.phase = PHASE_OVER;
            si.countdown = 0x1e;
            high_score_save();
            return SI_DONE_REDRAW;
        }
        draw_number(si.countdown_digits, (unsigned)si.countdown, 2);
        return SI_DONE_REDRAW;
    case PHASE_EXIT: {
        int x = si_pics[si.ship_pic].x;

        si_pic_move_by(si.ship_pic, x / 5 + 1, 0);
        if (si_pics[si.ship_pic].x > W + 0x14) {
            if (si.chapter < SI_CHAPTER_COUNT - 1) {
                next_chapter();
                return SI_DONE_REDRAW;
            }
            si.phase = PHASE_OVER;
            si.countdown = 0x1e;
            high_score_save();
            return SI_DONE_REDRAW;
        }
        break;
    }
    case PHASE_OVER:
        game_over_step();
        return SI_DONE_REDRAW;
    }

    terrain_render();
    if (si.delay && si.entries_left)
        si.scroll = (int16_t)((si.scroll + 1) % (si.map_w << 5));
    if (si.phase != PHASE_EXIT)
        keys_poll();
    spawn_step();
    objects_step();
    if (si.phase == PHASE_CONTINUE || si.phase == PHASE_OVER)
        return SI_DONE_REDRAW;
    if (si.phase != PHASE_EXIT) {
        ship_collisions();
        shot_collisions();
        if (kills())
            terrain_kills();
    }
    if (si.cooldown)
        si.cooldown = si.cooldown == 2 ? 0 : (uint8_t)(si.cooldown + 1);
    if (si.fill)
        si_pic_move_after(si.fill, 0);
    if (si.boss_state && si.boss_state != 0x7f)
        si_pic_move_after(PIC(si.boss), si.fill);
    if (si.shield != NO_RECORD)
        si_pic_move(PIC(si.shield), si_pics[si.ship_pic].x - 2, si_pics[si.ship_pic].y - 2);
    return SI_DONE_REDRAW;
}

#ifdef SI_DEBUG
#include <stdio.h>

/* For the host's replay: what the autopilot wrote (the lives), and the
   live objects. */
void si_debug_lives(int8_t lives)
{
    si.lives = lives;
}

void si_debug(void)
{
    uint8_t k;

    printf("ch %u phase %x left %u delay %u boss %x count %u:", si.chapter, si.phase, si.entries_left, si.delay, si.boss_state, si.count);
    for (k = 0; k < RECORDS; k++)
        if (si.rec[k].type != FREE)
            printf(" [%u t%u %d,%d hp%u p%u]", k, si.rec[k].type, X(k), Y(k), si.rec[k].hp, si.rec[k].pattern);
    printf("\n");
}
#endif

/* The handler's pause (event 3): where everything is, saved with the
   rest of the state unless the game is over. */
static uint8_t pause(void)
{
    uint8_t k;

    if (si.phase == PHASE_CLOSE || si.phase == PHASE_OVER)
        return SI_DONE_CLOSE;
    for (k = 0; k < RECORDS; k++)
        if (si.rec[k].type != FREE) {
            si.rec[k].saved_x = (uint8_t)X(k);
            si.rec[k].saved_y = (uint8_t)Y(k);
        }
    si_pic_reset();
    if (!(si.phase == PHASE_EXIT && IS_LAST_CHAPTER())) {
        si_saved = si;
        si_have_saved = 1;
    }
    return SI_DONE_CLOSE;
}

/* 0x259334: Continue, from the saved state. */
static uint8_t resume(void)
{
    uint8_t k;

    if (!si_have_saved)
        return 0;
    si = si_saved;
    si_period = 100;
    si.timer = 10;
    if (si.phase == PHASE_CONTINUE) {
        si.countdown = 5;
        continue_enter();
        return SI_DONE_REDRAW;
    }
    hud_create();
    if (si.phase == PHASE_EXIT)
        si_pic_set_mode(si.terrain_pic, SI_MODE_NONE);
    for (k = 0; k < RECORDS; k++) {
        struct object *o = &si.rec[k];
        int x = o->saved_x, y = o->saved_y;

        switch (o->type) {
        case FREE:
            continue;
        case TYPE_SHIP:
            o->pic = si_pic_create(si.mode, x, y, type_frames(o->type), si_type_frames[o->type]);
            si.ship_pic = o->pic;
            continue;
        case TYPE_SHIELD:
            o->pic = si_pic_create(si.mode, x, y, type_frames(o->type), si_type_frames[o->type]);
            si.shield = k;
            si.timer = 0x5c;
            continue;
        case TYPE_EXPLOSION:
            o->pic = si_pic_create(si.mode, x, y, type_frames(o->type), si_type_frames[o->type]);
            if (si.ship_lost == 1)
                si.ship_pic = o->pic;
            continue;
        case TYPE_BEAM:
            if (si.top == 0x10)
                si.beam_line = si_pic_create_line(si.mode, si.beam_col, 0, si.beam_col, si.bottom);
            else
                si.beam_line = si_pic_create_line(si.mode, si.beam_col, si.top, si.beam_col, H - 1);
            o->pic = si.beam_line;
            continue;
        }
        if (!o->boss) {
            o->pic = si_pic_create(si.mode, x, y, type_frames(o->type), si_type_frames[o->type]);
        } else {
            o->pic = si_pic_create(si.mode == 0x12 ? SI_MODE_COPY : si.mode, x, y, type_frames(o->type),
                                   si_type_frames[o->type]);
            si.boss = k;
        }
    }
    hud_refresh();
    return SI_DONE_REDRAW;
}

uint8_t si_event(uint8_t event, uint8_t a)
{
    switch (event) {
    case SI_EVENT_NEW_GAME:
        new_game();
        return SI_DONE_REDRAW;
    case SI_EVENT_PAUSE:
        return pause();
    case SI_EVENT_CONTINUE:
        return resume();
    case SI_EVENT_TICK:
    case SI_EVENT_KEY_DOWN:
    case SI_EVENT_KEY_UP:
        return tick(event, a);
    }
    return 0;
}
