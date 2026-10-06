/* Access to the firmware's Space Impact data by firmware address. */
#ifndef CORE_SI_DATA_H
#define CORE_SI_DATA_H

#include <stdint.h>

#include "game_assets.h"
#include "sprite.h"

/* A place in the data: its firmware address less SI_DATA_BASE. The data
   is under 64 KiB, so 16 bits hold it, which matters on the Game Boy. */
typedef uint16_t si_ref;
#define SI_REF(address) ((si_ref)((address) - SI_DATA_BASE))
#define SI_NO_REF 0xffff
/* A platform that keeps the data somewhere other than where the linker
   put si_data, as the Game Boy does, names the address in SI_DATA_AT. */
#ifdef SI_DATA_AT
#define SI_ROM(ref) ((const uint8_t *)(SI_DATA_AT) + (ref))
#else
#define SI_ROM(ref) (si_data + (ref))
#endif

/* Tables in the data, by address. */
#define SI_TEMPLATES SI_REF(0x3131bcul)    /* 12 bytes per object type */
#define SI_ICON_LIFE SI_REF(0x313180ul)    /* sprite descriptors */
#define SI_ICON_MISSILE SI_REF(0x313198ul)
#define SI_ICON_WALL SI_REF(0x3131a4ul)
#define SI_ICON_BEAM SI_REF(0x3131b0ul)
#define SI_SHIP_FIRST_LEVEL SI_REF(0x312dccul) /* the ship's sprite in the first level */
#define SI_YPATH_WAVE SI_REF(0x312104ul)      /* y by x: a wave across the screen */
#define SI_YPATH_RIPPLE SI_REF(0x312158ul)    /* y offset by x, -4..4 */
#define SI_YPATH_FALL SI_REF(0x3121acul)
#define SI_YPATH_RISE SI_REF(0x312200ul)

/* A 12-byte sprite descriptor: bitmap address, four unused bytes, width,
   height. */
void si_image(struct sprite_image *out, si_ref descriptor);

#endif
