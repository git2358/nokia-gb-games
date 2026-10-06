/* Space Impact: the state and the helpers every part of the game uses. */
#include <string.h>

#include "rand.h"
#include "si_state.h"

struct si_state si;
uint8_t si_first_level;

/* The tile sets of the eight levels: the firmware stores these addresses
   one by one when a game starts. */
static const si_ref tiles_a[] = { SI_REF(0x312760ul), SI_REF(0x312780ul), SI_REF(0x3127a0ul) };
static const si_ref tiles_b[] = { SI_REF(0x3127e4ul), SI_REF(0x312804ul), SI_REF(0x312824ul), SI_REF(0x312844ul) };
static const si_ref tiles_c[] = { SI_REF(0x3124b8ul), SI_REF(0x3124d8ul), SI_REF(0x3124f8ul), SI_REF(0x312518ul) };
static const si_ref tiles_d[] = { SI_REF(0x3126b8ul), SI_REF(0x3126d8ul), SI_REF(0x3126f8ul), SI_REF(0x312718ul) };
static const si_ref tiles_e[] = { SI_REF(0x312254ul), SI_REF(0x312274ul), SI_REF(0x312294ul), SI_REF(0x3122b4ul) };
static const si_ref tiles_f[] = { SI_REF(0x312864ul), SI_REF(0x312884ul), SI_REF(0x3128a4ul), SI_REF(0x3128c4ul), SI_REF(0x3128e4ul), SI_REF(0x312904ul) };
static const si_ref tiles_g[] = { SI_REF(0x3128a4ul), SI_REF(0x312984ul), SI_REF(0x312924ul), SI_REF(0x312944ul), SI_REF(0x312964ul) };
const si_ref *const si_level_tiles[8] = { tiles_a, tiles_b, tiles_c, tiles_d, tiles_e, tiles_e, tiles_f, tiles_g };

/* Outside the state on the phone, and never reset by a new game. */
struct si_work si_work = { -1, 0, 0, 0, SI_YPATH_RIPPLE, 0 };

/* A firmware pointer stored in the data, as a place in the data. */
si_ref si_rom_ref(si_ref at)
{
    const uint8_t *p = SI_ROM(at);

    return (si_ref)(((uint16_t)p[2] << 8 | p[3]) - (uint16_t)SI_DATA_BASE);
}

void si_image(struct sprite_image *out, si_ref descriptor)
{
    out->bitmap = SI_ROM(si_rom_ref(descriptor));
    out->w = SI_ROM(descriptor)[8];
    out->h = SI_ROM(descriptor)[9];
}

const uint8_t *tilemap_tile(uint16_t address)
{
    return SI_ROM(address);
}

void si_set_image(uint16_t id, si_ref descriptor)
{
    struct sprite_image image;

    si_image(&image, descriptor);
    sprite_set_image(id, &image);
}

uint16_t si_create(si_ref descriptor, uint8_t mode, uint8_t layer, int x, int y)
{
    struct sprite_image image;

    si_image(&image, descriptor);
    return sprite_create(&image, mode, layer, x, y);
}

void si_digit_image(struct sprite_image *out, unsigned digit)
{
    out->bitmap = si_digit_glyphs + 4 * digit;
    out->w = 4;
    out->h = 5;
}

void si_set_template(uint16_t id, uint8_t type)
{
    memcpy(&si.objects[id], SI_ROM(SI_TEMPLATES + 12 * type), 12);
}

void si_objects_clear(void)
{
    int i;

    for (i = 0; i < OBJECT_COUNT; i++) {
        si.objects[i].type = TYPE_FREE;
        si.objects[i].frames = 0;
        si.objects[i].frame = 0;
        si.objects[i].no_score = 0;
    }
    si.objects_alive = 0;
}

/* Writes a number into a run of digit sprites, which must follow each
   other in the list. */
void si_draw_number(uint16_t id, uint16_t value, int digits)
{
    /* The five decimal digits, by taking away powers of ten: a division
       for each is slow where there is no divide instruction. */
    static const uint16_t tens[4] = { 10000, 1000, 100, 10 };
    uint8_t buf[5];
    struct sprite_image image;
    int i;

    for (i = 0; i < 4; i++)
        for (buf[i] = 0; value >= tens[i]; value -= tens[i])
            buf[i]++;
    buf[4] = (uint8_t)value;
    for (i = 5 - digits; i < 5; i++) {
        si_digit_image(&image, buf[i]);
        sprite_set_image(id, &image);
        id = sprites[id].next;
    }
}

si_ref si_special_icon(void)
{
    return si.special == TYPE_MISSILE ? SI_ICON_MISSILE : si.special == TYPE_BEAM ? SI_ICON_BEAM : SI_ICON_WALL;
}

uint16_t si_object_spawn(uint8_t type, uint8_t mode, int x, int y)
{
    uint16_t id;

    if (si.objects_alive >= OBJECT_LIMIT)
        return 0;
    id = si_create(si_type_frames[type], mode, (uint8_t)(2 - (type == TYPE_SHIELD)), x, y);
    si_set_template(id, type);
    si.objects_alive++;
    return id;
}

void si_hud_refresh(void)
{
    int i;

    si_draw_number(si.special_digits, (unsigned)si.specials, 2);
    si_draw_number(si.score_digits, si.score & 0xffff, 5);
    for (i = 0; i < 5; i++)
        sprite_set_mode(si.life_icons[i], i < si.lives ? SPRITE_MODE_XOR : SPRITE_MODE_HIDDEN);
}
