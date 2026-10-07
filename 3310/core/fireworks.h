/* The fireworks the phone's games framework shows after a new top score
   and a won game of Bantumi: six full-screen pictures, read from the
   user's dump, the last of them all black. And the Top score page's
   animation, whose pictures are kept with them. */
#ifndef CORE_FIREWORKS_H
#define CORE_FIREWORKS_H

#include <stdint.h>

/* Puts picture `picture` (0 to 5) into sprite_screen. */
void fireworks_draw(uint8_t picture);

/* The Top score page's animation: its steps, and step `step` drawn on
   the LCD. */
#define SPARKLE_STEPS 19
void sparkle_draw(uint8_t step);

#endif
