/* Access to the firmware's Snake II data by firmware address, extracted
   from the user's dump: its bitmaps and their 24-byte descriptors, the
   maze walls and records, and the speeds. */
#ifndef CORE_SNAKE2_DATA_H
#define CORE_SNAKE2_DATA_H

#include <stdint.h>

#include "game_assets.h"

/* A place in the pictures or the mazes: its firmware address less the
   part's base. Every pointer in them lies within 64 KiB of its base. */
#define SNAKE2_PICTURE_AT(address) (snake2_pictures + (uint16_t)((address) - SNAKE2_PICTURES_BASE))
#define SNAKE2_MAZE_AT(address) (snake2_mazes + (uint16_t)((address) - SNAKE2_MAZES_BASE))

/* Picture sets: 24-byte descriptors (u16 width, u16 height, two bytes
   unused, u16 1, u32 bitmap address, twelve bytes unused), four to a set,
   by direction (0 left, 1 up, 2 right, 3 down) unless noted. The bitmaps
   are in the LCD's band layout. The 3410 has no large creature. */
#define SNAKE2_DESCRIPTOR 24
#define SNAKE2_FOOD 0x4b2ef4ul        /* one, 3x3 */
#define SNAKE2_CREATURES 0x4b2f0cul   /* six small creatures, 8x4 */
#define SNAKE2_CORNERS 0x4b2f9cul     /* by turn, see snake2.c */
#define SNAKE2_HEADS 0x4b2ffcul       /* mouth closed */
#define SNAKE2_TAILS 0x4b305cul       /* by the direction to the next segment */
#define SNAKE2_BODY 0x4b30bcul
#define SNAKE2_BODY_FAT 0x4b311cul
#define SNAKE2_HEADS_OPEN 0x4b317cul
#define SNAKE2_CORNERS_FAT 0x4b31dcul
/* The first descriptor: a picture is named by its descriptor's place among
   them, plus one, so that 0 is none and a byte holds it. */
#define SNAKE2_PICTURES_FIRST SNAKE2_FOOD
#define SNAKE2_PICTURE(address) ((uint8_t)(((address) - SNAKE2_PICTURES_FIRST) / SNAKE2_DESCRIPTOR + 1))

/* Six 12-byte maze records: a u32 pointer to the walls, the first snake's
   start cell at +4 and +5, a second snake's at +6 and +7, two bytes the
   game does not read, the number of walls at +10. A wall is four bytes
   {x1, y1, x2, y2}, a horizontal or a vertical run. */
#define SNAKE2_MAZES 0x497458ul
#define SNAKE2_MAZE_RECORD 12

#endif
