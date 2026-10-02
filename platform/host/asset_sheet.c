/* Draws the extracted sprites and terrain tiles as PGM sheets, to check the
   extraction and the bitmap format by eye. Usage: asset_sheet OUT_DIR */
#include <stdio.h>
#include <string.h>

#include "game_assets.h"
#include "si.h"
#include "si_data.h"

void platform_vibrate(void)
{
}

#define SHEET_W 256
#define SHEET_H 192

static unsigned char sheet[SHEET_W * SHEET_H];

static void blit(int x0, int y0, const uint8_t *bitmap, int w, int h)
{
    int x, y;

    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            if (x0 + x < SHEET_W && y0 + y < SHEET_H && (bitmap[x + w * (y >> 3)] >> (y & 7) & 1))
                sheet[(y0 + y) * SHEET_W + x0 + x] = 0;
}

static int write_sheet(const char *dir, const char *name)
{
    char path[1024];
    FILE *f;

    snprintf(path, sizeof path, "%s/%s.pgm", dir, name);
    f = fopen(path, "wb");
    if (!f) {
        perror(path);
        return 1;
    }
    fprintf(f, "P5\n%d %d\n255\n", SHEET_W, SHEET_H);
    fwrite(sheet, 1, sizeof sheet, f);
    printf("wrote %s\n", path);
    return fclose(f) != 0;
}

int main(int argc, char **argv)
{
    /* The terrain tiles of the eight levels' sets, in address order. */
    static const uint32_t tiles[] = {
        0x312254, 0x312274, 0x312294, 0x3122b4, 0x3124b8, 0x3124d8, 0x3124f8, 0x312518, 0x3126b8, 0x3126d8,
        0x3126f8, 0x312718, 0x312760, 0x312780, 0x3127a0, 0x3127e4, 0x312804, 0x312824, 0x312844, 0x312864,
        0x312884, 0x3128a4, 0x3128c4, 0x3128e4, 0x312904, 0x312924, 0x312944, 0x312964, 0x312984,
    };
    const char *dir = argc > 1 ? argv[1] : ".";
    int x = 1, y = 1, row_h = 0, failed, type, frame;
    unsigned i;

    /* Every frame of every object type, packed left to right. */
    memset(sheet, 255, sizeof sheet);
    for (type = 0; type < SI_TYPE_COUNT; type++) {
        int frames = SI_ROM(SI_TEMPLATES + 12ul * (unsigned)type)[0];

        if (!si_type_frames[type])
            continue;
        for (frame = 0; frame < (frames ? frames : 1); frame++) {
            struct sprite_image image;

            si_image(&image, si_type_frames[type] + 12ul * (unsigned)frame);
            if (x + image.w + 1 > SHEET_W) {
                x = 1;
                y += row_h + 2;
                row_h = 0;
            }
            blit(x, y, image.bitmap, image.w, image.h);
            x += image.w + 2;
            if (image.h > row_h)
                row_h = image.h;
        }
    }
    failed = write_sheet(dir, "sheet_sprites");

    memset(sheet, 255, sizeof sheet);
    for (i = 0; i < sizeof tiles / sizeof tiles[0]; i++)
        blit(1 + (int)(i % 7) * 34, 1 + (int)(i / 7) * 10, SI_ROM(tiles[i]), 32, 8);
    for (i = 0; i < 10; i++)
        blit(1 + (int)i * 6, 60, si_digit_glyphs + 4 * i, 4, 5);
    failed |= write_sheet(dir, "sheet_tiles");
    return failed;
}
