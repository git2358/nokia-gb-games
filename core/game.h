/* What a platform layer keeps for the games between power cycles. */
#ifndef CORE_GAME_H
#define CORE_GAME_H

#include <stdint.h>

/* The games in the order of the phone's list. */
enum {
    GAME_SNAKE,
    GAME_SPACE_IMPACT,
    GAME_BANTUMI,
    GAME_PAIRS,
    GAME_COUNT
};

/* Per-game settings, kept in save storage by the platform. */
struct game_settings {
    uint16_t top_score;
    uint8_t level;
};

/* Load reports whether anything had been saved, and gives zeros if not. */
uint8_t platform_settings_load(uint8_t game, struct game_settings *out);
void platform_settings_save(uint8_t game, const struct game_settings *in);

/* The games' own settings, the phone's Games, Settings pages: each 0 or 1.
   Lights is kept but does nothing here. The phone starts with Sounds off
   and the other two on; the port starts with all three on. */
struct game_options {
    uint8_t sounds;
    uint8_t lights;
    uint8_t shakes;
};

/* Load reports whether anything had been saved, and leaves `out` as it
   was if not. */
uint8_t platform_options_load(struct game_options *out);
void platform_options_save(const struct game_options *in);

#endif
