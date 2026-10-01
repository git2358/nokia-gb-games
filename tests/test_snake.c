/* Host check that drawing only a move's changes gives the same picture as
   drawing the whole board. Needs the extracted assets. */
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "lcd.h"
#include "rand.h"
#include "snake.h"

void platform_beep(void)
{
}

int main(void)
{
    static uint8_t stepped[sizeof lcd_fb];
    static const char keys[] = "2468";
    unsigned step, games = 1, eaten = 0, seed = 1;

    game_srand(1);
    snake_init(0);
    lcd_clear();
    snake_draw();
    /* Head for the first food, then steer at random. */
    for (step = 0; step < 20000; step++) {
        uint16_t score = snake.score;

        if (step < 5)
            snake_key('2');
        else if (step < 7)
            snake_key('6');
        else if ((seed = seed * 1103515245u + 12345u) >> 16 & 3)
            snake_key(keys[seed >> 20 & 3]);

        if (!snake_step() ) {
            games++;
            snake_init(0);
            lcd_clear();
            snake_draw();
            continue;
        }
        eaten += snake.score != score;
        snake_draw_step();
        memcpy(stepped, lcd_fb, sizeof lcd_fb);
        lcd_clear();
        snake_draw();
        if (memcmp(stepped, lcd_fb, sizeof lcd_fb) != 0) {
            printf("FAIL: step %u differs from a full redraw\n", step);
            return 1;
        }
    }
    if (eaten < 10) {
        printf("FAIL: only %u foods eaten; the test did not cover eating\n", eaten);
        return 1;
    }
    printf("ok (%u games, %u foods)\n", games, eaten);
    return 0;
}
