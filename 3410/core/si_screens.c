/* Space Impact: the keys, the title and the High scores page
   (si_int.h). */
#include "si_int.h"
#include "si_rows.h"

/* 0x25a068 */
static void ship_clamp_y(void)
{
    int s = si.shield == NO_RECORD ? 0 : 2, y = si_pics[si.ship_pic].y;

    if ((H - s - 19) < y)
        y = H - s - 19;
    else if (y < s + 10)
        y = s + 10;
    si_pics[si.ship_pic].y = (int16_t)y;
}

/* 0x25a110: the keys held, every tick. */
void si_keys_poll(void) SI_FAR
{
    int s = si.shield == NO_RECORD ? 0 : 2, x, y, step, moved = 0;

    if (si.ship_lost == 1)
        return;
    x = si_pics[si.ship_pic].x;
    y = si_pics[si.ship_pic].y;
    if (held(0)) {
        if (si.accel < 3 || (si.place == FLOOR && y == H - s - 9)
            || (si.place == CEILING && y == si.bottom - s - 9)) {
            si.accel++;
            step = 1;
        } else {
            step = 2;
        }
        si_pic_move_by(si.ship_pic, 0, step);
        moved = 1;
    }
    if (held(8)) {
        if (si.accel < 3 || (si.place == FLOOR && 5 < y && y < 8 && s == 0)) {
            si.accel++;
            step = 1;
        } else {
            step = 2;
        }
        si_pic_move_by(si.ship_pic, 0, -step);
        moved = 1;
    }
    if (held(SI_KEY_STAR)) {
        if (si.accel < 3 || (s == 0 && 0 < x && x < 3)) {
            si.accel++;
            step = 1;
        } else {
            step = 2;
        }
        if (step + s < x) {
            si_pic_move_by(si.ship_pic, -step, 0);
            moved = 1;
        }
    }
    if (held(SI_KEY_HASH)) {
        if (si.accel < 3) {
            si.accel++;
            step = 1;
        } else {
            step = 2;
        }
        if (x <= W - step - 10) {
            si_pic_move_by(si.ship_pic, step, 0);
            moved = 1;
        }
    }
    if ((held(1) || held(3)) && !si.cooldown && si.fire_count < 3) {
        si_spawn(si.shot_type, si.mode, x + 6, y + 3);
        si.cooldown = 1;
        si.fire_count++;
    }
    if (!held(4) && !held(6)) {
        si.special_latch = 0;
    } else if (!si.special_latch && (si.special_latch = 1, si.specials >= 1)) {
        if (si.special == TYPE_BEAM) {
            if (!si.beam_col) {
                uint8_t k;

                si.beam_col = (uint8_t)(width(si.ship) + x);
                if (si.top == 0x10)
                    si.beam_line = si_pic_create_line(si.mode, si.beam_col, 0, si.beam_col, si.bottom);
                else
                    si.beam_line = si_pic_create_line(si.mode, si.beam_col, si.top, si.beam_col, H - 1);
                k = find_free();
                si_set_template(&si.rec[k], TYPE_BEAM);
                si.rec[k].pic = si.beam_line;
                si.specials--;
                draw_number(si.count_digits, (unsigned)(int)si.specials & 0xffff, 2);
            }
        } else {
            si_spawn(si.special, si.mode, x + 6, y + 3);
            si.specials--;
            draw_number(si.count_digits, (unsigned)(int)si.specials & 0xffff, 2);
        }
    }
    if (moved)
        ship_clamp_y();
}

/* The title (0x3db036, 0x3daf94, 0x3daf28): stars, the name's two halves
   closing in and four ships flying across, a step every 210 ms, nine of
   them, then 700 ms and the game's menu. */
static struct {
    uint8_t running;
    uint16_t step;
    uint8_t top, bottom, flyers[4];
} title;

static void stars(void)
{
    uint8_t i;

    for (i = 0; i < 30; i++) {
        int x = (int)((unsigned)game_rand() % W);
        int y = (int)((unsigned)game_rand() % H);

        si_pic_create(SI_MODE_XOR, x, y, &si_pictures[SI_PIC_STAR], 1);
    }
}

void si_title_start(void) SI_FAR
{
    static const int8_t flyers[4][2] = { { -52, 27 }, { 27, 25 }, { 0, 29 }, { -12, 26 } };
    uint8_t i;

    title.step = 0;
    si_pic_reset();
    si_pic_create_fill(SI_MODE_COPY, 0, 0, W, H);
    stars();
    title.top = si_pic_create(SI_MODE_COPY, 9, -10, &si_pictures[SI_PIC_LOGO_TOP], 1);
    title.bottom = si_pic_create(SI_MODE_COPY, 4, 60, &si_pictures[SI_PIC_LOGO_BOTTOM], 1);
    for (i = 0; i < 4; i++)
        title.flyers[i] = si_pic_create(SI_MODE_COPY, flyers[i][0], flyers[i][1],
                                        &si_pictures[i ? SI_PIC_TITLE_ENEMY : SI_PIC_TITLE_SHIP], 1);
    title.running = 1;
    si_period = 210;
}

uint8_t si_title_event(uint8_t event) SI_FAR
{
    uint8_t i;

    if (event == SI_EVENT_TICK) {
        if (++title.step < 10) {
            si_pic_move_by(title.top, 0, 3);
            si_pic_move_by(title.bottom, 0, -3);
            for (i = 0; i < 4; i++)
                si_pic_move_by(title.flyers[i], 12, 0);
            return SI_DONE_REDRAW;
        }
        if (title.step == 10) {
            si_period = 700;
            return 0;
        }
    } else if (event != SI_EVENT_KEY_DOWN) {
        return 0;
    }
    /* Over, or cut short by a key. */
    title.running = 0;
    si_pic_reset();
    return SI_DONE_CLOSE;
}

/* The High scores page (0x258a52, 0x259aec): the top score in a box with a
   medal each side, the last game's below when it was played with the same
   chapters, and a ship that shoots an enemy and flies off. */
uint16_t si_top_score, si_last_score;
uint8_t si_show_last;

static struct {
    uint8_t running, step, bob, fired;
    uint8_t ship, shot, enemy, explosion;
} page;

void si_scores_start(void) SI_FAR
{
    page.step = 0;
    page.bob = 1;
    page.fired = 0;
    si_pic_reset();
    page.ship = si_pic_create(SI_MODE_COPY, 0, H / 4, type_frames(TYPE_SHIP), 1);
    page.enemy = si_pic_create(SI_MODE_COPY, (W / 3) * 2, (H / 5) * 2, &si_pictures[SI_PIC_SCORES_ENEMY], 1);
    page.shot = si_pic_create(SI_MODE_NONE, 10, H / 4 + 3, type_frames(TYPE_SHOT), 1);
    page.explosion = si_pic_create(SI_MODE_NONE, (W / 3) * 2, (H / 5) * 2, type_frames(TYPE_EXPLOSION), 5);
    page.running = 1;
    si_period = 100;
}

uint8_t si_scores_event(uint8_t event) SI_FAR
{
    struct si_pic *ship = &si_pics[page.ship], *shot = &si_pics[page.shot], *enemy = &si_pics[page.enemy];

    if (event == SI_EVENT_KEY_DOWN || event == SI_EVENT_PAUSE) {
        page.running = 0;
        si_pic_reset();
        return SI_DONE_CLOSE;
    }
    if (event != SI_EVENT_TICK)
        return 0;
    if (page.step == 0) {
        enemy->y += page.bob ? 1 : -1;
        page.bob ^= 1;
        ship->x += 2;
        /* Down towards the enemy's row, else a shot, which then flies
           twice as fast on the ticks it is fired. */
        if (ship->y <= enemy->y - 2) {
            ship->y += 3;
            page.fired = 0;
        } else {
            si_pic_set_mode(page.shot, SI_MODE_COPY);
            page.fired = 1;
        }
        shot->y = (int16_t)(ship->y + si_pictures[si_type_picture[TYPE_SHIP]].h / 2);
        shot->x += page.fired ? 4 : 2;
        if (shot->x > enemy->x - 4)
            page.step = 1;
    } else if (page.step == 1) {
        si_pic_set_mode(page.shot, SI_MODE_NONE);
        si_pic_set_mode(page.enemy, SI_MODE_NONE);
        si_pic_set_mode(page.explosion, SI_MODE_COPY);
        ship->x += 3;
        page.step = 2;
    } else if (page.step <= 5) {
        si_pics[page.explosion].frame = (uint8_t)(page.step - 1);
        ship->x += 3;
        page.step++;
    } else {
        si_pic_set_mode(page.explosion, SI_MODE_NONE);
        if (ship->x <= W)
            ship->x += 3;
    }
    return SI_DONE_REDRAW;
}

void si_render(void) SI_FAR
{
    si_pic_render();
}

uint8_t si_scores_shown(void) SI_FAR
{
    return page.running;
}

/* The title's or the High scores page's turn at an event, while one is up. */
uint8_t si_screen_event(uint8_t event) SI_FAR
{
    if (title.running && event < 3)
        return si_title_event(event);
    if (page.running && event <= SI_EVENT_PAUSE)
        return si_scores_event(event);
    return SI_NO_SCREEN;
}

void si_render_rows(void) SI_FAR
{
    si_rows_render();
}
