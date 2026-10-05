/* What the cartridge RAM keeps between power cycles, for the menus (bank
   1, as they are). It starts with the settings: a two-byte signature, then
   one four-byte record per game laid out as the phone stores them: top
   score high byte, low byte, level (the option in its top four bits:
   Snake II's maze) and a check byte; then the games'
   settings as one byte of switches (Sounds, Lights, Shakes in bits 0 to 2)
   and its check byte. main.c enables the RAM at power-on. */
#include <stdint.h>

#include "game.h"

#define SAVE ((uint8_t *)0xa000)
#define SAVE_SIGNATURE_0 'N'
#define SAVE_SIGNATURE_1 '4'
#define SAVE_CHECK(r) ((uint8_t)((r)[0] + (r)[1] + (r)[2] + 0x5a))
#define SAVE_OPTIONS (2 + GAME_COUNT * 4)
#define OPTIONS_CHECK(flags) ((uint8_t)((flags) ^ 0xa5))

/* Where a game's record is: the games' in order. */
static uint8_t save_record(uint8_t slot)
{
    return (uint8_t)(2 + slot * 4);
}

uint8_t platform_settings_load(uint8_t game, struct game_settings *out)
{
    const uint8_t *record = SAVE + save_record(game);

    out->top_score = 0;
    out->level = 0;
    out->option = 0;
    if (SAVE[0] == SAVE_SIGNATURE_0 && SAVE[1] == SAVE_SIGNATURE_1 && record[3] == SAVE_CHECK(record)) {
        out->top_score = (uint16_t)(record[0] << 8 | record[1]);
        out->level = record[2] & 15;
        out->option = record[2] >> 4;
        return 1;
    }
    return 0;
}

void platform_settings_save(uint8_t game, const struct game_settings *in)
{
    uint8_t *record = SAVE + save_record(game);

    SAVE[0] = SAVE_SIGNATURE_0;
    SAVE[1] = SAVE_SIGNATURE_1;
    record[0] = (uint8_t)(in->top_score >> 8);
    record[1] = (uint8_t)in->top_score;
    record[2] = (uint8_t)(in->level | in->option << 4);
    record[3] = SAVE_CHECK(record);
}

uint8_t platform_options_load(struct game_options *out)
{
    uint8_t flags = SAVE[SAVE_OPTIONS];

    if (SAVE[0] != SAVE_SIGNATURE_0 || SAVE[1] != SAVE_SIGNATURE_1 || SAVE[SAVE_OPTIONS + 1] != OPTIONS_CHECK(flags))
        return 0;
    out->sounds = flags & 1;
    out->lights = (uint8_t)(flags >> 1 & 1);
    out->shakes = (uint8_t)(flags >> 2 & 1);
    return 1;
}

void platform_options_save(const struct game_options *in)
{
    uint8_t flags = (uint8_t)(in->sounds | in->lights << 1 | in->shakes << 2);

    SAVE[0] = SAVE_SIGNATURE_0;
    SAVE[1] = SAVE_SIGNATURE_1;
    SAVE[SAVE_OPTIONS] = flags;
    SAVE[SAVE_OPTIONS + 1] = OPTIONS_CHECK(flags);
}
