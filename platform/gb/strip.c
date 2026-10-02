/* The game at 2x on the Game Boy.

   A tile is four columns by four rows of the phone's picture, and making a
   tile from the picture is slow. Most of the picture stands still from one
   tick to the next and only the tiles that changed are made again; but the
   terrain, a strip two bands high along the bottom of the picture (the top
   in two levels), moves a column every tick, and that would be up to 80
   tiles a tick. So the strip is not made from the picture. Its four rows
   of the tile map hold tiles made once per level, one for every piece of
   every kind of terrain tile, laid out as the level's terrain is, and the
   LCD is told to scroll just those rows (the cuts in crt0.s). The picture
   is still drawn whole, terrain and all; each tick the strip's cells are
   compared with the terrain alone, and where a cell holds anything else,
   a ship or a shot or a wreck over the terrain, it gets a tile of its own
   made from the picture, until it is plain terrain again. What is on the
   screen is therefore always the picture, to the pixel.

   Tiles, by the number the tile map names them with:
     0..159    the four bands that are not the strip, eight rows of 20
               tiles, at 0x8000;
     160..243  the strip's own tiles, one for each of its 21 cells across
               and 4 down, at 0x8a00;
     255       an empty tile, at 0x8ff0;
   and, in the strip only, where the LCD takes tiles 0..127 from 0x9000:
     0..95     the terrain: 16 for each kind of terrain tile, which is 8
               cells across and 2 down;
     96        a tile with every pixel set, the empty terrain of a level
               drawn light on dark. */
#include "strip.h"

#include <string.h>

#include "si_state.h"

#define REG(addr) (*(volatile uint8_t *)(addr))
#define LCDC_LOW 0x91  /* LCD on, tiles 0..127 at 0x8000 */
#define LCDC_HIGH 0x81 /* LCD on, tiles 0..127 at 0x9000 */

#define MAP ((uint8_t *)0x9800)
#define TILE_LOW(id) ((uint8_t *)0x8000 + (uint16_t)(id) * 16)
#define TILE_HIGH(id) ((uint8_t *)0x9000 + (uint16_t)(id) * 16)

#define CELLS 21        /* cells of the strip the screen can show part of */
#define ID_OWN 160
#define ID_EMPTY 255
#define ID_FULL 96
#define NO_PLACE 0xff

extern uint8_t gb_present_all, gb_zoom_band, gb_columns[4];
void gb_present_zoom(void);
void gb_clear_tiles(void);
void gb_tile_low(uint8_t *tile);
void gb_tile_high(uint8_t *tile);
void gb_vram_put(uint8_t *address, uint8_t value);

/* The terrain the strip is laid out for. */
static const uint8_t *terrain_map;
static const uint16_t *terrain_tiles;
static uint8_t terrain_top, terrain_polarity;
uint8_t strip_invert;       /* 0xff when the level is drawn light on dark */
#define invert strip_invert
static uint8_t empty_id;    /* the tile of empty terrain */
static uint8_t strip_row;   /* the strip's first row of tiles on the screen */
static uint8_t strip_band;  /* and its first band of the picture */
uint8_t strip_place = NO_PLACE; /* the terrain cell at the strip's left edge, 0..127 */
#define place strip_place
/* For each cell of the strip on the screen, across and down: whether the
   tile map names the cell's own tile there, and for each band the bytes of
   the picture its own tiles were made from. */
uint8_t strip_own[CELLS][4];
#define own strip_own
static uint8_t made[CELLS][2][4];

/* For the pass over the strip's cells: the picture is `fine` columns to the
   left of the cells, and these are the band of the picture and of the
   terrain's bitmap being gone over. strip_scan (draw.s) goes over a band's
   cells and calls strip_cell for those that are not plain terrain already
   shown as such. */
uint8_t strip_fine;
const uint8_t *strip_picture, *strip_terrain;
void strip_scan(uint8_t band);
void strip_cell(uint8_t across, uint8_t band);

void lcd_cuts(uint8_t scx, uint8_t line0, uint8_t lcdc0, uint8_t scx0, uint8_t line1, uint8_t lcdc1, uint8_t scx1)
{
    __asm__("di");
    lcd_scx = scx;
    lcd_cut[0].line = line0;
    lcd_cut[0].lcdc = lcdc0;
    lcd_cut[0].scx = scx0;
    lcd_cut[1].line = line1;
    lcd_cut[1].lcdc = lcdc1;
    lcd_cut[1].scx = scx1;
    __asm__("ei");
}

void strip_leave(uint8_t scx, uint8_t write_map)
{
    uint8_t tx, ty;

    lcd_cuts(scx, 12 * 8 - 1, LCDC_HIGH, scx, 0xff, LCDC_HIGH, scx);
    place = NO_PLACE;
    terrain_map = 0;
    if (!write_map)
        return;
    /* The strip used all 32 columns of its rows; past the screen's 20 the
       map is tile 0 again, as it was, because the phone-sized game scrolls
       the 21st into view. */
    for (ty = 0; ty < 18; ty++)
        for (tx = 0; tx < 32; tx++)
            gb_vram_put(MAP + ty * 32 + tx, tx < 20 ? (uint8_t)((ty * 20 + tx) % 240) : 0);
}

/* The kind of terrain tile at a terrain cell of a band, 0 for none. */
static uint8_t kind_at(uint8_t cell, uint8_t band)
{
    return terrain_map[(uint8_t)(band << 4) + ((cell >> 3) & 15)];
}

/* The tile the map names for plain terrain at a cell, `down` rows into the
   strip. */
static uint8_t terrain_id(uint8_t cell, uint8_t down)
{
    uint8_t kind = kind_at(cell, down >> 1);

    return kind ? (uint8_t)(((kind - 1) << 4) + ((cell & 7) << 1) + (down & 1)) : empty_id;
}

/* The four bytes of the terrain at a cell of a band, as the picture has
   them when nothing else is there. */
static void terrain_bytes(uint8_t cell, uint8_t band, uint8_t *out)
{
    uint8_t kind = kind_at(cell, band), i;
    const uint8_t *tile;

    if (!kind) {
        out[0] = out[1] = out[2] = out[3] = invert;
        return;
    }
    tile = tilemap_tile(terrain_tiles[kind - 1]) + ((cell & 7) << 2);
    for (i = 0; i < 4; i++)
        out[i] = tile[i] ^ invert;
}

static uint8_t *map_at(uint8_t cell, uint8_t down)
{
    return MAP + (uint16_t)(strip_row + down) * 32 + (cell & 31);
}

/* A column of the strip's tile map for a terrain cell. */
static void map_column(uint8_t cell)
{
    uint8_t down;

    for (down = 0; down < 4; down++)
        gb_vram_put(map_at(cell, down), terrain_id(cell, down));
}

/* Lays the screen out for the level's terrain: the tile map, the terrain's
   tiles, and where the cuts are. */
static void strip_setup(void)
{
    uint8_t tx, ty, kinds = 0, kind, i, region_row;

    terrain_map = si.terrain.map;
    terrain_tiles = si.terrain.tiles;
    terrain_top = si.terrain.top;
    terrain_polarity = si.polarity;
    invert = si.polarity == 2 ? 0xff : 0;
    empty_id = invert ? ID_FULL : ID_EMPTY;
    strip_row = terrain_top ? 3 : 11;
    strip_band = terrain_top ? 0 : 4;
    region_row = terrain_top ? 7 : 3;
    gb_zoom_band = terrain_top ? 2 : 0;

    /* Nothing scrolls until the strip is laid out. */
    lcd_cuts(0, (uint8_t)(strip_row * 8 - 1), LCDC_HIGH, 0, (uint8_t)(strip_row * 8 + 31), LCDC_LOW, 0);
    for (ty = 0; ty < 18; ty++) {
        if ((uint8_t)(ty - strip_row) < 4)
            continue;
        for (tx = 0; tx < 20; tx++)
            gb_vram_put(MAP + ty * 32 + tx,
                        (uint8_t)(ty - region_row) < 8 ? (uint8_t)((ty - region_row) * 20 + tx) : ID_EMPTY);
    }

    /* The terrain's tiles. */
    for (i = 0; i < 32; i++)
        if (terrain_map[i] > kinds)
            kinds = terrain_map[i];
    for (kind = 0; kind < kinds; kind++) {
        const uint8_t *tile = tilemap_tile(terrain_tiles[kind]);

        for (i = 0; i < 8; i++, tile += 4) {
            gb_columns[0] = tile[0] ^ invert;
            gb_columns[1] = tile[1] ^ invert;
            gb_columns[2] = tile[2] ^ invert;
            gb_columns[3] = tile[3] ^ invert;
            gb_tile_low(TILE_HIGH((kind << 4) + (i << 1)));
            gb_tile_high(TILE_HIGH((kind << 4) + (i << 1) + 1));
        }
    }
    gb_columns[0] = gb_columns[1] = gb_columns[2] = gb_columns[3] = 0xff;
    gb_tile_low(TILE_HIGH(ID_FULL));

    memset(own, 0, sizeof own);
    place = NO_PLACE;
    gb_present_all = 1;
}

/* Moves the strip's tile map to where the terrain now is: cells that had
   tiles of their own go back to plain terrain, and the cells that come
   into view are written. */
static void strip_move(uint8_t to)
{
    uint8_t across, down;

    if (place != NO_PLACE) {
        for (across = 0; across < CELLS; across++) {
            for (down = 0; down < 4; down++) {
                if (own[across][down]) {
                    own[across][down] = 0;
                    gb_vram_put(map_at((uint8_t)(place + across), down), terrain_id((place + across) & 127, down));
                }
            }
        }
    }
    if (place != NO_PLACE && to == ((place + 1) & 127)) {
        map_column((to + CELLS - 1) & 127);
    } else {
        for (across = 0; across < CELLS; across++)
            map_column((to + across) & 127);
    }
    place = to;
}

/* One tile of the strip, `across` cells along and `down` rows into it,
   which is terrain cell `cell`: `now` is what the picture has in the cell's
   band and `differs` whether this row's half of it is anything but plain
   terrain. If so the cell shows a tile of its own made from the picture,
   and if not the terrain's tile. */
static void strip_tile(uint8_t across, uint8_t down, uint8_t cell, uint8_t differs, const uint8_t *now)
{
    uint8_t mask = down & 1 ? 0xf0 : 0x0f, *was = made[across][down >> 1], remake, i, id;

    if (!differs) {
        if (own[across][down]) {
            own[across][down] = 0;
            gb_vram_put(map_at(cell, down), terrain_id(cell, down));
        }
        return;
    }
    remake = !own[across][down];
    for (i = 0; i < 4; i++) {
        if ((now[i] ^ was[i]) & mask)
            remake = 1;
        was[i] = (was[i] & (uint8_t)~mask) | (now[i] & mask);
    }
    id = (uint8_t)(ID_OWN + (across << 2) + down);
    if (remake) {
        memcpy(gb_columns, now, 4);
        if (down & 1)
            gb_tile_high(TILE_LOW(id));
        else
            gb_tile_low(TILE_LOW(id));
    }
    if (!own[across][down]) {
        own[across][down] = 1;
        gb_vram_put(map_at(cell, down), id);
    }
}

/* A cell of the strip that may need something done: it is not plain
   terrain, or was not, or the quick comparison strip_scan makes does not
   hold for it. */
void strip_cell(uint8_t across, uint8_t band)
{
    uint8_t cell = (place + across) & 127, i, differs = 0;
    int8_t x = (int8_t)((across << 2) - strip_fine);
    uint8_t now[4], plain[4];

    /* What the picture has in the cell, and what plain terrain would be.
       The terrain's own bitmap says the latter, but not at the first cell
       of a terrain tile, where the phone draws one column from the tile
       before, nor outside the picture. */
    if ((cell & 7) && x >= 0) {
        const uint8_t *p = strip_picture + x, *t = strip_terrain + x;

        for (i = 0; i < 4; i++) {
            now[i] = p[i];
            differs |= now[i] ^ t[i] ^ invert;
        }
    } else {
        terrain_bytes(cell, band, plain);
        for (i = 0; i < 4; i++) {
            int8_t at = (int8_t)(x + i);

            now[i] = at >= 0 && at < 84 ? strip_picture[at] : plain[i];
            differs |= now[i] ^ plain[i];
        }
    }
    strip_tile(across, band << 1, cell, differs & 0x0f, now);
    strip_tile(across, (uint8_t)((band << 1) + 1), cell, differs & 0xf0, now);
}

void strip_present(uint8_t all)
{
    uint16_t scroll;
    uint8_t band;

    if (all) {
        gb_clear_tiles();
        terrain_map = 0;
    }
    if (terrain_map != si.terrain.map || terrain_tiles != si.terrain.tiles || terrain_top != si.terrain.top
        || terrain_polarity != si.polarity)
        strip_setup();

    scroll = si.terrain.scroll & 511;
    strip_fine = (uint8_t)scroll & 3;
    if ((uint8_t)(scroll >> 2) != place)
        strip_move((uint8_t)(scroll >> 2));
    /* The LCD scrolls the strip: 32 cells of 8 pixels go round, and a
       column of the picture is 2 pixels. */
    __asm__("di");
    lcd_cut[0].scx = (uint8_t)(((place & 31) << 3) + (strip_fine << 1));
    __asm__("ei");

    for (band = 0; band < 2; band++) {
        strip_picture = sprite_screen + (strip_band + band) * 84;
        strip_terrain = si.terrain.bitmap + band * 84;
        strip_scan(band);
    }
    gb_present_zoom();
}
