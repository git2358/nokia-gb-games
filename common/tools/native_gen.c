/* Draws every numbered full-screen menu (native_tiles.h) with the host
   build of the core, at the Game Boy's screen size, and writes them as C
   source for the Game Boy: the tiles they are made of, each once, and for
   each screen which tile is in which of its cells.

   A tile is a cell's 8 rows of framebuffer bytes; the Game Boy takes them
   as the first bit plane, the second being ignored by the palette the
   menus are shown with (native_tiles.c). Tile 0 is the empty one and tile 1
   the full one, which are not written out: the screen names them from two
   tiles of video RAM kept for them.

   A row of a screen is the number of its cells that are not empty and then
   for each one (column << 11 | tile). Rows alike are kept once, and a
   screen is where each of its 18 rows is among them. The
   cursor on each list row is the cells it touches, with their bytes as
   drawn on an empty screen. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lcd.h"
#include "native_tiles.h"

#define CELLS_X (LCD_FB_WIDTH / 8)
#define CELLS_Y (LCD_FB_HEIGHT / 8)
#define MAX_TILES 2048
#define MAX_SCREENS 1024

static uint8_t tiles[MAX_TILES][8];
static unsigned tile_count = 2;

static void cell_bytes(unsigned tx, unsigned ty, uint8_t out[8])
{
    unsigned row;

    for (row = 0; row < 8; row++)
        out[row] = lcd_fb[(ty * 8 + row) * LCD_STRIDE + tx];
}

static unsigned tile_id(const uint8_t bytes[8])
{
    static const uint8_t empty[8], full[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    unsigned i;

    if (!memcmp(bytes, empty, 8))
        return 0;
    if (!memcmp(bytes, full, 8))
        return 1;
    for (i = 2; i < tile_count; i++)
        if (!memcmp(tiles[i], bytes, 8))
            return i;
    if (tile_count == MAX_TILES) {
        fprintf(stderr, "native_gen: more than %d tiles\n", MAX_TILES);
        exit(1);
    }
    memcpy(tiles[tile_count], bytes, 8);
    return tile_count++;
}

static void fresh(void)
{
    lcd_view_full();
    lcd_zoom_set(0, 0, 0, 0, 1);
    lcd_clear();
}

/* The rows kept so far, end to end; returns where `row` is among them,
   adding it if it is new. */
static uint16_t rows[MAX_SCREENS * (CELLS_Y + CELLS_X * CELLS_Y)];
static unsigned rows_used;

static unsigned keep_row(const uint16_t *row)
{
    unsigned at = 0, size = row[0] + 1u;

    while (at < rows_used) {
        if (rows[at] == row[0] && !memcmp(rows + at, row, size * sizeof *row))
            return at;
        at += rows[at] + 1u;
    }
    memcpy(rows + rows_used, row, size * sizeof *row);
    rows_used += size;
    return at;
}

int native_gen(const char *path)
{
    static uint16_t screen_rows[MAX_SCREENS * CELLS_Y];
    static long start[MAX_SCREENS];
    unsigned ids = 0, used = 0, n = 0, screens = 0, i, row;
    FILE *out = fopen(path, "w");

    if (!out)
        return 1;
    for (;; ids++) {
        uint8_t drawn;
        unsigned tx, ty;

        if (ids == MAX_SCREENS) {
            fprintf(stderr, "native_gen: more than %d screens\n", MAX_SCREENS);
            return 1;
        }
        fresh();
        drawn = menu_native_draw((uint16_t)ids);
        if (drawn == NATIVE_END)
            break;
        start[ids] = -1;
        if (drawn != NATIVE_DRAWN)
            continue;
        start[ids] = n;
        screens++;
        for (ty = 0; ty < CELLS_Y; ty++) {
            uint16_t one[1 + CELLS_X];

            one[0] = 0;
            for (tx = 0; tx < CELLS_X; tx++) {
                uint8_t bytes[8];
                unsigned id;

                cell_bytes(tx, ty, bytes);
                id = tile_id(bytes);
                if (!id)
                    continue;
                one[1 + one[0]++] = (uint16_t)(tx << 11 | id);
            }
            screen_rows[n++] = (uint16_t)keep_row(one);
        }
    }

    fprintf(out, "/* Made by native_gen.c from the firmware's fonts and text: not to be\n"
                 "   committed. */\n#include <stdint.h>\n\n");
    /* On the Game Boy the tiles fill a bank of their own and the file is
       compiled once for each part; elsewhere once, with both. */
    fprintf(out, "#ifndef NATIVE_PART_SCREENS\n");
    fprintf(out, "const uint8_t native_tiles[][8] = {\n");
    for (i = 2; i < tile_count; i++) {
        unsigned b;

        fprintf(out, "    {");
        for (b = 0; b < 8; b++)
            fprintf(out, " 0x%02x,", tiles[i][b]);
        fprintf(out, " },\n");
    }
    fprintf(out, "};\n#endif\n#ifndef NATIVE_PART_TILES\n\nconst uint16_t native_screen_count = %u;\n\n", ids);
    fprintf(out, "const uint16_t native_row_data[] = {");
    for (i = 0; i < rows_used; i++)
        fprintf(out, "%s0x%04x,", i % 12 ? " " : "\n    ", rows[i]);
    fprintf(out, "\n};\n\nconst uint16_t native_screen_rows[] = {");
    for (i = 0; i < n; i++)
        fprintf(out, "%s%u,", i % CELLS_Y ? " " : "\n    ", screen_rows[i]);
    fprintf(out, "\n};\n\nconst uint16_t native_screen_start[] = {");
    for (i = 0; i < ids; i++)
        fprintf(out, "%s%ld,", i % 12 ? " " : "\n    ", start[i] < 0 ? 0xffffL : start[i]);
    fprintf(out, "\n};\n\n");

    /* The cursor, row by row: column, row of cells, then the cell's bytes. */
    fprintf(out, "const uint8_t native_cursor_data[] = {");
    {
        unsigned cursor_start[NATIVE_CURSOR_ROWS + 1];

        for (row = 0; row < NATIVE_CURSOR_ROWS; row++) {
            unsigned tx, ty;

            cursor_start[row] = used;
            fresh();
            menu_native_cursor((uint8_t)row);
            for (ty = 0; ty < CELLS_Y; ty++)
                for (tx = 0; tx < CELLS_X; tx++) {
                    static const uint8_t empty[8];
                    uint8_t bytes[8];
                    unsigned b;

                    cell_bytes(tx, ty, bytes);
                    if (!memcmp(bytes, empty, 8))
                        continue;
                    fprintf(out, "\n    %u, %u,", tx, ty);
                    for (b = 0; b < 8; b++)
                        fprintf(out, " 0x%02x,", bytes[b]);
                    used += 10;
                }
        }
        cursor_start[NATIVE_CURSOR_ROWS] = used;
        fprintf(out, "\n};\n\nconst uint16_t native_cursor_start[] = {");
        for (row = 0; row <= NATIVE_CURSOR_ROWS; row++)
            fprintf(out, " %u,", cursor_start[row]);
        fprintf(out, " };\n#endif\n");
    }
    if (fclose(out))
        return 1;
    printf("%s: %u screens, %u tiles (%u bytes), %u bytes of screens, %u of cursor\n", path, screens,
           tile_count - 2, (tile_count - 2) * 8, rows_used * 2 + n * 2 + ids * 2, used);
    return 0;
}
