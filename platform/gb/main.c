/* Game Boy layer. The 84x48 LCD is drawn 1:1 as an 11x6 block of background
   tiles, centred on the screen. Start or A is the phone's Navi key, B is its
   C key, and Up/Down scroll. */
#include <stdint.h>

#include "game.h"
#include "lcd.h"
#include "menu.h"

#define REG(addr) (*(volatile uint8_t *)(addr))
#define P1 REG(0xff00)
#define IF REG(0xff0f)
#define IE REG(0xffff)
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
#define PAD_RIGHT 0x10
#define PAD_LEFT 0x20
#define PAD_UP 0x40
#define PAD_DOWN 0x80

/* Keys pressed at power-on, for scripted screenshots: u, d, l, r, s select,
   b back, t one move of the running game, and p to start over as after a
   power cycle (settings are read back). */
#ifndef START_KEYS
#define START_KEYS ""
#endif

/* Battery-backed cartridge RAM (MBC1): a two-byte signature, then one
   four-byte record per game laid out as the phone stores them: top score
   high byte, low byte, level, and a check byte. */
#define MBC_RAM_ENABLE REG(0x0000)
#define SAVE ((uint8_t *)0xa000)
#define SAVE_SIGNATURE_0 'N'
#define SAVE_SIGNATURE_1 '3'
#define SAVE_CHECK(r) ((uint8_t)((r)[0] + (r)[1] + (r)[2] + 0x5a))

/* Tiles copied to video RAM in one vertical blank with the LCD on. */
#define TILES_PER_BLANK 4
/* With more changed tiles than this the LCD is switched off for the copy. */
#define MAX_LIVE_TILES 16

/* Frames since power-on, counted by the vertical-blank handler in crt0.s. */
volatile uint8_t frame_count;

/* Tile data waiting for the next vertical blank, and where it goes;
   flush_tiles in crt0.s copies it. */
uint8_t staged[TILES_PER_BLANK * 16];
uint8_t *staged_at[TILES_PER_BLANK];
uint8_t staged_count;

void flush_tiles(void);

void platform_beep(void)
{
}

void platform_settings_load(uint8_t game, struct game_settings *out)
{
    const uint8_t *record = SAVE + 2 + game * 4;

    out->top_score = 0;
    out->level = 0;
    MBC_RAM_ENABLE = 0x0a;
    if (SAVE[0] == SAVE_SIGNATURE_0 && SAVE[1] == SAVE_SIGNATURE_1 && record[3] == SAVE_CHECK(record)) {
        out->top_score = (uint16_t)(record[0] << 8 | record[1]);
        out->level = record[2];
    }
    MBC_RAM_ENABLE = 0x00;
}

void platform_settings_save(uint8_t game, const struct game_settings *in)
{
    uint8_t *record = SAVE + 2 + game * 4;

    MBC_RAM_ENABLE = 0x0a;
    SAVE[0] = SAVE_SIGNATURE_0;
    SAVE[1] = SAVE_SIGNATURE_1;
    record[0] = (uint8_t)(in->top_score >> 8);
    record[1] = (uint8_t)in->top_score;
    record[2] = in->level;
    record[3] = SAVE_CHECK(record);
    MBC_RAM_ENABLE = 0x00;
}

static void wait_vblank(void)
{
    while (LY != 144)
        ;
}

/* Converts one 8x8 cell of lcd_fb to tile data: a set pixel is colour 3, a
   clear one 0. A framebuffer row is 11 bytes, one per tile across, so each
   tile row is one framebuffer byte written to both bit planes. */
static void render_tile(uint8_t tx, uint8_t ty, uint8_t *tile)
{
    const uint8_t *src = lcd_fb + ty * (8 * LCD_STRIDE) + tx;
    uint8_t row;

    for (row = 8; row; row--, src += LCD_STRIDE) {
        *tile++ = *src;
        *tile++ = *src;
    }
}

static uint8_t *tile_address(uint8_t tx, uint8_t ty)
{
    return VRAM_TILES + 16 + (uint16_t)(ty * TILES_X + tx) * 16; /* tile 0 stays blank */
}

/* Brings video RAM up to date with the cells of lcd_fb drawn to since the
   last call. Video RAM can only be written while the LCD controller is not
   using it: a few tiles are copied during vertical blanks, and a whole new
   screen with the LCD off. */
static void present(void)
{
    uint8_t tx, ty, count = 0, n = 0;
    uint16_t mask;

    for (ty = 0; ty < TILES_Y; ty++)
        for (mask = lcd_dirty[ty]; mask; mask >>= 1)
            count += mask & 1;
    if (!count && (LCDC & 0x80))
        return;

    if (count > MAX_LIVE_TILES || !(LCDC & 0x80)) {
        if (LCDC & 0x80) {
            wait_vblank();
            LCDC = 0;
        }
        for (ty = 0; ty < TILES_Y; ty++)
            for (tx = 0, mask = lcd_dirty[ty]; tx < TILES_X; tx++, mask >>= 1)
                if (mask & 1)
                    render_tile(tx, ty, tile_address(tx, ty));
        LCDC = LCDC_ON;
    } else {
        for (ty = 0; ty < TILES_Y; ty++) {
            for (tx = 0, mask = lcd_dirty[ty]; tx < TILES_X; tx++, mask >>= 1) {
                if (!(mask & 1))
                    continue;
                render_tile(tx, ty, staged + n * 16);
                staged_at[n] = tile_address(tx, ty);
                if (++n == TILES_PER_BLANK) {
                    staged_count = n;
                    flush_tiles();
                    n = 0;
                }
            }
        }
        if (n) {
            staged_count = n;
            flush_tiles();
        }
    }
    for (ty = 0; ty < TILES_Y; ty++)
        lcd_dirty[ty] = 0;
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
    uint8_t tx, ty, pad, last = 0, pressed, seen = 0;
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
    for (key = START_KEYS; *key; key++) {
        if (*key == 'p')
            menu_init();
        else if (*key == 't')
            menu_game_step();
        else
            press(*key == 'u' ? MENU_KEY_UP : *key == 'd' ? MENU_KEY_DOWN : *key == 'l' ? MENU_KEY_LEFT
                  : *key == 'r' ? MENU_KEY_RIGHT : *key == 's' ? MENU_KEY_SELECT : MENU_KEY_BACK);
    }
    menu_draw();
    present();

    IF = 0;
    IE = 0x01; /* vertical blank */
    __asm__("ei");

    for (;;) {
        uint8_t frames;

        /* One menu tick per frame, catching up on frames spent drawing. */
        while (frame_count == seen)
            ;
        frames = (uint8_t)(frame_count - seen);
        seen += frames;
        while (frames--) {
            if (menu_tick()) {
                menu_draw();
                present();
            }
        }

        pad = read_pad();
        pressed = (uint8_t)(pad & ~last);
        last = pad;
        if (pressed & (PAD_START | PAD_A))
            press(MENU_KEY_SELECT);
        else if (pressed & PAD_B)
            press(MENU_KEY_BACK);
        else if (pressed & PAD_UP)
            press(MENU_KEY_UP);
        else if (pressed & PAD_DOWN)
            press(MENU_KEY_DOWN);
        else if (pressed & PAD_LEFT)
            press(MENU_KEY_LEFT);
        else if (pressed & PAD_RIGHT)
            press(MENU_KEY_RIGHT);
        if (pressed) {
            menu_draw();
            present();
        }
    }
}
