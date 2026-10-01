/* The phone's Games menus: the main menu's Games entry, the list of games
   and each game's own menu. */
#ifndef CORE_MENU_H
#define CORE_MENU_H

#include <stdint.h>

enum {
    MENU_KEY_UP,
    MENU_KEY_DOWN,
    MENU_KEY_SELECT, /* the phone's Navi key */
    MENU_KEY_BACK    /* the phone's C key */
};

void menu_init(void);
void menu_key(uint8_t key);
void menu_draw(void);

#endif
