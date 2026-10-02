/* Writes a reference frame for the platform builds and the MAME comparison.
   Usage: frame NAME OUT.pgm
   NAME: testcard, outline, snake-start, or menu-KEYS where KEYS is the menu
   keys pressed from the first screen (u up, d down, l left, r right,
   s select, b back, a the full-screen key, t one move of the running game,
   F forget what is drawn so the next draw is a full one). */
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "lcd.h"
#include "menu.h"
#include "pgm.h"
#include "snake.h"
#include "testcard.h"

/* The host keeps no settings: every frame starts from a fresh cartridge. */
void platform_settings_load(uint8_t game, struct game_settings *out)
{
    (void)game;
    out->top_score = 0;
    out->level = 0;
}

void platform_settings_save(uint8_t game, const struct game_settings *in)
{
    (void)game;
    (void)in;
}

void platform_beep(void)
{
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: frame NAME OUT.pgm\n");
        return 2;
    }
    if (strcmp(argv[1], "testcard") == 0) {
        testcard_draw();
    } else if (strcmp(argv[1], "outline") == 0) {
        testcard_frame();
    } else if (strcmp(argv[1], "snake-start") == 0) {
        lcd_clear();
        snake_init(0, SNAKE_COLS, SNAKE_ROWS);
        snake_draw();
    } else if (strncmp(argv[1], "menu-", 5) == 0) {
        const char *key;

        menu_init();
        for (key = argv[1] + 5; *key; key++) {
            /* Draw after every key, as a platform does, so that drawing only
               a move's changes is exercised. */
            menu_draw();
            if (*key == 'F')
                menu_redraw_all();
            else if (*key == 't')
                menu_game_step();
            else
                menu_key(*key == 'u' ? MENU_KEY_UP : *key == 'd' ? MENU_KEY_DOWN : *key == 'l' ? MENU_KEY_LEFT
                         : *key == 'r' ? MENU_KEY_RIGHT : *key == 's' ? MENU_KEY_SELECT : *key == 'a' ? MENU_KEY_ALT
                         : MENU_KEY_BACK);
        }
        menu_draw();
    } else {
        fprintf(stderr, "unknown frame %s\n", argv[1]);
        return 2;
    }
    if (pgm_write_lcd(argv[2]) != 0) {
        perror(argv[2]);
        return 1;
    }
    printf("wrote %s\n", argv[2]);
    return 0;
}
