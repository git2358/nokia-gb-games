/* The ROM is banks of 16 KiB, of which the cartridge (MBC5) maps the
   first always and one of the others at a time:

   0  startup, this layer, the framebuffer, the sprite layer, the game's
      timers and the sounds, which play from an interrupt;
   1  the menus, the fonts, the text and what the cartridge RAM keeps;
   2  Snake II and its data, the score's digits among it, and its title
      and game-over pictures.

   The main loop runs with bank 1 mapped. A call into bank 2 goes through
   one of the far_ functions here, which map that bank for the call and
   then the one that was mapped before. */
#ifndef GB_FAR_H
#define GB_FAR_H

#include <stdint.h>

enum {
    BANK_MENU = 1,
    BANK_SNAKE
};

void far_bank(uint8_t bank);

#endif
