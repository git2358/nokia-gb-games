#include "sound.h"

#include "game_assets.h"

volatile uint8_t sound_active;

static volatile uint8_t requested; /* where a sound to start is in game_sounds, plus one; 0 for none */
static const uint8_t *next;        /* the note after the one sounding, or null */
static uint8_t left;               /* units the note sounding still has */

/* Only leaves a request: sound_tick runs from an interrupt, and it alone
   touches the notes. */
void sound_play(uint8_t sound)
{
    uint8_t place;

    if (sound < GAME_SOUND_FIRST || sound >= GAME_SOUND_FIRST + sizeof game_sound_places)
        return;
    place = game_sound_places[sound - GAME_SOUND_FIRST];
    if (place == 0xff)
        return;
    requested = (uint8_t)(place + 1);
    sound_active = 1;
}

void sound_tick(void)
{
    uint8_t request = requested;

    if (request) {
        requested = 0;
        next = game_sounds + request - 1;
    } else if (!next || --left) {
        return;
    }
    if (*next == SOUND_SILENCE) {
        next = 0;
        sound_active = 0;
        platform_tone(SOUND_SILENCE);
        return;
    }
    platform_tone(next[0] == SOUND_REST ? SOUND_SILENCE : next[0]);
    left = next[1];
    next += 2;
}
