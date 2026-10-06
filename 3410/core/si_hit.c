/* Space Impact: collisions, spawning from the chapter's script, the
   missile and the dive (si_int.h). */
#include "si_int.h"

/* 0x25a3f4: the chapter's script. */
void si_spawn_step(void) SI_FAR
{
    if (si.delay) {
        si.delay--;
        if (si.delay)
            return;
    }
    while (si.entries_left) {
        const uint8_t *e;
        uint8_t k, n, mode, boss = 0;

        for (k = 0; k < 4; k++)
            if (si.entries - si.entries_left == si.checkpoints[k])
                si.checkpoint = si.entries_left;
        e = si.script + 9 * (si.entries - si.entries_left);
        if (e[4] == 0x18)
            for (k = 0; k < RECORDS; k++)
                if (si.rec[k].type != FREE && si.rec[k].pattern == 0x18)
                    si.rec[k].pattern = 3;
        si.delay = e[5];
        mode = si.mode;
        if (si.entries_left == 1) {
            boss = 1;
            mode = si.mode == 0x12 ? SI_MODE_COPY : SI_MODE_HIDDEN;
        }
        for (n = 0; n < e[0]; n++) {
            uint8_t y = e[6];
            int x;

            if (y == 0x3f) {
                unsigned r = (unsigned)game_rand();
                int limit;

                y = (uint8_t)(si.top + r % si.bottom);
                if (y < si.top)
                    y = si.top;
                limit = si.bottom - type_frames(e[1])[0].h;
                if (limit < y)
                    y = (uint8_t)limit;
            }
            if (y == 0 && si.place == FLOOR)
                y = (uint8_t)(si.top + 1);
            y = (uint8_t)row_adjust(y);
            x = e[4] == 5 ? (int16_t)-(n * e[2]) : (int16_t)(n * e[2] + W);
            k = si_spawn(e[1], mode, x, y);
            si.rec[k].pattern = e[4];
            si.rec[k].speed = e[3];
            si.rec[k].hp = e[8];
            si.rec[k].fire = e[7];
            if (boss) {
                si.boss_state = 1;
                si.boss = k;
                si.rec[k].boss = 1;
            }
        }
        si.entries_left--;
        if (si.delay)
            return;
    }
}

/* A byte of the terrain's bitmap as the phone reads it: an object part
   off the screen's left edge reads before the row, and before the bitmap
   itself there is the heap's own data. Taken here to be the block's size,
   192 bytes and an 8-byte header, as a big-endian word: inferred, the one
   reading that fits what MAME shows, a bullet at x -1 beside the terrain
   taken away on bit 3 of the byte before the bitmap and a projectile
   there not on bits 0 and 1. */
static const uint8_t before_terrain[4] = { 0x00, 0x00, 0x00, 0xc8 };

/* Whether row `orow` of an object's bitmap (w wide) at x has a set pixel
   on a set pixel of the terrain's row `trow`, in its first `cols` columns:
   the phone's test a pixel at a time, a row at a time. */
static uint8_t row_hits(const uint8_t *bitmap, uint8_t w, int x, uint8_t cols, uint8_t orow, uint8_t trow)
{
    const uint8_t *b = bitmap + w * (orow >> 3);
    uint8_t om = (uint8_t)(1u << (orow & 7)), tm = (uint8_t)(1u << (trow & 7)), c;
    int at = x + W * (trow >> 3);

    for (c = 0; c < cols; c++, at++) {
        if (!(b[c] & om) || at < -4)
            continue;
        if ((at < 0 ? before_terrain[4 + at] : si.terrain[at]) & tm)
            return 1;
    }
    return 0;
}

/* 0x25a5d4: whether an object's picture touches the terrain's (frame 0's
   bitmap, the current frame's size). The phone goes a row at a time,
   giving up (no touch) at the first column off the screen's right edge,
   and in the first two cases at the first row past the picture's. */
int si_terrain_collide(uint8_t k) SI_FAR
{
    const uint8_t *bitmap = type_frames(si.rec[k].type)[0].bitmap;
    int w = width(k), h = height(k), x = X(k), y = Y(k);
    int ty = si_pics[si.terrain_pic].y, th = si.map_rows << 3, orow, trow, n;
    uint8_t cols = x >= W ? 0 : W - x < w ? (uint8_t)(W - x) : (uint8_t)w, clipped = w > cols;

    if (si.place == CEILING && y > ty + th)
        return 0;
    if (si.place == FLOOR && y + h < ty)
        return 0;
    if (ty < y && y + h < ty + th) {
        trow = y - ty;
        for (orow = 0, n = th - trow; n > 0; n--, orow++, trow++) {
            if (w && orow >= h)
                return 0;
            if (row_hits(bitmap, (uint8_t)w, x, cols, (uint8_t)orow, (uint8_t)trow))
                return h / 2 < orow ? 2 : 1;
            if (clipped)
                return 0;
        }
    } else if (y < ty && ty < y + h) {
        orow = ty - y;
        for (trow = 0, n = h - orow; n > 0; n--, orow++, trow++) {
            if (w && orow >= h)
                return 0;
            if (row_hits(bitmap, (uint8_t)w, x, cols, (uint8_t)orow, (uint8_t)trow))
                return 2;
            if (clipped)
                return 0;
        }
    } else if (ty < y && y < ty + th) {
        trow = y - ty;
        for (orow = 0; orow < h; orow++, trow++) {
            if (row_hits(bitmap, (uint8_t)w, x, cols, (uint8_t)orow, (uint8_t)trow))
                return 1;
            if (clipped)
                return 0;
            if (trow + 1 >= th)
                return 0;
        }
    }
    return 0;
}

/* 0x25b06c: the enemy with the most hit points, by record. */
static uint8_t missile_pick_target(void)
{
    uint8_t k, best = 0;
    int most = -99;

    for (k = 0; k < RECORDS; k++) {
        uint8_t t = si.rec[k].type;

        if (t != FREE && t != TYPE_SHIP && t != TYPE_EXPLOSION && si.rec[k].side != SIDE_PLAYER
            && most < (int8_t)si.rec[k].hp) {
            best = k;
            most = (int8_t)si.rec[k].hp;
        }
    }
    return best;
}

/* 0x25b0f0 */
void si_move_missile(uint8_t k) SI_FAR
{
    int x = X(k), y = Y(k), aim;
    uint8_t t = si.missile_target;

    if (!t || X(t) < x)
        si.missile_target = t = missile_pick_target();
    if (!t) {
        si_pic_move_by(PIC(k), 1, 0);
        return;
    }
    aim = Y(t) + (height(t) >> 1);
    if (y < aim && y < si.bottom)
        y++;
    else if (y > aim && y > si.top)
        y--;
    si_pic_move(PIC(k), x + 2, y);
    free_at_right(k);
}

/* 0x25b1c4: leftwards, then down to the ship's row and a shot when level
   with it. */
void si_move_dive(uint8_t k, int speed) SI_FAR
{
    uint8_t top = 5, bottom = 0x21, x, y;
    int h;

    if (si.place == CEILING && si.map_rows < 3) {
        top = 0xf;
        bottom = 0x2b;
    }
    top = (uint8_t)row_adjust(top);
    bottom = (uint8_t)row_adjust(bottom);
    x = (uint8_t)X(k);
    y = (uint8_t)Y(k);
    if (free_at_left(k))
        return;
    h = height(k);
    if (x > W / 2) {
        if (W - width(k) < x) {
            x = (uint8_t)(x - speed);
            y = top;
        } else {
            x = (uint8_t)(x - speed);
            if (y < bottom - h)
                y = (uint8_t)(y + speed);
        }
    } else if (x == W / 2 || x == W / 2 - 1 || x == W / 2 - 2) {
        if (top < y) {
            y = (uint8_t)(y - speed);
            if (y == si_pics[si.ship_pic].y)
                si_enemy_fire(k, TYPE_BULLET);
        } else {
            x = (uint8_t)(x - speed);
        }
    } else {
        x = (uint8_t)(x - speed);
        if (y < bottom)
            y = (uint8_t)(y + speed);
    }
    if (bottom - h < y)
        y = (uint8_t)(bottom - h);
    si_pic_move(PIC(k), x, y);
}

/* 0x25ba34: boxes, on the low bytes of the places. */
static int overlap(uint8_t a, uint8_t b)
{
    uint8_t ax = (uint8_t)X(a), ay = (uint8_t)Y(a), bx = (uint8_t)X(b), by = (uint8_t)Y(b);

    return by <= (uint8_t)(ay + height(a)) && ay <= (uint8_t)(by + height(b)) && bx <= (uint8_t)(ax + width(a))
           && ax <= (uint8_t)(bx + width(b));
}

/* 0x25bae6 */
static uint8_t find_hit(uint8_t a)
{
    uint8_t k;
    const struct object *o = si.rec;

    for (k = 0; k < RECORDS; k++, o++) {
        uint8_t t = o->type;

        if (t != FREE && t != TYPE_SHIP && t != TYPE_EXPLOSION && o->side != SIDE_PLAYER && overlap(a, k))
            return k;
    }
    return 0;
}

static int solid(unsigned a, unsigned b)
{
    return si.mode == 0x12 ? !a && !b : si.mode == 0x20 && a && b;
}

/* 0x25bb38 */
static int ship_pixel_collide(uint8_t other)
{
    uint8_t P = si.ship;
    const uint8_t *pb = type_frames(si.rec[P].type)[0].bitmap, *ob = type_frames(si.rec[other].type)[0].bitmap;
    int pw = width(P), ow = width(other), px = X(P), py = Y(P), ox = X(other), oy = Y(other);
    unsigned ship_row, other_row, ship_col, other_col, start_ship, start_other, cols, rows;

    if (py < oy) {
        ship_row = (uint8_t)(oy - py);
        other_row = 0;
    } else {
        other_row = (uint8_t)(py - oy);
        ship_row = 0;
    }
    if (px < ox) {
        ship_col = (uint8_t)(ox - px);
        other_col = 0;
    } else {
        other_col = (uint8_t)(px - ox);
        ship_col = 0;
    }
    start_ship = ship_col;
    start_other = other_col;
    if (px + pw < ox + ow)
        cols = (uint8_t)(pw - ship_col);
    else
        cols = (uint8_t)(ow - other_col);
    if (height(P) + py < oy + height(other))
        rows = (uint8_t)(height(P) - ship_row);
    else
        rows = (uint8_t)(height(other) - other_row);
    for (; rows; rows--) {
        unsigned a = start_ship, b = start_other, n;

        for (n = cols; n; n--) {
            unsigned ship_bit = pb[a + pw * (ship_row >> 3)] & 1u << (ship_row & 7);
            unsigned other_bit = ob[b + ow * (other_row >> 3)] & 1u << (other_row & 7);

            if (solid(ship_bit, other_bit))
                return 1;
            a = (a + 1) & 0xff;
            b = (b + 1) & 0xff;
        }
        ship_row = (ship_row + 1) & 0xff;
        other_row = (other_row + 1) & 0xff;
    }
    return 0;
}

/* 0x25be40: the row distance when the shot meets the boss's picture along
   its row, else 0. */
static int boss_pixel_hit(uint8_t shot, uint8_t boss)
{
    const uint8_t *bitmap = type_frames(si.rec[boss].type)[0].bitmap;
    int dy = Y(shot) - Y(boss), bw = width(boss), i;

    if (dy < 0)
        dy = -dy;
    for (i = 0; i <= width(shot); i = (int8_t)(i + 1)) {
        int dx = (int8_t)(X(shot) - X(boss));
        unsigned bit;

        if (dx < 0)
            dx = -dx;
        bit = bitmap[(unsigned)(dy >> 3) * bw + ((i + (dx & 0xff)) & 0xff)] & 1u << (dy & 7);
        if (si.mode == 0x12 ? !bit : si.mode == 0x20 && bit)
            return dy;
    }
    return 0;
}

static void ship_destroyed(void)
{
    explode(si.ship);
    si.ship_lost = 1;
    vibrate();
}

/* 0x25bd24 */
void si_ship_collisions(void) SI_FAR
{
    uint8_t hit;

    if (si.ship_lost == 1)
        return;
    if (kills() && si_terrain_collide(si.ship)) {
        si.ship_lost = 1;
        explode(si.ship);
        vibrate();
    }
    if (si.shield == NO_RECORD && (hit = find_hit(si.ship)) != 0
        && (si.rec[hit].type == TYPE_BULLET || ship_pixel_collide(hit) == 1)) {
        struct object *o = &si.rec[hit];

        if (o->type == TYPE_BONUS) {
            si_award(TYPE_BONUS, 1);
            object_free(hit);
            return;
        }
        if (--o->hp == 0) {
            if (!o->boss) {
                explode(hit);
                if (!o->no_score)
                    si_award(0x7e, 10);
            } else {
                si_boss_destroyed(hit);
            }
        }
        ship_destroyed();
    }
}

static void boss_flash(uint8_t k)
{
    if (si.flashed == PIC(k)) {
        si.flashed = 0;
    } else {
        si_pic_set_mode(PIC(k), SI_MODE_NONE);
        si.flashed = PIC(k);
    }
}

/* 0x25bf64: the player's projectiles, and every projectile against the
   terrain. */
void si_shot_collisions(void) SI_FAR
{
    uint8_t k, on_terrain = 0;

    for (k = 0; k < RECORDS; k++) {
        struct object *o = &si.rec[k];
        uint8_t hit, lo, hi;
        int pixel;

        if (o->type == FREE || !o->side || o->type == TYPE_BEAM)
            continue;
        if (kills() && si_terrain_collide(k) && k != si.shield) {
            object_free(k);
            on_terrain = 1;
            continue;
        }
        if (on_terrain || o->side != SIDE_PLAYER || (hit = find_hit(k)) == 0)
            continue;
        if (!si.rec[hit].boss) {
            pixel = 1;
        } else {
            int weak = SETTINGS(7) && SETTINGS(8);

            lo = SETTINGS(7);
            hi = SETTINGS(8);
            pixel = boss_pixel_hit(k, hit);
            if (!((!weak && pixel) || (pixel < hi && lo < pixel)))
                goto spent;
        }
        {
            struct object *t = &si.rec[hit];

            if (t->boss)
                boss_flash(hit);
            if (o->type == TYPE_SHIELD) {
                t->hp = t->boss ? (uint8_t)(t->hp - 1) : 0;
                if (t->type == TYPE_BONUS) {
                    si_award(TYPE_BONUS, 1);
                    object_free(hit);
                    t->hp = 1;
                }
                vibrate();
            } else if (o->type == TYPE_SHOT) {
                t->hp--;
            } else if (o->type == TYPE_WALL || o->type == TYPE_MISSILE) {
                t->hp = t->hp < 4 ? 0 : (uint8_t)(t->hp - 4);
                if (hit == si.missile_target)
                    explode(k);
            }
            if (!t->hp) {
                if (!t->boss) {
                    if (t->type != TYPE_BONUS) {
                        explode(hit);
                        if (!t->no_score)
                            si_award(0x7e, 10);
                    }
                } else {
                    si_boss_destroyed(hit);
                }
            } else if (!t->no_score) {
                si_award(0x7e, 5);
            }
        }
    spent:
        if (pixel && o->type != TYPE_SHIELD && o->type != TYPE_WALL)
            object_free(k);
    }
}
