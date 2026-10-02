/* The phone's Games menus: the main menu's Games entry, the list of games
   and each game's own menu. */
#ifndef CORE_MENU_H
#define CORE_MENU_H

#include <stdint.h>

enum {
    MENU_KEY_UP,
    MENU_KEY_DOWN,
    MENU_KEY_SELECT, /* the phone's Navi key */
    MENU_KEY_BACK,   /* the phone's C key */
    MENU_KEY_LEFT,   /* only used in a game */
    MENU_KEY_RIGHT,
    MENU_KEY_START   /* on the first screen: enter the full-screen variant;
                        anywhere else the same as MENU_KEY_SELECT */
};

/* The platform calls menu_tick this many times a second. */
#define MENU_TICKS_PER_SECOND 60

void menu_init(void);
/* Returns nonzero when the screen changed and menu_draw should be called. */
uint8_t menu_key(uint8_t key);
/* Advances timed pages and the running game. Returns nonzero when the
   screen changed; call menu_draw before the next menu_tick, because a
   game move is drawn as a change to the previous picture. */
uint8_t menu_tick(void);
void menu_draw(void);

/* Whether the last menu_draw drew in the phone's LCD, which a platform may
   show magnified, and not over the whole framebuffer. */
uint8_t menu_phone_view(void);

/* Forgets what the LCD holds, so the next menu_draw draws everything. */
void menu_redraw_all(void);

/* Makes the running game's next move now instead of when its timer runs
   out; menu_tick normally does this. For scripted frames. */
void menu_game_step(void);

#endif
