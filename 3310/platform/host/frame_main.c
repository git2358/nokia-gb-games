/* Writes a reference frame for the platform builds.
   Usage: frame NAME OUT.pgm
   NAME: testcard, outline, start (a new game, before any time passes),
   run-N (a new game after N screen frames with no key pressed), or
   menu-KEYS, the screen after the scripted keys of menu_script, or
   menu-KEYS+N, the same N screen frames later. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "games.h"
#include "lcd.h"
#include "menu.h"
#include "pgm.h"
#include "si.h"
#include "sound.h"
#include "testcard.h"

void platform_tone(uint8_t note)
{
    (void)note;
}

void platform_rumble(uint8_t on)
{
    (void)on;
}

void platform_game_starts(uint8_t game)
{
    (void)game;
}

uint8_t platform_settings_load(uint8_t game, struct game_settings *out)
{
    (void)game;
    out->top_score = 0;
    out->level = 0;
    out->option = 0;
    return 0;
}

void platform_settings_save(uint8_t game, const struct game_settings *in)
{
    (void)game;
    (void)in;
}

uint8_t platform_options_load(struct game_options *out)
{
    (void)out;
    return 0;
}

void platform_options_save(const struct game_options *in)
{
    (void)in;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: frame NAME OUT.pgm\n");
        return 2;
    }
    lcd_view_phone();
    lcd_zoom_set(LCD_PHONE_X, LCD_PHONE_Y, LCD_WIDTH, LCD_HEIGHT, LCD_ZOOM);
    if (strcmp(argv[1], "testcard") == 0) {
        testcard_draw();
    } else if (strcmp(argv[1], "outline") == 0) {
        testcard_frame();
    } else if (strcmp(argv[1], "start") == 0 || strncmp(argv[1], "run-", 4) == 0) {
        long frames = argv[1][0] == 'r' ? strtol(argv[1] + 4, 0, 10) : 0;

        games_start(GAME_SPACE_IMPACT, 1, 1);
        while (frames-- > 0)
            games_elapse(16743);
        games_draw(1);
    } else if (strncmp(argv[1], "menu-", 5) == 0) {
        /* menu-KEYS+N lets N more screen frames pass after the keys. */
        char *plus = strchr(argv[1], '+');
        long frames = plus ? strtol(plus + 1, 0, 10) : 0;

        if (plus)
            *plus = 0;
        menu_init();
        menu_script(argv[1] + 5);
        menu_draw();
        while (frames-- > 0)
            if (menu_tick())
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
