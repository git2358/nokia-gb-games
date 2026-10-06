/* Space Impact on the 3410. Each function follows one of the firmware's,
   named in its comment by address, in the same order of operations:
   objects are walked by their record's index and the generator is drawn
   from in the firmware's order, both of which a frame-exact game needs.
   This file is the handler, the chapters, the HUD, pause and game over;
   the rest is in the files si_int.h lists.

   Not done yet: the Instructions' demos, sounds and the vibrator. */
#include "si_int.h"

/* The state, and the state as the game saved it when paused (0x3b2922),
   for Continue. A platform short of work RAM may keep them elsewhere, as
   snake2.c does. */
#ifdef SI_STATE_AT
__at(SI_STATE_AT) struct si_state si;
__at(SI_STATE_AT + sizeof(struct si_state)) struct si_state si_saved;
typedef char si_state_fits[SI_STATE_AT + 2 * sizeof(struct si_state) <= SI_STATE_END ? 1 : -1];
#else
struct si_state si;
struct si_state si_saved;
#endif
static uint8_t si_have_saved;

uint16_t si_keys_held;
uint16_t si_records_changed;
uint16_t si_period;

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
    si_records_changed++;
}

/* The template a record starts from: the game's own types', or for the
   enemies the phone's table {frames, type, side, boss}. */
void si_set_template(struct object *o, uint8_t type) SI_FAR
{
    si_records_changed++;
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
uint8_t si_spawn(uint8_t type, uint8_t mode, int x, int y) SI_FAR
{
    uint8_t pic, k;

    if (si.count >= MAX_OBJECTS)
        return 0;
    pic = si_pic_create(mode, x, y, type_frames(type), si_type_frames[type]);
    k = find_free();
    if (k == NO_RECORD)
        return 0;
    si_set_template(&si.rec[k], type);
    si.rec[k].pic = pic;
    si.count++;
    return k;
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

    /* A tile's columns at a time, from where the scroll is in the first,
       the map wrapping round (the scroll is always within it, but for the
       division the Game Boy would rather not do). */
    uint16_t first = (uint16_t)(si.scroll >> 5);

    if (first >= si.map_w)
        first %= si.map_w;
    for (row = 0; row < si.map_rows; row++) {
        uint8_t at = (uint8_t)first, in = si.scroll & 0x1f, n, tile;
        const uint8_t *cells = si.map + row * si.map_w;
        uint8_t *d = &si.terrain[row * W];

        for (col = 0; col < W; col += n, in = 0) {
            n = (uint8_t)(32 - in);
            if (n > W - col)
                n = (uint8_t)(W - col);
            tile = cells[at];
            if (tile)
                memcpy(d + col, si.tiles + 32 * (tile - 1) + in, n);
            else
                memset(d + col, 0, n);
            if (++at == si.map_w)
                at = 0;
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
void si_ship_spawn(uint8_t how) SI_FAR
{
    uint8_t mode;

    si.ship_lost = 0;
    si.accel = 0;
    si.fire_count = 0;
    si.special_latch = 0;
    if (how == 10) {
        si.ship = si_spawn(TYPE_SHIP, si.mode, 5, row_adjust(0x14));
        si.ship_pic = PIC(si.ship);
        if (si.shield != NO_RECORD)
            object_free(si.shield);
        si.shield = NO_RECORD;
        si.missile_target = 0;
    } else {
        int x, y;

        object_free(si.ship);
        respawn_place(&x, &y);
        si.ship = si_spawn(TYPE_SHIP, si.mode, x, y);
        si.ship_pic = PIC(si.ship);
    }
    mode = si.mode == 0x12 ? 0x12 : SI_MODE_HIDDEN;
    if (si.shield == NO_RECORD) {
        si.shield = si_spawn(TYPE_SHIELD, mode, 3, 0x12);
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
void si_high_score_save(void) SI_FAR
{
    game_rand();
}

/* 0x2598b0 */
static void new_game(void)
{
    memset(&si, 0, sizeof si);
    si_records_changed++;
    si.countdown = 5;
    si.lives = 3;
    si.continues = 4;
    si.shot_type = TYPE_SHOT;
    si.special = TYPE_WALL;
    si.specials = 3;
    si.shield = NO_RECORD;
    si.chapter = 0;
    chapter_load();
    si_ship_spawn(10);
}

/* 0x258c42 */
void si_continue_enter(void) SI_FAR
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
    si_ship_spawn(10);
    si.entries_left = si.checkpoint ? si.checkpoint : si.entries;
    si.delay = 0x14;
    si.phase = PHASE_PLAY;
    return 1;
}

/* 0x25b3ec */
void si_award(uint8_t kind, int amount) SI_FAR
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

/* 0x25c218 */
static void next_chapter(void)
{
    if (si.fill) {
        si_pic_free(si.fill);
        si.fill = 0;
    }
    si.chapter++;
    chapter_load();
    si_ship_spawn(10);
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
            box_set(x, y, box_bit(si_box + BOX_LEFT_END, BOX_END_W, x, y));
            box_set(BOX_W - BOX_END_W + x, y, box_bit(si_box + BOX_RIGHT_END, BOX_END_W, x, y));
        }
    }
    for (i = 4; i >= 0; i--, value /= 10) {
        const uint8_t *digit = si_box + BOX_DIGIT_0 + (value % 10) * BOX_DIGIT_W;

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

/* 0x25c1a4 */
static void terrain_kills(void)
{
    uint8_t k;
    struct object *o = si.rec;

    for (k = 0; k < RECORDS; k++, o++) {
        if (o->type != FREE && o->type != TYPE_SHIP && o->type != TYPE_EXPLOSION && o->side != SIDE_PLAYER
            && !o->boss && o->pattern != 0x18 && si_terrain_collide(k))
            explode(k);
    }
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
                si_high_score_save();
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
            si_high_score_save();
            return SI_DONE_REDRAW;
        }
        draw_number(si.countdown_digits, (unsigned)si.countdown, 2);
        return SI_DONE_REDRAW;
    case PHASE_EXIT: {
        int x = si_pics[si.ship_pic].x;

        /* Off the screen judged by where it was before this step. */
        si_pic_move_by(si.ship_pic, x / 5 + 1, 0);
        if (x > W + 0x14) {
            if (si.chapter < SI_CHAPTER_COUNT - 1) {
                next_chapter();
                return SI_DONE_REDRAW;
            }
            si.phase = PHASE_OVER;
            si.countdown = 0x1e;
            si_high_score_save();
            return SI_DONE_REDRAW;
        }
        break;
    }
    case PHASE_OVER:
        game_over_step();
        return SI_DONE_REDRAW;
    }

    terrain_render();
    if (si.delay && si.entries_left) {
        int scroll = si.scroll + 1, end = si.map_w << 5;

        si.scroll = (int16_t)(scroll >= 0 && scroll < end ? scroll : scroll == end ? 0 : scroll % end);
    }
    if (si.phase != PHASE_EXIT)
        si_keys_poll();
    si_spawn_step();
    si_objects_step();
    if (si.phase == PHASE_CONTINUE || si.phase == PHASE_OVER)
        return SI_DONE_REDRAW;
    if (si.phase != PHASE_EXIT) {
        si_ship_collisions();
        si_shot_collisions();
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

/* For the replays (the host's, the Game Boy's benchmark): what the
   autopilot wrote, the lives. */
void si_debug_lives(int8_t lives) SI_FAR
{
    si.lives = lives;
}

#ifdef SI_DEBUG
#include <stdio.h>

/* For the host's replay: the live objects. */
void si_debug(void)
{
    uint8_t k;

    printf("ch %u phase %x left %u delay %u boss %x count %u cd %u fc %u lost %u:", si.chapter, si.phase, si.entries_left, si.delay, si.boss_state, si.count, si.cooldown, si.fire_count, si.ship_lost);
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
    si_records_changed++;
    si_period = 100;
    si.timer = 10;
    if (si.phase == PHASE_CONTINUE) {
        si.countdown = 5;
        si_continue_enter();
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

uint16_t si_score(void) SI_FAR
{
    return si.score;
}

uint8_t si_event(uint8_t event, uint8_t a) SI_FAR
{
    uint8_t done = si_screen_event(event);

    if (done != SI_NO_SCREEN)
        return done;
    switch (event) {
    case SI_EVENT_TITLE:
        si_title_start();
        return SI_DONE_REDRAW;
    case SI_EVENT_HIGH_SCORES:
        si_scores_start();
        return SI_DONE_REDRAW;
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
