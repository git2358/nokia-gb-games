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

static void test_rand(void)
{
    int i;

    /* seed 1: 1103527590, 2524885223, 662824084 -> bits 30..16 */
    game_srand(1);
    CHECK(game_rand() == 16838);
    CHECK(game_rand() == 5758);
    CHECK(game_rand() == 10113);

    /* The 3310 games' generator, from the power-on seed as seen in MAME. */
    game_rand16_seed = 1;
    CHECK(game_rand16() == 0x9882);

    /* Every seed against the generator's definition. */
    for (i = 0; i < 0x10000; i++) {
        game_rand16_seed = (uint16_t)i;
        if (game_rand16() != (uint16_t)((i * 0x625ful + 0x3623ul) % 0xfff1ul))
            break;
    }
    CHECK(i == 0x10000);
}

static int screen_pixel(int x, int y)
{
    return sprite_screen[x + LCD_WIDTH * (y >> 3)] >> (y & 7) & 1;
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
    sprite_create_fill(SPRITE_MODE_OPAQUE, 0, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    id = sprite_create(&image, SPRITE_MODE_XOR, 1, 10, 9);
    sprite_render();
    CHECK(screen_pixel(0, 0) == 1 && screen_pixel(LCD_WIDTH - 1, LCD_HEIGHT - 1) == 1);
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

/* The renderer works a byte at a time. This is the same picture made a
   pixel at a time, straight from the rules: each sprite in list order,
   its set and clear bits doing what its mode says, nothing drawn at or
   beyond the screen's last column or row, and nothing at all of a bitmap
   whose far edge has wrapped past 255. */
static uint8_t slow_screen[LCD_WIDTH * SPRITE_SCREEN_BANDS];

static void slow_put(unsigned x, unsigned y, int op)
{
    uint8_t *p = &slow_screen[x + LCD_WIDTH * (y >> 3)], bit = (uint8_t)(1 << (y & 7));

    if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
        return;
    if (op == 1)
        *p |= bit;
    else if (op == 2)
        *p &= (uint8_t)~bit;
    else if (op == 3)
        *p ^= bit;
}

static void slow_render(void)
{
    /* 0 nothing, 1 set, 2 clear, 3 flip: for set bits, for clear bits. */
    static const uint8_t ops[6][2] = { { 0, 2 }, { 1, 0 }, { 3, 0 }, { 2, 1 }, { 1, 2 }, { 1, 2 } };
    uint16_t id;

    memset(slow_screen, 0, sizeof slow_screen);
    for (id = sprites[0].next; id; id = sprites[id].next) {
        const struct sprite *s = &sprites[id];
        unsigned x, y;

        if (sprite_mode(s) >= SPRITE_MODE_HIDDEN)
            continue;
        if (sprite_kind(s) == SPRITE_FILL) {
            for (y = s->y; y < (unsigned)s->y + s->y2; y++)
                for (x = s->x; x < (unsigned)s->x + s->x2; x++)
                    slow_put(x, y, ops[sprite_mode(s)][0]);
        } else if (sprite_kind(s) == SPRITE_BITMAP) {
            unsigned x_end = (uint8_t)(s->x + s->image.w - 1), y_end = (uint8_t)(s->y + s->image.h - 1);

            for (x = s->x; x <= x_end; x++) {
                for (y = s->y; y <= y_end; y++) {
                    unsigned row = y - s->y;
                    uint8_t bits = s->image.bitmap[(x - s->x) + s->image.w * (row >> 3)];

                    slow_put(x, y, ops[sprite_mode(s)][(bits >> (row & 7)) & 1 ? 0 : 1]);
                }
            }
        }
    }
}

static void test_render_matches_pixels(void)
{
    static uint8_t noise[40 * 5];
    unsigned trial, i;

    game_srand(7);
    for (i = 0; i < sizeof noise; i++)
        noise[i] = (uint8_t)game_rand();
    for (trial = 0; trial < 4000; trial++) {
        unsigned count = 1 + (unsigned)game_rand() % 6;

        sprite_reset_all();
        for (i = 0; i < count; i++) {
            /* Sizes up to the last boss's 38x38, places all over the byte
               range, with the edges and the wrap-around more likely. */
            static const uint8_t edges[] = { 0, 1, 7, 8, 40, 56, 63, 64, 65, 88, 95, 96, 200, 250, 255 };
            struct sprite_image image;
            unsigned x = (unsigned)game_rand(), y = (unsigned)game_rand();
            uint8_t mode = (uint8_t)(game_rand() % 7);

            x = x & 0x100 ? edges[x % sizeof edges] : x & 0xff;
            y = y & 0x100 ? edges[y % sizeof edges] : y & 0xff;
            image.w = (uint8_t)(1 + game_rand() % 40);
            image.h = (uint8_t)(1 + game_rand() % 40);
            image.bitmap = noise;
            if (game_rand() % 8 == 0)
                sprite_create_fill(mode, 0, (int)(x % 100), (int)(y % 70), 1 + game_rand() % 100, 1 + game_rand() % 70);
            else
                sprite_create(&image, mode, (uint8_t)(game_rand() % 3), (int)x, (int)y);
        }
        sprite_render();
        slow_render();
        if (memcmp(sprite_screen, slow_screen, sizeof slow_screen) != 0) {
            CHECK(!"the renderer and the pixel-by-pixel picture differ");
            printf("  trial %u\n", trial);
            return;
        }
    }
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
    test_render_matches_pixels();
    test_present();
    if (failures) {
        printf("%d checks failed\n", failures);
        return 1;
    }
    printf("core checks passed\n");
    return 0;
}
