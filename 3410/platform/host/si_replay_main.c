/* Replays a recorded Space Impact game and writes the frames it produces,
   for comparison with frames captured from the firmware in MAME.

   Usage: si_replay EVENTS OUT_DIR SEED

   EVENTS holds the handler's calls, one "EVENT A B KEYS" in hex per line
   (tools/si_events.py): the event, its arguments, and the keys the game
   would see held then. A frame is written each time the picture changes,
   as OUT_DIR/NNNN.pgm, numbered by the event that produced it. SEED is the
   ANSI generator's state at New game, in hex. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lcd.h"
#include "pgm.h"
#include "rand.h"
#include "si.h"
#include "si_pic.h"
#include "sprite.h"

void si_debug(void);
void si_debug_lives(int8_t lives);

void platform_tone(uint8_t note)
{
    (void)note;
}

void platform_rumble(uint8_t on)
{
    (void)on;
}

int main(int argc, char **argv)
{
    static uint8_t last[sizeof sprite_screen];
    char path[1024];
    unsigned event, a, b, keys, n = 0, written = 0;
    FILE *events;

    if (argc != 4) {
        fprintf(stderr, "usage: si_replay EVENTS OUT_DIR SEED\n");
        return 2;
    }
    events = fopen(argv[1], "r");
    if (!events) {
        perror(argv[1]);
        return 1;
    }
    game_srand((uint32_t)strtoul(argv[3], 0, 16));
    lcd_view_phone();
    memset(last, 0xaa, sizeof last);
    for (; fscanf(events, "%x %x %x %x", &event, &a, &b, &keys) == 4; n++) {
        uint8_t done;
        long at;
        unsigned next = 0xff;

        /* Continue after a pause is New game then the saved state at once:
           the phone shows nothing between the two. */
        at = ftell(events);
        if (fscanf(events, "%x", &next) != 1)
            next = 0xff;
        fseek(events, at, SEEK_SET);

        if (event == 0xff) {
            /* The autopilot set the lives back (tools/si_events.py). */
            si_debug_lives((int8_t)a);
            continue;
        }
        si_keys_held = (uint16_t)keys;
        done = si_event((uint8_t)event, (uint8_t)a);
        if (getenv("SI_DEBUG_FROM") && n >= strtoul(getenv("SI_DEBUG_FROM"), 0, 0)
            && n <= strtoul(getenv("SI_DEBUG_TO"), 0, 0)) {
            printf("%04u %x %x: ", n, event, a);
            si_debug();
        }
        if (!(done & SI_DONE_REDRAW) || (event == SI_EVENT_NEW_GAME && next == SI_EVENT_CONTINUE))
            continue;
        si_pic_render();
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
