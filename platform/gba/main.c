/* GBA layer. The core's framebuffer is the whole 240x160 screen, shown in
   bitmap mode 3, with the phone's 84x48 LCD magnified twice in the middle.

   The D-pad moves the ship (the phone's 8, 0, * and # keys), A fires (1),
   B uses the special weapon (4) and Start begins a new game. The phone's
   keypad reports one key at a time, so one button is held at a time here
   too: the last one pressed. */
#include <stdint.h>

#include "games.h"
#include "lcd.h"
#include "rand.h"
#include "si.h"

#define REG16(addr) (*(volatile uint16_t *)(addr))
#define REG_DISPCNT REG16(0x04000000)
#define REG_DISPSTAT REG16(0x04000004)
#define REG_IE REG16(0x04000200)
#define REG_WAITCNT REG16(0x04000204)
#define REG_IME REG16(0x04000208)
#define IRQ_VECTOR (*(void (*volatile *)(void))0x03007ffc)
#define VRAM ((uint16_t *)0x06000000)

#define SCREEN_W LCD_FB_WIDTH
#define SCREEN_H LCD_FB_HEIGHT
/* Most frames of game time made up at once after a slow draw. */
#define MAX_CATCH_UP 6
#define FRAME_US 16743 /* a frame is 1/59.73 s */

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

/* Called once a frame by the interrupt handler in crt0.s. */
void sound_frame(void)
{
}

/* The game's sounds are not written yet. */
void platform_sound(uint8_t sound)
{
    (void)sound;
}

void platform_vibrate(void)
{
}

/* The buttons that are phone keys, and the key each one is. */
static const struct {
    uint16_t button;
    uint8_t key;
} keys[] = {
    { PAD_UP, SI_KEY_8 },   { PAD_DOWN, SI_KEY_0 }, { PAD_LEFT, SI_KEY_STAR },
    { PAD_RIGHT, SI_KEY_HASH }, { PAD_A, SI_KEY_1 },    { PAD_B, SI_KEY_4 },
};
#define KEY_COUNT (sizeof keys / sizeof keys[0])

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

/* Redraws the 8x8 cells of lcd_fb drawn to since the last call, or the
   whole screen when what is magnified changes. */
static void present(void)
{
    uint8_t all = lcd_zoom_x != shown_x || lcd_zoom_y != shown_y || lcd_zoom_w != shown_w || lcd_zoom_h != shown_h;
    int cx, cy, x, y;

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
            for (y = cy * 8; y < cy * 8 + 8; y++)
                for (x = cx * 8; x < cx * 8 + 8; x++)
                    plot(x, y);
        }
    }
}

int main(void)
{
    uint32_t seen = 0;
    uint16_t held_button = 0;
    uint8_t changed;
    unsigned i;

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

    lcd_view_phone();
    lcd_zoom_set(LCD_PHONE_X, LCD_PHONE_Y, LCD_WIDTH, LCD_HEIGHT);
    games_start();
    games_draw(1);
    present();
    /* The first picture takes a while to put up; the game's clock starts
       once it is there. */
    seen = frame_count;

    for (;;) {
        uint32_t frames;
        uint16_t pressed, held;

        while (frame_count == seen)
            ;

        /* Keys first, so a press takes effect before the game's next move. */
        pressed = take_presses();
        held = (uint16_t)pad_last;
        changed = 0;
        if (pressed & PAD_START) {
            /* The phone seeds the games' generator from its clock; here the
               time of the press does the same. */
            game_rand16_seed = (uint16_t)(frame_count % 0xfff0 + 1);
            games_key_up();
            held_button = 0;
            games_start();
            changed = 1;
        }
        for (i = 0; i < KEY_COUNT; i++) {
            if (pressed & keys[i].button) {
                held_button = keys[i].button;
                changed |= games_key_down(keys[i].key);
            }
        }
        if (held_button && !(held & held_button)) {
            /* The held button came up; another one still down takes over. */
            games_key_up();
            held_button = 0;
            for (i = 0; i < KEY_COUNT && !held_button; i++) {
                if (held & keys[i].button) {
                    held_button = keys[i].button;
                    changed |= games_key_down(keys[i].key);
                }
            }
        }

        /* Game time for every frame since the last pass, but after a slow
           draw never so much that the game visibly jumps ahead. */
        frames = frame_count - seen;
        seen += frames;
        if (frames > MAX_CATCH_UP)
            frames = MAX_CATCH_UP;
        while (frames--)
            changed |= games_elapse(FRAME_US);
        if (changed) {
            games_draw(0);
            present();
        }
    }
}
