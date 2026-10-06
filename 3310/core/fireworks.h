/* The fireworks the phone's games framework shows after a new top score
   and a won game of Bantumi: six full-screen pictures, read from the
   user's dump, the last of them all black. */
#ifndef CORE_FIREWORKS_H
#define CORE_FIREWORKS_H

#include <stdint.h>

/* Puts picture `picture` (0 to 5) into sprite_screen. */
void fireworks_draw(uint8_t picture);

#endif
