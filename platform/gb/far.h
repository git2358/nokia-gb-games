/* The ROM is banks of 16 KiB, of which the cartridge (MBC5) maps the
   first always and one of the others at a time:

   0  startup, this layer, the framebuffer, the sprite layer, the game's
      timers and the helpers every part of Space Impact uses;
   1  the menus, the fonts, the text and what the cartridge RAM keeps;
   2  Space Impact's play;
   3  how its games, levels and ships begin, the image of its data that
      is copied to cartridge RAM at power-on, and the games' titles;
   4  Snake II and its data, and the games at 2x (strip.c).

   The main loop runs with bank 1 mapped. A call into bank 2, 3 or 4 goes
   through one of the far_ functions here, which map that bank for the call
   and then the one that was mapped before. */
#ifndef GB_FAR_H
#define GB_FAR_H

#include <stdint.h>

enum {
    BANK_MENU = 1,
    BANK_PLAY,
    BANK_SETUP,
    BANK_SNAKE
};

void far_bank(uint8_t bank);

#endif
