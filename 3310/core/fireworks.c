#include "fireworks.h"

#include <string.h>

#include "game_assets.h"
#include "sprite.h"

void fireworks_draw(uint8_t picture)
{
    memcpy(sprite_screen, fireworks + (uint16_t)picture * sizeof sprite_screen, sizeof sprite_screen);
}
