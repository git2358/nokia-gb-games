/* Writes a reference frame for the platform builds and the MAME comparison.
   Usage: frame NAME OUT.pgm
   NAME: testcard, outline, snake-start, or menu-KEYS where KEYS is the menu
   keys pressed from the first screen (u up, d down, s select, b back). */
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "menu.h"
#include "pgm.h"
#include "snake.h"
#include "testcard.h"

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
        snake_init();
        snake_draw();
    } else if (strncmp(argv[1], "menu-", 5) == 0) {
        const char *key;

        menu_init();
        for (key = argv[1] + 5; *key; key++)
            menu_key(*key == 'u' ? MENU_KEY_UP : *key == 'd' ? MENU_KEY_DOWN : *key == 's' ? MENU_KEY_SELECT : MENU_KEY_BACK);
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
