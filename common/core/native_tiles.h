/* The full-screen variant's menus as pictures made at build time.

   Drawing a full-screen menu into the framebuffer, a letter at a time, and
   then making the screen's tiles from it takes the Game Boy most of a
   second. But most of those menus are always the same picture, apart from
   the cursor beside the selection: the list of games, each game's menu, the
   settings, the instructions page by page. Those are drawn once, at build
   time, by the host build of the same core (native_gen.c), and cut into the
   Game Boy's 8x8 tiles; showing one is then copying its tiles.

   menu.c numbers those screens and draws any of them on request; the
   platform shows them by number and draws the cursor over them. A screen
   without a number (one showing a score, say) is drawn as before. */
#ifndef NATIVE_TILES_H
#define NATIVE_TILES_H

#include <stdint.h>

#define NATIVE_NONE 0xffff

/* The rows of a list that can hold the cursor. */
#define NATIVE_CURSOR_ROWS 8

/* menu.c */

/* The number of the full-screen menu the menus are on, or NATIVE_NONE if it
   is not one made at build time. */
uint16_t menu_native_id(void);

/* For native_gen.c: draws screen `id` into the cleared framebuffer, without
   the cursor. Returns NATIVE_DRAWN, NATIVE_SKIP when there is no screen of
   that number, or NATIVE_END past the last. */
enum {
    NATIVE_END,
    NATIVE_DRAWN,
    NATIVE_SKIP
};
uint8_t menu_native_draw(uint16_t id);

/* For native_gen.c: draws the cursor on list row `row` into the cleared
   framebuffer. */
void menu_native_cursor(uint8_t row);

/* The platform's (with NATIVE_PLATFORM_TILES). */

/* Shows screen `id`; returns 0 if it cannot. */
uint8_t platform_native_show(uint16_t id);

/* Draws or takes away the cursor on a list row of the screen shown. */
void platform_native_cursor(uint8_t row, uint8_t on);

/* The menus are about to draw something else into the framebuffer: if a
   made screen is up, the screen goes blank at once until that is shown,
   rather than showing the old screen meanwhile. */
void platform_native_blank(void);

/* For the host: writes the tiles of every numbered screen as C source.
   Returns nonzero on failure. */
int native_gen(const char *path);

#endif
