/* Game Boy layer. The 84x48 LCD is drawn 1:1 as an 11x6 block of background
   tiles, centred on the screen. Start or A is the phone's Navi key, B is its
   C key, and Up/Down scroll. */
#include <stdint.h>

#include "lcd.h"
#include "menu.h"

#define REG(addr) (*(volatile uint8_t *)(addr))
#define P1 REG(0xff00)
#define LCDC REG(0xff40)
#define SCY REG(0xff42)
#define SCX REG(0xff43)
#define LY REG(0xff44)
#define BGP REG(0xff47)

#define VRAM_TILES ((uint8_t *)0x8000)
#define VRAM_MAP ((uint8_t *)0x9800)

#define TILES_X 11 /* 88 px, holds the 84 px LCD */
#define TILES_Y 6
#define MAP_X 4
#define MAP_Y 6
/* Map column 4 starts at x = 32; scrolling by -6 puts the LCD at x = 38. */
#define SCROLL_X ((uint8_t)-6)

#define LCDC_ON 0x91 /* LCD on, tile data at 0x8000, background on */

#define PAD_A 0x01
#define PAD_B 0x02
#define PAD_START 0x08
#define PAD_UP 0x40
#define PAD_DOWN 0x80

/* Menu keys pressed at power-on, for scripted screenshots: u, d, s, b. */
#ifndef START_KEYS
#define START_KEYS ""
#endif

static uint8_t tiles[TILES_X * TILES_Y * 16];

static void wait_vblank(void)
{
    while (LY != 144)
        ;
}

/* Converts lcd_fb to tile data: a set pixel is colour 3, a clear one 0. */
static void render_tiles(void)
{
    uint8_t *tile = tiles;
    uint8_t tx, ty, row, bit, bits;
    const uint8_t *src;

    for (ty = 0; ty < TILES_Y; ty++) {
        for (tx = 0; tx < TILES_X; tx++) {
            for (row = 0; row < 8; row++) {
                bits = 0;
                src = lcd_fb + (ty * 8 + row) * LCD_WIDTH + tx * 8;
                for (bit = 0; bit < 8; bit++) {
                    bits <<= 1;
                    if (tx * 8 + bit < LCD_WIDTH && src[bit])
                        bits |= 1;
                }
                *tile++ = bits;
                *tile++ = bits;
            }
        }
    }
}

/* Copies the tile data to video RAM with the LCD off, so it is never
   written while the LCD controller owns it. */
static void present(void)
{
    uint8_t *dst = VRAM_TILES + 16; /* tile 0 stays blank */
    const uint8_t *src = tiles;
    uint16_t n;

    render_tiles();
    if (LCDC & 0x80) {
        wait_vblank();
        LCDC = 0;
    }
    for (n = sizeof tiles; n; n--)
        *dst++ = *src++;
    LCDC = LCDC_ON;
}

/* Buttons currently held: A, B, Select, Start in bits 0-3, then Right,
   Left, Up, Down in bits 4-7. */
static uint8_t read_pad(void)
{
    uint8_t pad;

    P1 = 0x10; /* select the button keys */
    pad = P1;
    pad = (uint8_t)(~P1 & 0x0f);
    P1 = 0x20; /* select the direction keys */
    pad |= P1 & 0;
    pad |= (uint8_t)((~P1 & 0x0f) << 4);
    P1 = 0x30;
    return pad;
}

static void press(uint8_t key)
{
    menu_key(key);
}

void main(void)
{
    uint16_t i;
    uint8_t tx, ty, pad, last = 0, pressed;
    const char *key;

    wait_vblank();
    LCDC = 0;
    for (i = 0; i < 16; i++)
        VRAM_TILES[i] = 0;
    for (i = 0; i < 32 * 32; i++)
        VRAM_MAP[i] = 0;
    for (ty = 0; ty < TILES_Y; ty++)
        for (tx = 0; tx < TILES_X; tx++)
            VRAM_MAP[(MAP_Y + ty) * 32 + MAP_X + tx] = (uint8_t)(1 + ty * TILES_X + tx);
    SCX = SCROLL_X;
    SCY = 0;
    BGP = 0xe4;

    menu_init();
    for (key = START_KEYS; *key; key++)
        press(*key == 'u' ? MENU_KEY_UP : *key == 'd' ? MENU_KEY_DOWN : *key == 's' ? MENU_KEY_SELECT : MENU_KEY_BACK);
    menu_draw();
    present();

    for (;;) {
        wait_vblank();
        pad = read_pad();
        pressed = (uint8_t)(pad & ~last);
        last = pad;
        if (!pressed) {
            /* Leave line 144 so the next wait is a new frame. */
            while (LY == 144)
                ;
            continue;
        }
        if (pressed & (PAD_START | PAD_A))
            press(MENU_KEY_SELECT);
        else if (pressed & PAD_B)
            press(MENU_KEY_BACK);
        else if (pressed & PAD_UP)
            press(MENU_KEY_UP);
        else if (pressed & PAD_DOWN)
            press(MENU_KEY_DOWN);
        menu_draw();
        present();
    }
}
