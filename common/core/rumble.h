/* How hard the rumble motor runs, for every game in every port. A rumble
   cartridge's motor is only on or off, so it is made gentler two ways:

   - length: every pulse a game asks for is scaled by
     RUMBLE_LENGTH_NUM / RUMBLE_LENGTH_DEN timer units, rounded up so a
     pulse never vanishes; at most 1, since the longest pulse (141 units)
     must still fit a byte. A NUM of 0 turns rumble off altogether.
   - strength: during a pulse the motor runs RUMBLE_ON_FRAMES screen frames
     and rests RUMBLE_OFF_FRAMES, over and over, as Game Boy rumble games
     soften theirs. 1 and 0 is the motor on throughout, as on the phone.

   Each can be overridden with -D on the compiler's command line. */
#ifndef CORE_RUMBLE_H
#define CORE_RUMBLE_H

#include <stdint.h>

#ifndef RUMBLE_LENGTH_NUM
#define RUMBLE_LENGTH_NUM 1
#endif
#ifndef RUMBLE_LENGTH_DEN
#define RUMBLE_LENGTH_DEN 1
#endif
#ifndef RUMBLE_ON_FRAMES
#define RUMBLE_ON_FRAMES 1
#endif
#ifndef RUMBLE_OFF_FRAMES
#define RUMBLE_OFF_FRAMES 1
#endif

#if RUMBLE_LENGTH_NUM > RUMBLE_LENGTH_DEN
#error "RUMBLE_LENGTH_NUM / RUMBLE_LENGTH_DEN must be at most 1"
#endif
#if RUMBLE_ON_FRAMES < 1
#error "RUMBLE_ON_FRAMES must be at least 1"
#endif

/* A pulse of `units` timer units as it is run. */
#define RUMBLE_UNITS(units) \
    ((uint8_t)(((uint16_t)(units) * RUMBLE_LENGTH_NUM + RUMBLE_LENGTH_DEN - 1) / RUMBLE_LENGTH_DEN))

#endif
