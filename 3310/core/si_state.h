/* What the files of Space Impact share: its state and the helpers used
   from more than one of them. A platform short of room in one piece of ROM
   places si.c (play), si_setup.c (a game's and a level's beginning) and
   si_base.c (the helpers) apart; see platform/gb. */
#ifndef CORE_SI_STATE_H
#define CORE_SI_STATE_H

#include <stdint.h>

#include "si.h"
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
#ifdef __SDCC
    /* 16 bytes, so that finding a record by its id is a shift. */
    uint8_t unused[4];
#endif
};

struct si_state {
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
    si_ref spawn_list;
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
};
extern struct si_state si;

/* Outside the state on the phone, and never reset by a new game. */
struct si_work {
    int8_t bounce;        /* direction of the vertical bounce */
    int8_t volley;        /* ticks the last boss has been firing */
    int8_t burst_pause;
    uint16_t flashed;     /* sprite hidden for a hit flash */
    si_ref ypath;         /* the y table the path patterns read */
    int32_t boss_cooldown; /* ticks until a boss may fire again */
};
extern struct si_work si_work;

/* si_base.c */
/* Each level's tile bitmaps, for the terrain's tile numbers 1 upwards. The
   sprite layer reads the list all through the level. */
extern const si_ref *const si_level_tiles[8];
si_ref si_rom_ref(si_ref at);
void si_set_image(uint16_t id, si_ref descriptor);
uint16_t si_create(si_ref descriptor, uint8_t mode, uint8_t layer, int x, int y);
void si_digit_image(struct sprite_image *out, unsigned digit);
void si_set_template(uint16_t id, uint8_t type);
void si_objects_clear(void);
void si_draw_number(uint16_t id, uint16_t value, int digits);
si_ref si_special_icon(void);
uint16_t si_object_spawn(uint8_t type, uint8_t mode, int x, int y);
void si_hud_refresh(void);

/* si_setup.c. A platform that has these in another bank of ROM than their
   callers defines SI_SETUP_FAR and supplies the far_ functions, which
   switch banks around a call of the real ones. */
#if defined(SI_SETUP_FAR) && !defined(SI_SETUP_IMPL)
#define si_level_load far_si_level_load
#define si_ship_spawn far_si_ship_spawn
#define si_new_game far_si_new_game
#define si_continue_key far_si_continue_key
#define si_continue_enter far_si_continue_enter
#endif
void si_level_load(si_ref header);
void si_ship_spawn(struct game_context *ctx, int how);
int si_new_game(struct game_context *ctx);
int si_continue_key(int event, struct game_context *ctx);
void si_continue_enter(struct game_context *ctx);

#endif
