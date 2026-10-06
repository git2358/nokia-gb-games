/* Space Impact's insides, shared by the files the game is split into so
   that the Game Boy can keep it in banks of 16 KiB: si.c (the handler, the
   chapters, the HUD, pause and game over), si_move.c (the objects' moves),
   si_hit.c (collisions, spawning from the script, the missile and the
   dive) and si_screens.c (the keys, the title and the High scores page).
   What one calls in another is declared here, SI_FAR (si.h); the small
   helpers they all use are here, a copy in each. */
#ifndef CORE_SI_INT_H
#define CORE_SI_INT_H

#include <string.h>

#include "game_assets.h"
#include "rand.h"
#include "si.h"
#include "si_data.h"
#include "si_pic.h"

/* The helpers below, a copy in each file: plain static functions for
   SDCC, which would copy an inline one into every call; inline elsewhere,
   only to keep the compiler quiet about those a file does not use. */
#ifdef __SDCC
#define SI_HELPER static
#else
#define SI_HELPER static inline
#endif

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
   (si_box): two 6x12 ends and five 6x8 digits 8 apart between them. */
#define BOX_X 23
#define BOX_Y 26
#define BOX_W 51
#define BOX_H 12
#define BOX_END_W 6
#define BOX_DIGIT_W 6
#define BOX_DIGIT_H 8
#define BOX_DIGIT_PITCH 8
#define BOX_LEFT_END 0 /* places in si_box */
#define BOX_RIGHT_END 12
#define BOX_DIGIT_0 24

/* An object, 20 bytes on the phone. */
struct object {
    uint8_t frames, frame, type, hp, pattern, speed, fire;
    uint8_t pic;
    uint8_t saved_x, saved_y; /* where it was when the game was paused */
    uint8_t no_score, side, boss;
};

struct si_state {
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
};

/* Counts the times a record's type or side may have been set other than
   to free or an explosion: a template set, the records cleared or the
   state restored (si.c). Not the phone's: for a platform's own find_hit
   (platform/gb/si_fast.s), which keeps the records that can be hit until
   it changes. */
extern uint16_t si_records_changed;

/* The state, and the state as the game saved it when paused (0x3b2922),
   for Continue (si.c). A platform short of work RAM may keep them
   elsewhere, as snake2.c does. */
#ifdef SI_STATE_AT
extern __at(SI_STATE_AT) struct si_state si;
extern __at(SI_STATE_AT + sizeof(struct si_state)) struct si_state si_saved;
#else
extern struct si_state si;
extern struct si_state si_saved;
#endif

#define held(key) (si_keys_held >> (key) & 1)
#define IS_LAST_CHAPTER() (si.chapter == SI_CHAPTER_COUNT - 1)
#define PATH(address) (si_paths + ((address) - SI_PATHS_BASE))
#define SETTINGS(i) (si_chapter_settings[9 * si.chapter + (i)])

#define PIC(k) (si.rec[k].pic)
#define X(k) (si_pics[PIC(k)].x)
#define Y(k) (si_pics[PIC(k)].y)

void si_set_template(struct object *o, uint8_t type) SI_FAR;
uint8_t si_spawn(uint8_t type, uint8_t mode, int x, int y) SI_FAR;
void si_ship_spawn(uint8_t how) SI_FAR;
void si_high_score_save(void) SI_FAR;
void si_continue_enter(void) SI_FAR;
void si_keys_poll(void) SI_FAR;
void si_spawn_step(void) SI_FAR;
int si_terrain_collide(uint8_t k) SI_FAR;
void si_enemy_fire(uint8_t k, uint8_t type) SI_FAR;
void si_move_missile(uint8_t k) SI_FAR;
void si_move_dive(uint8_t k, int speed) SI_FAR;
void si_award(uint8_t kind, int amount) SI_FAR;
void si_boss_destroyed(uint8_t k) SI_FAR;
void si_objects_step(void) SI_FAR;
void si_ship_collisions(void) SI_FAR;
void si_shot_collisions(void) SI_FAR;
void si_title_start(void) SI_FAR;
uint8_t si_title_event(uint8_t event) SI_FAR;
/* si_screens.c's: what neither the title nor the High scores page is up
   to take. */
#define SI_NO_SCREEN 0xff
uint8_t si_screen_event(uint8_t event) SI_FAR;
void si_scores_start(void) SI_FAR;
uint8_t si_scores_event(uint8_t event) SI_FAR;

SI_HELPER const struct sprite_image *type_frames(uint8_t type)
{
    return &si_pictures[si_type_picture[type]];
}

/* Width and height of a record's current frame (0x259cc4, 0x25a598). */
SI_HELPER int width(uint8_t k)
{
    return type_frames(si.rec[k].type)[si.rec[k].frame].w;
}

SI_HELPER int height(uint8_t k)
{
    return type_frames(si.rec[k].type)[si.rec[k].frame].h;
}

/* 0x258bb4: leading zeros shown. */
SI_HELPER void draw_number(const uint8_t *pics, unsigned value, uint8_t digits)
{
    uint8_t d[5], i;

    for (i = digits; i; i--) {
        d[i - 1] = (uint8_t)(value % 10);
        value /= 10;
    }
    for (i = 0; i < digits; i++)
        si_pic_set_frames(pics[i], &si_pictures[SI_PIC_DIGIT + d[i]], 1);
}

/* 0x25975c: rows on a 96-column screen sit lower. */
SI_HELPER int row_adjust(int y)
{
    return (uint8_t)(y + (si.place == CEILING ? 10 : 6));
}

/* 0x2596b2 */
SI_HELPER uint8_t find_free(void)
{
    uint8_t k;

    for (k = 0; k < RECORDS; k++)
        if (si.rec[k].type == FREE)
            return k;
    return NO_RECORD;
}

/* 0x259598 */
SI_HELPER void object_free(uint8_t k)
{
    si_pic_free(PIC(k));
    if (si.count)
        si.count--;
    si.rec[k].type = FREE;
    if (k == si.missile_target)
        si.missile_target = 0;
}

/* 0x25a8c2 */
SI_HELPER int fire_roll(uint8_t k)
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

/* 0x25adfa */
SI_HELPER int free_at_left(uint8_t k)
{
    if (X(k) < 1 || X(k) > 250) {
        object_free(k);
        return 1;
    }
    return 0;
}

/* 0x25b0ae */
SI_HELPER int free_at_right(uint8_t k)
{
    if (width(k) + X(k) > W) {
        object_free(k);
        return 1;
    }
    return 0;
}

/* 0x25b4fa: the record keeps all but its type and picture. */
SI_HELPER void explode(uint8_t k)
{
    si.rec[k].type = TYPE_EXPLOSION;
    si_pic_set_frames(PIC(k), type_frames(TYPE_EXPLOSION), 5);
}

SI_HELPER void vibrate(void)
{
    if (!si.vibration)
        si.vibration = 3;
}

SI_HELPER int kills(void)
{
    return SI_KILLING_CHAPTERS >> si.chapter & 1;
}

#endif
