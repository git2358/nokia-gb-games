/* The buzzer sounds Snake makes, as traced from the firmware in MAME: the
   pitches are exact, the lengths are the traced ones corrected for MAME's
   slightly fast clock. */
#ifndef CORE_SOUND_H
#define CORE_SOUND_H

#include <stdint.h>

enum {
    SOUND_EAT,       /* one short high blip */
    SOUND_GAME_OVER, /* three quick low pulses */
    SOUND_TOP_SCORE  /* a six-note tune */
};

void sound_play(uint8_t sound);

/* Advances the sound in progress by `us` microseconds. */
void sound_tick(uint16_t us);

/* The platform's buzzer: a square wave of this many hertz, 0 for silence. */
void platform_tone(uint16_t hz);

#endif
