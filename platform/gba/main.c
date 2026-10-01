/* GBA layer. For now it shows the core's test card: the 84x48 LCD is drawn
   at 2x (168x96), centred on the 240x160 screen in bitmap mode 3. */
#include <stdint.h>

#include "game.h"
#include "lcd.h"
#include "testcard.h"

#define REG_DISPCNT (*(volatile uint16_t *)0x04000000)
#define VRAM ((uint16_t *)0x06000000)

#define SCREEN_W 240
#define SCREEN_H 160
#define SCALE 2
#define ORIGIN_X ((SCREEN_W - LCD_WIDTH * SCALE) / 2)
#define ORIGIN_Y ((SCREEN_H - LCD_HEIGHT * SCALE) / 2)

#define RGB(r, g, b) ((uint16_t)((r) | (g) << 5 | (b) << 10))
#define COLOR_CLEAR RGB(19, 24, 15)
#define COLOR_SET RGB(4, 6, 3)

/* Settings are not saved on the GBA yet. */
void platform_settings_load(uint8_t game, struct game_settings *out)
{
    (void)game;
    out->top_score = 0;
    out->level = 0;
}

void platform_settings_save(uint8_t game, const struct game_settings *in)
{
    (void)game;
    (void)in;
}

static void present(void)
{
    int x, y;

    for (y = 0; y < LCD_HEIGHT * SCALE; y++) {
        uint16_t *dst = VRAM + (ORIGIN_Y + y) * SCREEN_W + ORIGIN_X;
        for (x = 0; x < LCD_WIDTH * SCALE; x++)
            dst[x] = lcd_pixel(x / SCALE, y / SCALE) ? COLOR_SET : COLOR_CLEAR;
    }
}

int main(void)
{
    REG_DISPCNT = 0x0403; /* mode 3, BG2 on */

    testcard_draw();
    present();

    for (;;)
        ;
}
