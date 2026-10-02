/* Runs a Game Boy ROM headlessly in SameBoy's core and saves its sound.

   Usage: gb_audio ROM.gb BOOT_ROM OUT.raw SECONDS

   Writes the left channel as raw signed 16-bit at 32768 Hz. Built against
   the library `make lib` makes in the SameBoy clone. */
#include <stdio.h>
#include <stdlib.h>

#include <Core/gb.h>

static FILE *out;
static uint32_t pixels[160 * 144];

static void on_sample(GB_gameboy_t *gb, GB_sample_t *sample)
{
    (void)gb;
    fwrite(&sample->left, sizeof sample->left, 1, out);
}

static uint32_t rgb(GB_gameboy_t *gb, uint8_t r, uint8_t g, uint8_t b)
{
    (void)gb;
    return (uint32_t)(r << 16 | g << 8 | b);
}

int main(int argc, char **argv)
{
    GB_gameboy_t *gb;
    long frames;

    if (argc != 5) {
        fprintf(stderr, "usage: gb_audio ROM.gb BOOT_ROM OUT.raw SECONDS\n");
        return 2;
    }
    gb = GB_init(GB_alloc(), GB_MODEL_DMG_B);
    if (GB_load_boot_rom(gb, argv[2]) || GB_load_rom(gb, argv[1])) {
        fprintf(stderr, "cannot load %s or %s\n", argv[2], argv[1]);
        return 1;
    }
    out = fopen(argv[3], "wb");
    if (!out) {
        perror(argv[3]);
        return 1;
    }
    GB_set_pixels_output(gb, pixels);
    GB_set_rgb_encode_callback(gb, rgb);
    GB_set_sample_rate(gb, 32768);
    GB_apu_set_sample_callback(gb, on_sample);
    for (frames = strtol(argv[4], NULL, 10) * 60; frames > 0; frames--)
        GB_run_frame(gb);
    fclose(out);
    return 0;
}
