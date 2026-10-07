#include "fireworks.h"

#include <string.h>

#include "game_assets.h"
#include "lcd.h"
#include "sprite.h"

#define SPARKLE_X 63
#define SPARKLE_WIDTH 21
#define SPARKLE_HEIGHT 24
#define SPARKLE_FRAME_BYTES 84

static const uint8_t sparkle_frames[SPARKLE_STEPS] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 10, 10, 9, 9, 10, 10, 9, 10 };

void fireworks_draw(uint8_t picture)
{
    memcpy(sprite_screen, fireworks + (uint16_t)picture * sizeof sprite_screen, sizeof sprite_screen);
}

void sparkle_draw(uint8_t step)
{
    lcd_blit_strips(SPARKLE_X, 0, SPARKLE_WIDTH, SPARKLE_HEIGHT,
                    top_score_sparkle + sparkle_frames[step] * SPARKLE_FRAME_BYTES);
}
