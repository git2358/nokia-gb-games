/* The games' sounds on the phone's buzzer: each a run of notes read from
   the firmware (si_sounds in the extracted assets). */
#ifndef CORE_SOUND_H
#define CORE_SOUND_H

#include <stdint.h>

/* Starts one of the games' sounds (an SI_SOUND_ or SNAKE2_SOUND_ code) at
   the next sound_tick, in place of the one in progress, as on the phone. */
void sound_play(uint8_t sound);

/* Lets one of the phone's timer units (GAMES_UNIT_US) pass for the sound in
   progress. A platform calls it from a timer interrupt, since the notes
   are shorter than a screen frame, and need not while sound_active is 0. */
void sound_tick(void);
extern volatile uint8_t sound_active;

/* The platform's buzzer: a square wave this many semitones above 440 Hz,
   0 to 47, or SOUND_SILENCE. In a sound, SOUND_REST in place of a note is
   silence for its length, and SOUND_SILENCE ends the sound. */
#define SOUND_SILENCE 0xff
#define SOUND_REST 0xfe
void platform_tone(uint8_t note);

#endif
