/* The phone's Games menus: the main menu's Games entry, the list of games
   and Space Impact's own menu, and the game played from it. */
#ifndef CORE_MENU_H
#define CORE_MENU_H

#include <stdint.h>

enum {
    MENU_KEY_UP,
    MENU_KEY_DOWN,
    MENU_KEY_SELECT, /* the console's A: the phone's Navi key; fire in the game */
    MENU_KEY_BACK,   /* the console's B: the phone's C key; the special weapon in the game */
    MENU_KEY_LEFT,   /* only used in the game */
    MENU_KEY_RIGHT,
    MENU_KEY_START,  /* on the first screen: enter the full-screen variant;
                        in the game: pause; anywhere else the same as
                        MENU_KEY_SELECT */
    MENU_KEY_ALT     /* the console's Select button: as MENU_KEY_START, but
                        on the first screen the same as MENU_KEY_SELECT */
};

/* The platform calls menu_tick once per screen frame of this many
   microseconds (1/59.73 s on both consoles). */
#define MENU_FRAME_US 16743
#define MENU_TICKS_PER_SECOND 60

void menu_init(void);
/* A button went down. Returns nonzero when the screen changed and
   menu_draw should be called. */
uint8_t menu_key(uint8_t key);
/* The buttons down now, as bits 1 << MENU_KEY_; the platform calls it
   every frame after menu_key. The game repeats a held button as the phone
   repeats a held key, one at a time. Returns nonzero when the screen
   changed. */
uint8_t menu_held(uint8_t keys);
/* Advances timed pages and the running game by one frame. Returns nonzero
   when the screen changed; call menu_draw before the next menu_tick. */
uint8_t menu_tick(void);
void menu_draw(void);

/* Forgets what the LCD holds, so the next menu_draw draws everything. */
void menu_redraw_all(void);

/* Fixes the seed new games start from, which is otherwise the time so far.
   For scripted frames. */
void menu_seed(uint16_t seed);

/* Plays scripted keys, drawing after each as a platform does: u, d, l, r,
   s the Navi key (A), b back (B), a Start, e Select, w a second of time,
   t a tenth of one, p start over as after a power cycle, 1 to 7 to have
   new games of Space Impact start at that level (the phone's second to
   eighth), and z to start games from the seed the phone has after
   power-on. */
void menu_script(const char *keys);

#endif
