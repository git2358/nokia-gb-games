/* GBA layer. The 84x48 LCD is drawn at 2x (168x96), centred on the 240x160
   screen in bitmap mode 3. Start or A is the phone's Navi key, B is its C
   key, and the D-pad scrolls and steers. */
#include <stdint.h>

#include "game.h"
#include "lcd.h"
#include "menu.h"

#define REG16(addr) (*(volatile uint16_t *)(addr))
#define REG_DISPCNT REG16(0x04000000)
#define REG_DISPSTAT REG16(0x04000004)
#define REG_KEYINPUT REG16(0x04000130)
#define REG_IE REG16(0x04000200)
#define REG_IME REG16(0x04000208)
#define IRQ_VECTOR (*(void (*volatile *)(void))0x03007ffc)
#define VRAM ((uint16_t *)0x06000000)

#define SCREEN_W 240
#define SCREEN_H 160
#define SCALE 2
#define ORIGIN_X ((SCREEN_W - LCD_WIDTH * SCALE) / 2)
#define ORIGIN_Y ((SCREEN_H - LCD_HEIGHT * SCALE) / 2)

#define RGB(r, g, b) ((uint16_t)((r) | (g) << 5 | (b) << 10))
#define COLOR_CLEAR RGB(19, 24, 15)
#define COLOR_SET RGB(4, 6, 3)
#define COLOR_BEZEL RGB(3, 4, 6)

#define PAD_A 0x001
#define PAD_B 0x002
#define PAD_START 0x008
#define PAD_RIGHT 0x010
#define PAD_LEFT 0x020
#define PAD_UP 0x040
#define PAD_DOWN 0x080

/* Keys pressed at power-on, for scripted screenshots: u, d, l, r, s select,
   b back, t one move of the running game, and p to start over as after a
   power cycle (settings are read back). */
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

void platform_beep(void)
{
}

/* Redraws the 8x8 cells of lcd_fb drawn to since the last call. */
static void present(void)
{
    int cx, cy, x, y;

    for (cy = 0; cy < LCD_CELLS_Y; cy++) {
        for (cx = 0; cx < LCD_CELLS_X; cx++) {
            uint8_t *dirty = &lcd_dirty[cx + LCD_CELLS_X * cy];

            if (!*dirty)
                continue;
            *dirty = 0;
            for (y = cy * 8; y < cy * 8 + 8; y++) {
                uint16_t *row = VRAM + (ORIGIN_Y + y * SCALE) * SCREEN_W + ORIGIN_X + cx * 8 * SCALE;

                for (x = cx * 8; x < cx * 8 + 8 && x < LCD_WIDTH; x++) {
                    uint16_t color = lcd_fb_pixel(x, y) ? COLOR_SET : COLOR_CLEAR;

                    row[0] = row[1] = row[SCREEN_W] = row[SCREEN_W + 1] = color;
                    row += SCALE;
                }
            }
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
                 : key == 'r' ? MENU_KEY_RIGHT : key == 's' ? MENU_KEY_SELECT : MENU_KEY_BACK);
}

int main(void)
{
    uint32_t seen = 0;
    uint16_t pad, last = 0, pressed;
    const char *key;
    int i;

    for (i = 0; i < SCREEN_W * SCREEN_H; i++)
        VRAM[i] = COLOR_BEZEL;
    REG_DISPCNT = 0x0403; /* mode 3, BG2 on */

    IRQ_VECTOR = irq_handler;
    REG_DISPSTAT |= 0x0008; /* raise an interrupt at each vertical blank */
    REG_IE = 0x0001;
    REG_IME = 1;

    menu_init();
    for (key = START_KEYS; *key; key++)
        press_script_key(*key);
    menu_draw();
    present();

    for (;;) {
        uint32_t frames;

        /* One menu tick per frame, catching up on frames spent drawing. */
        while (frame_count == seen)
            ;
        frames = frame_count - seen;
        seen += frames;
        while (frames--) {
            if (menu_tick()) {
                menu_draw();
                present();
            }
        }

        pad = (uint16_t)(~REG_KEYINPUT & 0x03ff);
        pressed = (uint16_t)(pad & ~last);
        last = pad;
        if (pressed & (PAD_START | PAD_A))
            menu_key(MENU_KEY_SELECT);
        else if (pressed & PAD_B)
            menu_key(MENU_KEY_BACK);
        else if (pressed & PAD_UP)
            menu_key(MENU_KEY_UP);
        else if (pressed & PAD_DOWN)
            menu_key(MENU_KEY_DOWN);
        else if (pressed & PAD_LEFT)
            menu_key(MENU_KEY_LEFT);
        else if (pressed & PAD_RIGHT)
            menu_key(MENU_KEY_RIGHT);
        if (pressed) {
            menu_draw();
            present();
        }
    }
}
