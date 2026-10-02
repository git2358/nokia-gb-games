/* Game Boy layer. The core's framebuffer is the whole 160x144 screen, shown
   as a 20x18 block of background tiles, one tile per 8x8 cell; the phone's
   84x48 LCD is a window in the middle of it. In the menus A, Select or
   Start is the phone's Navi key, B is its C key and the D-pad scrolls;
   Start on the first screen picks the full-screen variant, which shows the
   game at 2x. In the game the D-pad moves the ship, A fires, B uses the
   special weapon and Start or Select pauses.

   This file is in the ROM's first 16 KiB with the rest of what every part
   of the program needs; far.c says what is in the other banks. */
#include <stdint.h>

#include <string.h>

#include "far.h"
#include "game.h"
#include "game_assets.h"
#include "lcd.h"
#include "menu.h"
#include "si.h"
#include "si_data.h"
#include "strip.h"

#define REG(addr) (*(volatile uint8_t *)(addr))
#define P1 REG(0xff00)
#define IF REG(0xff0f)
#define IE REG(0xffff)
#define NR52 REG(0xff26)
#define LCDC REG(0xff40)
#define STAT REG(0xff41)
#define SCY REG(0xff42)
#define SCX REG(0xff43)
#define LY REG(0xff44)
#define LYC REG(0xff45)
#define BGP REG(0xff47)

/* Both bit planes of a tile are written alike for the menus, and only the
   first for the game (see draw.s), so every colour but 0 is shown dark. */
#define PALETTE 0xfc

#define VRAM_MAP ((uint8_t *)0x9800)

#define TILES_X LCD_CELLS_X /* 20 */
#define TILES_Y LCD_CELLS_Y /* 18 */

/* 360 tiles are more than the 256 a tile number can name. The first 240
   (12 rows) are at 0x8000, the rest at 0x9000, and the handlers in crt0.s
   switch the LCD between the two tile areas at this line of every frame. */
#define SPLIT_ROW 12
#define SPLIT_TILE (SPLIT_ROW * TILES_X)
#define SPLIT_LINE (SPLIT_ROW * 8)

#define LCDC_ON 0x91 /* LCD on, tile data at 0x8000, background on */

#define PAD_A 0x01
#define PAD_B 0x02
#define PAD_SELECT 0x04
#define PAD_START 0x08
#define PAD_RIGHT 0x10
#define PAD_LEFT 0x20
#define PAD_UP 0x40
#define PAD_DOWN 0x80

/* Keys pressed at power-on, for scripted screenshots; see menu_script. */
#ifndef START_KEYS
#define START_KEYS ""
#endif

/* Cartridge RAM (MBC1, 8 KiB, battery-backed), which stays enabled. It
   starts with the settings: a two-byte signature, then one four-byte record
   per game laid out as the phone stores them: top score high byte, low
   byte, level, and a check byte. From SI_DATA_AT on it holds Space Impact's
   data, copied there from bank 3 at every power-on: that is 7 KiB the game
   and the sprite code both read, more than the always-mapped part of the
   ROM or work RAM has room for. */
#define MBC_RAM_ENABLE REG(0x0000)
#define SAVE ((uint8_t *)0xa000)
#define SAVE_SIGNATURE_0 'N'
#define SAVE_SIGNATURE_1 '3'
#define SAVE_CHECK(r) ((uint8_t)((r)[0] + (r)[1] + (r)[2] + 0x5a))

/* Tiles converted at a time before being copied to video RAM. */
#define STAGED_TILES 4
/* Most frames of game time made up at once after a slow draw. */
#define MAX_CATCH_UP 18

/* Frames since power-on, counted by the vertical-blank handler in crt0.s. */
volatile uint8_t frame_count;

/* Tile data on its way to video RAM, and where it goes; flush_tiles in
   crt0.s copies it between the lines the LCD is drawing. */
uint8_t staged[STAGED_TILES * 16];
uint8_t *staged_at[STAGED_TILES];
uint8_t staged_count;
const uint8_t *staged_from; /* flush_tiles' place in staged */

void flush_tiles(void);

/* The game's sounds are not written yet. */
void platform_sound(uint8_t sound)
{
    (void)sound;
}

void platform_vibrate(void)
{
}

uint8_t platform_settings_load(uint8_t game, struct game_settings *out)
{
    const uint8_t *record = SAVE + 2 + game * 4;

    out->top_score = 0;
    out->level = 0;
    if (SAVE[0] == SAVE_SIGNATURE_0 && SAVE[1] == SAVE_SIGNATURE_1 && record[3] == SAVE_CHECK(record)) {
        out->top_score = (uint16_t)(record[0] << 8 | record[1]);
        out->level = record[2];
        return 1;
    }
    return 0;
}

void platform_settings_save(uint8_t game, const struct game_settings *in)
{
    uint8_t *record = SAVE + 2 + game * 4;

    SAVE[0] = SAVE_SIGNATURE_0;
    SAVE[1] = SAVE_SIGNATURE_1;
    record[0] = (uint8_t)(in->top_score >> 8);
    record[1] = (uint8_t)in->top_score;
    record[2] = in->level;
    record[3] = SAVE_CHECK(record);
}

/* Used once, at power-on, before interrupts are enabled. */
static void wait_vblank(void)
{
    while (LY != 144)
        ;
}

/* Converts one 8x8 cell of lcd_fb to tile data: a set pixel is colour 3, a
   clear one 0. A framebuffer row is 20 bytes, one per tile across, so each
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

/* Where each row of tiles starts in video RAM. */
static uint8_t *tile_row[TILES_Y];

static uint8_t *tile_address(uint8_t tx, uint8_t ty)
{
    return tile_row[ty] + (uint16_t)(tx << 4);
}

static void stage_tile(uint8_t tx, uint8_t ty, uint8_t *n)
{
    staged_at[*n] = tile_address(tx, ty);
    if (++*n == STAGED_TILES) {
        staged_count = *n;
        flush_tiles();
        *n = 0;
    }
}

/* The game is not drawn through lcd_fb: the tiles are made straight from
   the sprite layer's picture, at 2x when the core has named a rectangle to
   magnify (the full-screen variant; see strip.c) and as it is otherwise
   (draw.s). `direct` says which of the two the screen's tiles hold, 0 for
   neither; game_shown that the last menu_draw drew the game. */
enum {
    DIRECT_PLAIN = 1,
    DIRECT_ZOOM
};
static uint8_t direct, game_shown;
extern uint8_t gb_present_all;
void gb_present_plain(void);
void gb_clear_tiles(void);

void sprite_present(uint8_t all)
{
    uint8_t mode = lcd_zoom_w ? DIRECT_ZOOM : DIRECT_PLAIN;

    if (direct != mode)
        all = 1;
    if (mode == DIRECT_ZOOM) {
        strip_present(all);
    } else {
        if (all) {
            /* The tiles hold a menu, or nothing: clear them all and make
               the game's afresh. The picture is made 2 pixels right of the
               middle, on whole tiles, and scrolled back. */
            strip_leave(2, direct == DIRECT_ZOOM);
            gb_clear_tiles();
            gb_present_all = 1;
        }
        gb_present_plain();
    }
    direct = mode;
    game_shown = 1;
}

/* Brings video RAM up to date with the cells of lcd_fb drawn to since the
   last call. The LCD stays on: the tiles go in a few at a time between the
   lines being drawn, so a changed screen fills in over a few frames instead
   of blinking. */
static void present(void)
{
    uint8_t tx, ty, n = 0;
    uint8_t *dirty = lcd_dirty;

    for (ty = 0; ty < TILES_Y; ty++) {
        for (tx = 0; tx < TILES_X; tx++, dirty++) {
            if (!*dirty)
                continue;
            *dirty = 0;
            if (!(LCDC & 0x80)) {
                /* Only at power-on: nothing is being drawn yet. */
                render_tile(tx, ty, tile_address(tx, ty));
                continue;
            }
            render_tile(tx, ty, staged + n * 16);
            staged_at[n] = tile_address(tx, ty);
            if (++n == STAGED_TILES) {
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

/* Puts what menu_draw drew on the screen. The game has done that itself;
   a menu after the game needs every tile made again. */
static void show(void)
{
    uint16_t i;

    if (game_shown) {
        game_shown = 0;
        return;
    }
    if (direct) {
        /* Blank first, so that the game's tiles are not seen through the
           menus' tile map while the menu's are being made. */
        gb_clear_tiles();
        strip_leave(0, direct == DIRECT_ZOOM);
        direct = 0;
        for (i = 0; i < sizeof lcd_dirty; i++)
            lcd_dirty[i] = 1;
    }
    present();
}

/* The pad is read once a frame by the vertical-blank handler in crt0.s:
   pad_last is what was held then (A, B, Select, Start in bits 0-3, Right,
   Left, Up, Down in bits 4-7) and pad_latch collects every new press until
   the main loop takes it. */
volatile uint8_t pad_last, pad_latch;

static uint8_t take_presses(void)
{
    uint8_t pressed;

    __asm__("di");
    pressed = pad_latch;
    pad_latch = 0;
    __asm__("ei");
    return pressed;
}

static uint8_t press(uint8_t key)
{
    return menu_key(key);
}

/* The buttons down, as menu_held wants them. */
static uint8_t held_keys(void)
{
    uint8_t pad = pad_last, keys = 0;

    if (pad & PAD_UP)
        keys |= 1 << MENU_KEY_UP;
    if (pad & PAD_DOWN)
        keys |= 1 << MENU_KEY_DOWN;
    if (pad & PAD_LEFT)
        keys |= 1 << MENU_KEY_LEFT;
    if (pad & PAD_RIGHT)
        keys |= 1 << MENU_KEY_RIGHT;
    if (pad & PAD_A)
        keys |= 1 << MENU_KEY_SELECT;
    if (pad & PAD_B)
        keys |= 1 << MENU_KEY_BACK;
    return keys;
}

void main(void)
{
    uint8_t tx, ty, pressed, changed, seen = 0;
    uint16_t i;

    wait_vblank();
    LCDC = 0;
    for (ty = 0; ty < TILES_Y; ty++)
        tile_row[ty] = ty < SPLIT_ROW ? (uint8_t *)0x8000 + ty * (TILES_X * 16)
                                      : (uint8_t *)0x9000 + (ty - SPLIT_ROW) * (TILES_X * 16);
    /* Every cell of the screen gets its own tile. */
    for (ty = 0; ty < TILES_Y; ty++)
        for (tx = 0; tx < TILES_X; tx++)
            VRAM_MAP[ty * 32 + tx] = (uint8_t)((ty * TILES_X + tx) % SPLIT_TILE);
    SCX = 0;
    SCY = 0;
    BGP = PALETTE;
    strip_leave(0, 0);
    STAT = 0x40; /* interrupt when LY reaches LYC: the cuts in crt0.s */

    /* Video RAM holds whatever the boot ROM left: write every tile once. */
    for (i = 0; i < sizeof lcd_dirty; i++)
        lcd_dirty[i] = 1;

    NR52 = 0x00; /* sound off */

    MBC_RAM_ENABLE = 0x0a;
    far_bank(BANK_SETUP);
    memcpy((uint8_t *)SI_DATA_AT, si_data, SI_DATA_SIZE);
    far_bank(BANK_MENU);

    menu_init();
    menu_script(START_KEYS);
    menu_draw();
    show();
    LCDC = LCDC_ON;

    IF = 0;
    IE = 0x03; /* vertical blank and LCD status */
    __asm__("ei");

    for (;;) {
        uint8_t frames;

        while (frame_count == seen)
            ;

        /* Keys first, so a press takes effect before the game's next move. */
        pressed = take_presses();
        changed = 0;
        if (pressed & PAD_A)
            changed |= press(MENU_KEY_SELECT);
        if (pressed & PAD_SELECT)
            changed |= press(MENU_KEY_ALT);
        if (pressed & PAD_START)
            changed |= press(MENU_KEY_START);
        if (pressed & PAD_B)
            changed |= press(MENU_KEY_BACK);
        if (pressed & PAD_UP)
            changed |= press(MENU_KEY_UP);
        if (pressed & PAD_DOWN)
            changed |= press(MENU_KEY_DOWN);
        if (pressed & PAD_LEFT)
            changed |= press(MENU_KEY_LEFT);
        if (pressed & PAD_RIGHT)
            changed |= press(MENU_KEY_RIGHT);
        changed |= menu_held(held_keys());
        /* Draw only when a key changed something, not on every press. The
           time that takes is not game time: a new game's clock starts once
           its board is on the screen. */
        if (changed) {
            menu_draw();
            show();
            seen = frame_count;
        }

        /* One menu tick per frame that has passed, then one picture. The
           game takes several frames to put on the screen when much of it
           changes; its ticks are then all made up before the next picture,
           so that it keeps the phone's pace and shows fewer pictures. Never
           more than MAX_CATCH_UP frames' worth, or a long draw would be
           followed by a visible jump. */
        frames = (uint8_t)(frame_count - seen);
        seen += frames;
        if (frames > MAX_CATCH_UP)
            frames = MAX_CATCH_UP;
        changed = 0;
        while (frames--)
            changed |= menu_tick();
        if (changed) {
            menu_draw();
            show();
        }
    }
}
