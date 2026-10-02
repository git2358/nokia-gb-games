/* Replays a recorded sequence of game events and writes the frames it
   produces, for comparison with frames captured from the firmware in MAME.

   Usage: replay EVENTS OUT_DIR [SEED16]

   EVENTS holds the game events in hex, separated by white space, starting
   with 2b (new game). A frame is written each time the picture changes,
   as OUT_DIR/NNNN.pgm, numbered by the event that produced it. SEED16 is
   the games' random seed at the start, in hex; on the phone the title
   animation has drawn from it by then. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games.h"
#include "lcd.h"
#include "pgm.h"
#include "rand.h"
#include "si.h"
#include "sprite.h"

void platform_sound(uint8_t sound)
{
    (void)sound;
}

void platform_vibrate(void)
{
}

int main(int argc, char **argv)
{
    static uint8_t last[sizeof sprite_screen];
    char path[1024];
    unsigned event, n = 0, written = 0;
    FILE *events;

    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: replay EVENTS OUT_DIR [SEED16]\n");
        return 2;
    }
    events = fopen(argv[1], "r");
    if (!events) {
        perror(argv[1]);
        return 1;
    }
    if (argc == 4)
        game_rand16_seed = (uint16_t)strtoul(argv[3], 0, 16);
    memset(last, 0xaa, sizeof last);
    for (; fscanf(events, "%x", &event) == 1; n++) {
        if (!games_event((int)event))
            continue;
        sprite_render();
        if (memcmp(last, sprite_screen, sizeof last) == 0)
            continue;
        memcpy(last, sprite_screen, sizeof last);
        sprite_present(1);
        snprintf(path, sizeof path, "%s/%04u.pgm", argv[2], n);
        if (pgm_write_lcd(path) != 0) {
            perror(path);
            return 1;
        }
        written++;
    }
    fclose(events);
    printf("%u events, %u frames in %s\n", n, written, argv[2]);
    return 0;
}
