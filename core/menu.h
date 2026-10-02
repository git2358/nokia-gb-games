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
    MENU_KEY_ALT     /* on the first screen: enter the full-screen variant */
};

/* The platform calls menu_tick this many times a second. */
#define MENU_TICKS_PER_SECOND 60

void menu_init(void);
void menu_key(uint8_t key);
/* Advances timed pages and the running game. Returns nonzero when the
   screen changed; call menu_draw before the next menu_tick, because a
   game move is drawn as a change to the previous picture. */
uint8_t menu_tick(void);
void menu_draw(void);

/* Makes the running game's next move now instead of when its timer runs
   out; menu_tick normally does this. For scripted frames. */
void menu_game_step(void);

#endif
