/* Host checks for the core services. Needs no firmware. */
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "rand.h"
#include "sprite.h"

static int failures;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

/* The sprite layer asks the game for tile bitmaps; none are used here. */
const uint8_t *tilemap_tile(uint32_t address)
{
    (void)address;
    return 0;
}

static void test_rand(void)
{
    int i;

    /* seed 1: 1103527590, 2524885223, 662824084 -> bits 30..16 */
    game_srand(1);
    CHECK(game_rand() == 16838);
    CHECK(game_rand() == 5758);
    CHECK(game_rand() == 10113);

    /* The games' generator: the first value from the power-on seed, and
       the seed the phone's Space Impact title animation leaves after its
       60 draws, both as seen in MAME. */
    game_rand16_seed = 1;
    CHECK(game_rand16() == 0x9882);
    for (i = 1; i < 60; i++)
        game_rand16();
    CHECK(game_rand16_seed == 0xa335);
}

static int screen_pixel(int x, int y)
{
    return sprite_screen[x + 84 * (y >> 3)] >> (y & 7) & 1;
}

static void test_sprite_order(void)
{
    static const uint8_t dot[] = { 1 };
    struct sprite_image image = { dot, 1, 1 };
    uint16_t a, b, c, d;

    sprite_reset_all();
    a = sprite_create(&image, SPRITE_MODE_SET, 2, 0, 0);
    b = sprite_create(&image, SPRITE_MODE_SET, 0, 0, 0);
    c = sprite_create(&image, SPRITE_MODE_SET, 2, 0, 0);
    d = sprite_create(&image, SPRITE_MODE_SET, 1, 0, 0);
    /* Ids come in order; the list is by layer, then by age. */
    CHECK(a == 1 && b == 2 && c == 3 && d == 4);
    CHECK(sprites[0].next == b);
    CHECK(sprites[b].next == d);
    CHECK(sprites[d].next == a);
    CHECK(sprites[a].next == c);
    CHECK(sprites[c].next == 0);
    /* A freed id is the next one handed out. */
    CHECK(sprite_free(a) == c);
    CHECK(sprites[d].next == c);
    CHECK(sprite_create(&image, SPRITE_MODE_SET, 0, 0, 0) == a);
    CHECK(sprites[b].next == a);
}

static void test_sprite_modes(void)
{
    /* 2 columns, 2 rows: set, clear / clear, set. */
    static const uint8_t check[] = { 1, 2 };
    struct sprite_image image = { check, 2, 2 };
    uint16_t id;

    sprite_reset_all();
    sprite_create_fill(SPRITE_MODE_OPAQUE, 0, 0, 0, 84, 48);
    id = sprite_create(&image, SPRITE_MODE_XOR, 1, 10, 9);
    sprite_render();
    CHECK(screen_pixel(0, 0) == 1 && screen_pixel(83, 47) == 1);
    CHECK(screen_pixel(10, 9) == 0 && screen_pixel(11, 9) == 1);
    CHECK(screen_pixel(10, 10) == 1 && screen_pixel(11, 10) == 0);

    sprite_set_mode(id, SPRITE_MODE_INVERSE);
    sprite_render();
    CHECK(screen_pixel(10, 9) == 0 && screen_pixel(11, 9) == 1);

    sprite_set_mode(id, SPRITE_MODE_HIDDEN);
    sprite_render();
    CHECK(screen_pixel(10, 9) == 1 && screen_pixel(11, 10) == 1);

    /* Part-way off the right edge it is clipped; past the left edge, where
       x has wrapped, it is not drawn at all. */
    sprite_set_mode(id, SPRITE_MODE_XOR);
    sprite_move(id, 83, 0);
    sprite_render();
    CHECK(screen_pixel(83, 0) == 0 && screen_pixel(0, 0) == 1 && screen_pixel(0, 1) == 1);
    sprite_move(id, -1, 0);
    sprite_render();
    CHECK(screen_pixel(0, 0) == 1 && screen_pixel(0, 1) == 1);

    /* A vertical line, as the beam is. */
    sprite_reset_all();
    sprite_create_line(SPRITE_MODE_SET, 2, 20, 6, 20, 32);
    sprite_render();
    CHECK(screen_pixel(20, 5) == 0 && screen_pixel(20, 6) == 1 && screen_pixel(20, 32) == 1 && screen_pixel(20, 33) == 0);
    CHECK(screen_pixel(19, 10) == 0 && screen_pixel(21, 10) == 0);
}

static void test_present(void)
{
    static const uint8_t dot[] = { 1 };
    struct sprite_image image = { dot, 1, 1 };
    int x, y, set = 0;

    sprite_reset_all();
    sprite_create(&image, SPRITE_MODE_SET, 0, 5, 7);
    sprite_render();
    sprite_present(1);
    for (y = 0; y < LCD_HEIGHT; y++)
        for (x = 0; x < LCD_WIDTH; x++)
            set += lcd_pixel(x, y);
    CHECK(set == 1 && lcd_pixel(5, 7) == 1);
}

int main(void)
{
    test_rand();
    test_sprite_order();
    test_sprite_modes();
    test_present();
    if (failures) {
        printf("%d checks failed\n", failures);
        return 1;
    }
    printf("core checks passed\n");
    return 0;
}
