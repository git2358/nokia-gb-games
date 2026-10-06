/* Access to the firmware's Snake II data by firmware address: its bitmaps
   and their 12-byte descriptors, the maze walls, the speeds and the maze
   records, extracted from the user's dump into snake2_data. */
#ifndef CORE_SNAKE2_DATA_H
#define CORE_SNAKE2_DATA_H

#include <stdint.h>

#include "game_assets.h"

/* A place in the data: its firmware address less SNAKE2_DATA_BASE. */
typedef uint16_t snake2_ref;
#define SNAKE2_REF(address) ((snake2_ref)((address) - SNAKE2_DATA_BASE))
#define SNAKE2_ROM(ref) (snake2_data + (ref))

/* Picture sets: 12-byte descriptors (bitmap address, four unused bytes,
   width, height), four to a set, by direction (0 left, 1 up, 2 right,
   3 down) unless noted. The bitmaps are in the LCD's band layout. */
#define SNAKE2_FOOD 0x327e10ul        /* one, 3x3 */
#define SNAKE2_CREATURES 0x327e1cul   /* six small creatures, 8x4 */
#define SNAKE2_CORNERS 0x327e64ul     /* by turn, see snake2.c */
#define SNAKE2_BIG 0x327e94ul         /* the large creature, 8x8, two frames */
#define SNAKE2_BIG_ICON 0x327eacul    /* its icon, 8x4, two frames */
#define SNAKE2_HEADS 0x327ec4ul       /* mouth closed */
#define SNAKE2_TAILS 0x327ef4ul       /* by the direction to the next segment */
#define SNAKE2_BODY 0x327f24ul
#define SNAKE2_BODY_FAT 0x327f54ul
#define SNAKE2_HEADS_OPEN 0x327f84ul
#define SNAKE2_CORNERS_FAT 0x327fb4ul
/* The first and last descriptor: a picture is named by its descriptor's
   place among them, plus one, so that 0 is none and a byte holds it. */
#define SNAKE2_PICTURES_FIRST SNAKE2_FOOD
#define SNAKE2_PICTURE(address) ((uint8_t)(((address) - SNAKE2_PICTURES_FIRST) / 12 + 1))

/* Nine bytes, a level's tick period in tens of milliseconds. */
#define SNAKE2_SPEEDS 0x3280fcul
/* Six 16-byte maze records: number of wall segments, a u32 pointer to
   them, the snake's start cell at +8 and +9. A segment is two cells
   {x, y, 0, 0}, a horizontal or a vertical run. */
#define SNAKE2_MAZES 0x328108ul

#endif
