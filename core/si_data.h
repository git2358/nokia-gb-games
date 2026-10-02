/* Access to the firmware's Space Impact data by firmware address. */
#ifndef CORE_SI_DATA_H
#define CORE_SI_DATA_H

#include <stdint.h>

#include "game_assets.h"
#include "sprite.h"

#define SI_ROM(address) (si_data + ((address) - SI_DATA_BASE))

/* Tables in the data, by address. */
#define SI_TEMPLATES 0x3131bcul    /* 12 bytes per object type */
#define SI_ICON_LIFE 0x313180ul    /* sprite descriptors */
#define SI_ICON_MISSILE 0x313198ul
#define SI_ICON_WALL 0x3131a4ul
#define SI_ICON_BEAM 0x3131b0ul
#define SI_SHIP_FIRST_LEVEL 0x312dccul /* the ship's sprite in the first level */
#define SI_YPATH_WAVE 0x312104ul      /* y by x: a wave across the screen */
#define SI_YPATH_RIPPLE 0x312158ul    /* y offset by x, -4..4 */
#define SI_YPATH_FALL 0x3121acul
#define SI_YPATH_RISE 0x312200ul

/* A 12-byte sprite descriptor: bitmap address, four unused bytes, width,
   height. */
void si_image(struct sprite_image *out, uint32_t descriptor);

#endif
