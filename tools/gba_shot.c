/* Runs a GBA ROM headlessly in mGBA's core and saves the screen.

   Usage: gba_shot ROM.gba OUT.bmp FRAMES [KEYS FROM TO]

   KEYS is a mask of buttons held from frame FROM up to frame TO (bit 0 A,
   1 B, 2 Select, 3 Start, 4 Right, 5 Left, 6 Up, 7 Down).

   The picture is a 32-bit BMP in the layout SameBoy's tester writes, so the
   same tools read both. Built against the library scripts/setup-mgba.sh
   makes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>

static void quiet(struct mLogger *logger, int category, enum mLogLevel level, const char *format, va_list args)
{
    (void)logger;
    (void)category;
    (void)level;
    (void)format;
    (void)args;
}

static void put32(FILE *f, unsigned long v)
{
    fputc(v & 0xff, f);
    fputc(v >> 8 & 0xff, f);
    fputc(v >> 16 & 0xff, f);
    fputc(v >> 24 & 0xff, f);
}

int main(int argc, char **argv)
{
    static struct mLogger logger = { .log = quiet };
    struct mCore *core;
    unsigned width, height, x, y;
    color_t *pixels;
    long frames, frame, keys = 0, from = 0, to = 0;
    FILE *out;

    if (argc != 4 && argc != 7) {
        fprintf(stderr, "usage: gba_shot ROM.gba OUT.bmp FRAMES [KEYS FROM TO]\n");
        return 2;
    }
    frames = strtol(argv[3], NULL, 10);
    if (argc == 7) {
        keys = strtol(argv[4], NULL, 0);
        from = strtol(argv[5], NULL, 10);
        to = strtol(argv[6], NULL, 10);
    }
    mLogSetDefaultLogger(&logger);

    core = mCoreFind(argv[1]);
    if (!core || !core->init(core)) {
        fprintf(stderr, "%s: not a ROM mGBA can run\n", argv[1]);
        return 1;
    }
    mCoreInitConfig(core, NULL);
    core->desiredVideoDimensions(core, &width, &height);
    pixels = calloc(width * height, sizeof *pixels);
    core->setVideoBuffer(core, pixels, width);
    if (!mCoreLoadFile(core, argv[1])) {
        fprintf(stderr, "%s: cannot load\n", argv[1]);
        return 1;
    }
    core->reset(core);
    for (frame = 0; frame < frames; frame++) {
        core->setKeys(core, frame >= from && frame < to ? (uint32_t)keys : 0);
        core->runFrame(core);
    }

    out = fopen(argv[2], "wb");
    if (!out) {
        perror(argv[2]);
        return 1;
    }
    /* File header, then a 56-byte info header with channel masks. */
    fputc('B', out);
    fputc('M', out);
    put32(out, 70 + width * height * 4);
    put32(out, 0);
    put32(out, 70);
    put32(out, 56);
    put32(out, width);
    put32(out, (unsigned long)-(long)height); /* top-down */
    fputc(1, out);
    fputc(0, out);
    fputc(32, out);
    fputc(0, out);
    put32(out, 3); /* BI_BITFIELDS */
    put32(out, width * height * 4);
    put32(out, 2835);
    put32(out, 2835);
    put32(out, 0);
    put32(out, 0);
    put32(out, 0x000000ff); /* red: mGBA's pixels are 0x00BBGGRR */
    put32(out, 0x0000ff00);
    put32(out, 0x00ff0000);
    put32(out, 0);
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            put32(out, pixels[y * width + x] & 0xffffff);
    fclose(out);
    core->deinit(core);
    return 0;
}
