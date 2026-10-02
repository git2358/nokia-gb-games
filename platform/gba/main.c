/* GBA layer. The core's framebuffer is the whole 240x160 screen, shown in
   bitmap mode 3. The core names a part of it to magnify: the phone's 84x48
   LCD in the phone-sized mode and the board in the full-screen one are
   shown at 2x in the middle of the screen; the full-screen menus are shown
   as they are. A is the phone's Navi key, B is its C key, the D-pad scrolls
   and steers, and Start on the first screen picks the full-screen mode. */
#include <stdint.h>

#include "game.h"
#include "lcd.h"
#include "menu.h"
#include "sound.h"

#define REG16(addr) (*(volatile uint16_t *)(addr))
#define REG_DISPCNT REG16(0x04000000)
#define REG_DISPSTAT REG16(0x04000004)
#define REG_SOUND2CNT_L REG16(0x04000068)
#define REG_SOUND2CNT_H REG16(0x0400006c)
#define REG_SOUNDCNT_L REG16(0x04000080)
#define REG_SOUNDCNT_H REG16(0x04000082)
#define REG_SOUNDCNT_X REG16(0x04000084)
#define REG_KEYINPUT REG16(0x04000130)
#define REG_IE REG16(0x04000200)
#define REG_WAITCNT REG16(0x04000204)
#define REG_IME REG16(0x04000208)
#define IRQ_VECTOR (*(void (*volatile *)(void))0x03007ffc)
#define VRAM ((uint16_t *)0x06000000)

#define SCREEN_W LCD_FB_WIDTH
#define SCREEN_H LCD_FB_HEIGHT
/* Most frames of game time made up at once after a slow draw. */
#define MAX_CATCH_UP 6

#define RGB(r, g, b) ((uint16_t)((r) | (g) << 5 | (b) << 10))
#define COLOR_CLEAR RGB(19, 24, 15)
#define COLOR_SET RGB(4, 6, 3)

#define PAD_A 0x001
#define PAD_B 0x002
#define PAD_SELECT 0x004
#define PAD_START 0x008
#define PAD_RIGHT 0x010
#define PAD_LEFT 0x020
#define PAD_UP 0x040
#define PAD_DOWN 0x080

/* Keys pressed at power-on, for scripted screenshots: u, d, l, r, s select,
   e the Select button, b back, a the full-screen key, t one move of the
   running game, and p to start over as after a power cycle (settings are
   read back). */
#ifndef START_KEYS
#define START_KEYS ""
#endif

/* Battery-backed cartridge RAM, one byte at a time: a two-byte signature,
   then one four-byte record per game laid out as the phone stores them: top
   score high byte, low byte, level, and a check byte. */
#define SAVE ((volatile uint8_t *)0x0e000000)
#define SAVE_SIGNATURE_0 'N'
#define SAVE_SIGNATURE_1 '3'

/* Emulators and flash carts find the save type by this string. */
__attribute__((used)) static const char save_type[] = "SRAM_V113";

void irq_handler(void);

/* Frames since power-on, counted by the interrupt handler in crt0.s. */
volatile uint32_t frame_count;

/* The interrupt handler reads the pad once a frame: pad_last is what was
   held then and pad_latch collects every new press until the main loop
   takes it. */
volatile uint32_t pad_last, pad_latch;

static uint16_t take_presses(void)
{
    uint16_t pressed;

    REG_IME = 0;
    pressed = (uint16_t)(pad_latch & 0x03ff);
    pad_latch = 0;
    REG_IME = 1;
    return pressed;
}

static uint8_t save_check(uint8_t hi, uint8_t lo, uint8_t level)
{
    return (uint8_t)(hi + lo + level + 0x5a);
}

void platform_settings_load(uint8_t game, struct game_settings *out)
{
    volatile uint8_t *record = SAVE + 2 + game * 4;
    uint8_t hi = record[0], lo = record[1], level = record[2];

    out->top_score = 0;
    out->level = 0;
    if (SAVE[0] == SAVE_SIGNATURE_0 && SAVE[1] == SAVE_SIGNATURE_1 && record[3] == save_check(hi, lo, level)) {
        out->top_score = (uint16_t)(hi << 8 | lo);
        out->level = level;
    }
}

void platform_settings_save(uint8_t game, const struct game_settings *in)
{
    volatile uint8_t *record = SAVE + 2 + game * 4;
    uint8_t hi = (uint8_t)(in->top_score >> 8), lo = (uint8_t)in->top_score;

    SAVE[0] = SAVE_SIGNATURE_0;
    SAVE[1] = SAVE_SIGNATURE_1;
    record[0] = hi;
    record[1] = lo;
    record[2] = in->level;
    record[3] = save_check(hi, lo, in->level);
}

/* Called once a frame by the interrupt handler in crt0.s. */
void sound_frame(void)
{
    sound_tick(16743); /* a frame is 1/59.73 s */
}

/* The buzzer is pulse channel 2: a 50% square wave at full volume. Its
   frequency register is 2048 - 131072 / hz. */
void platform_tone(uint16_t hz)
{
    if (!hz) {
        REG_SOUND2CNT_L = 0; /* volume 0 */
        REG_SOUND2CNT_H = 0x8000;
        return;
    }
    REG_SOUND2CNT_L = 0xf080;
    REG_SOUND2CNT_H = (uint16_t)(0x8000 | (2048 - 131072ul / hz));
}

/* The magnified rectangle the screen currently shows. */
static uint8_t shown_x, shown_y, shown_w, shown_h;

/* One framebuffer pixel that changed: inside the magnified rectangle it is
   a block in the middle of the screen; outside, it goes where it is unless
   the magnified picture covers that spot. */
static void plot(int x, int y)
{
    uint16_t color = lcd_fb_pixel(x, y) ? COLOR_SET : COLOR_CLEAR;
    int zw = shown_w * LCD_ZOOM, zh = shown_h * LCD_ZOOM;
    int zx = (SCREEN_W - zw) / 2, zy = (SCREEN_H - zh) / 2;
    int px = x - shown_x, py = y - shown_y, i, j;

    if (px >= 0 && px < shown_w && py >= 0 && py < shown_h) {
        uint16_t *at = VRAM + (zy + py * LCD_ZOOM) * SCREEN_W + zx + px * LCD_ZOOM;

        for (j = 0; j < LCD_ZOOM; j++, at += SCREEN_W)
            for (i = 0; i < LCD_ZOOM; i++)
                at[i] = color;
    } else if (x < zx || x >= zx + zw || y < zy || y >= zy + zh) {
        VRAM[y * SCREEN_W + x] = color;
    }
}

/* An 8x8 cell at its own place, eight pixels from each framebuffer byte. */
static void plot_cell(int cx, int cy)
{
    const uint8_t *src = lcd_fb + cy * 8 * LCD_STRIDE + cx;
    uint16_t *dst = VRAM + cy * 8 * SCREEN_W + cx * 8;
    int row, i;

    for (row = 0; row < 8; row++, src += LCD_STRIDE, dst += SCREEN_W) {
        uint8_t bits = *src;

        for (i = 0; i < 8; i++, bits <<= 1)
            dst[i] = bits & 0x80 ? COLOR_SET : COLOR_CLEAR;
    }
}

/* How a cell's own place on the screen relates to the magnified picture:
   clear of it, under it, or partly both. */
enum {
    CELL_CLEAR,
    CELL_COVERED,
    CELL_PARTLY
};

static int cell_cover(int cx, int cy)
{
    int zw = shown_w * LCD_ZOOM, zh = shown_h * LCD_ZOOM;
    int zx = (SCREEN_W - zw) / 2, zy = (SCREEN_H - zh) / 2;
    int x = cx * 8, y = cy * 8;

    if (x + 8 <= zx || x >= zx + zw || y + 8 <= zy || y >= zy + zh)
        return CELL_CLEAR;
    if (x >= zx && x + 8 <= zx + zw && y >= zy && y + 8 <= zy + zh)
        return CELL_COVERED;
    return CELL_PARTLY;
}

/* Whether any of a cell lies in the magnified rectangle of the framebuffer. */
static int cell_magnified(int cx, int cy)
{
    int x = cx * 8, y = cy * 8;

    return x + 8 > shown_x && x < shown_x + shown_w && y + 8 > shown_y && y < shown_y + shown_h;
}

/* Redraws the 8x8 cells of lcd_fb drawn to since the last call, or the
   whole screen when what is magnified changes. */
static void present(void)
{
    uint8_t all = lcd_zoom_x != shown_x || lcd_zoom_y != shown_y || lcd_zoom_w != shown_w || lcd_zoom_h != shown_h;
    int cx, cy, x, y, cover;

    shown_x = lcd_zoom_x;
    shown_y = lcd_zoom_y;
    shown_w = lcd_zoom_w;
    shown_h = lcd_zoom_h;
    for (cy = 0; cy < LCD_CELLS_Y; cy++) {
        for (cx = 0; cx < LCD_CELLS_X; cx++) {
            uint8_t *dirty = &lcd_dirty[cx + LCD_CELLS_X * cy];

            if (!*dirty && !all)
                continue;
            *dirty = 0;
            if (!shown_w) {
                plot_cell(cx, cy);
                continue;
            }
            /* A cell clear of the magnified picture and not part of it is
               drawn whole; one under it and not part of it is not seen. */
            cover = cell_cover(cx, cy);
            if (!cell_magnified(cx, cy)) {
                if (cover == CELL_CLEAR) {
                    plot_cell(cx, cy);
                    continue;
                }
                if (cover == CELL_COVERED)
                    continue;
            }
            for (y = cy * 8; y < cy * 8 + 8; y++)
                for (x = cx * 8; x < cx * 8 + 8; x++)
                    plot(x, y);
        }
    }
}

static void press_script_key(char key)
{
    if (key == 'p')
        menu_init();
    else if (key == 't')
        menu_game_step();
    else
        menu_key(key == 'u' ? MENU_KEY_UP : key == 'd' ? MENU_KEY_DOWN : key == 'l' ? MENU_KEY_LEFT
                 : key == 'r' ? MENU_KEY_RIGHT : key == 's' ? MENU_KEY_SELECT
                 : key == 'a' ? MENU_KEY_START : key == 'e' ? MENU_KEY_ALT
                 : MENU_KEY_BACK);
}

int main(void)
{
    uint32_t seen = 0;
    uint16_t pressed;
    uint8_t changed;
    const char *key;
    int i;

    /* The cartridge's fast timing and prefetch: the code runs from ROM, and
       at the power-on timing it is several times slower. */
    REG_WAITCNT = 0x4317;

    for (i = 0; i < SCREEN_W * SCREEN_H; i++)
        VRAM[i] = COLOR_CLEAR;
    shown_w = 0xff; /* nothing is shown yet: the first present draws it all */
    REG_DISPCNT = 0x0403; /* mode 3, BG2 on */

    IRQ_VECTOR = irq_handler;
    REG_DISPSTAT |= 0x0008; /* raise an interrupt at each vertical blank */
    REG_IE = 0x0001;
    REG_IME = 1;

    REG_SOUNDCNT_X = 0x0080; /* sound on, channel 2 to both sides at full volume */
    REG_SOUNDCNT_L = 0x2277;
    REG_SOUNDCNT_H = 0x0002;

    menu_init();
    for (key = START_KEYS; *key; key++)
        press_script_key(*key);
    menu_draw();
    present();

    for (;;) {
        uint32_t frames;

        while (frame_count == seen)
            ;

        /* Keys first, so a press takes effect before the game's next move. */
        pressed = take_presses();
        changed = 0;
        if (pressed & PAD_A)
            changed |= menu_key(MENU_KEY_SELECT);
        if (pressed & PAD_SELECT)
            changed |= menu_key(MENU_KEY_ALT);
        if (pressed & PAD_START)
            changed |= menu_key(MENU_KEY_START);
        if (pressed & PAD_B)
            changed |= menu_key(MENU_KEY_BACK);
        if (pressed & PAD_UP)
            changed |= menu_key(MENU_KEY_UP);
        if (pressed & PAD_DOWN)
            changed |= menu_key(MENU_KEY_DOWN);
        if (pressed & PAD_LEFT)
            changed |= menu_key(MENU_KEY_LEFT);
        if (pressed & PAD_RIGHT)
            changed |= menu_key(MENU_KEY_RIGHT);
        /* The time a key's redraw takes is not game time: a new game's
           clock starts once its board is on the screen. */
        if (changed) {
            menu_draw();
            present();
            seen = frame_count;
        }

        /* One menu tick per frame, catching up on frames spent drawing a
           move, but never by so much that the game visibly jumps ahead. */
        frames = frame_count - seen;
        seen += frames;
        if (frames > MAX_CATCH_UP)
            frames = MAX_CATCH_UP;
        while (frames--) {
            if (menu_tick()) {
                menu_draw();
                present();
            }
        }
    }
}
