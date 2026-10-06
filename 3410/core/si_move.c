/* Space Impact: the objects' moves, each tick (si_int.h). */
#include "si_int.h"

/* 0x25a920 */
void si_enemy_fire(uint8_t k, uint8_t type) SI_FAR
{
    if (X(k) <= W)
        si_spawn(type, si.mode, X(k) - 2, (height(k) >> 1) + Y(k));
}

/* 0x25a96e: up and down, turning at the play area's limits and the
   terrain, with a direction kept per object. */
static void move_bounce(uint8_t k, uint8_t bullet)
{
    int y = Y(k), h = height(k);

    if (PIC(k) != si.bounce_last) {
        si.bounce_last = PIC(k);
        si.bounce_dir = -1;
        si.bounce_latch = 0;
        si.bounce_age = 0;
    }
    if ((si.place == FLOOR && y <= si.top) || (si.place == CEILING && si.bottom <= y + h)) {
        si.bounce_dir = si.place == FLOOR && y <= si.top ? 1 : -1;
        si.bounce_latch = 0;
    } else if (y + h < H && y >= 0) {
        if (si_terrain_collide(k)) {
            if (!si.bounce_latch) {
                si.bounce_dir = (int8_t)-si.bounce_dir;
                si.bounce_latch = si.bounce_dir;
                si.bounce_age = 0;
            } else if (++si.bounce_age > 10) {
                si.bounce_latch = 0;
            }
        }
    } else {
        si.bounce_dir = y < 0 ? 1 : -1;
        si.bounce_latch = si.bounce_dir;
        if (y < 0)
            y = 1;
        if (H <= y + h)
            y = H - h - 1;
    }
    if (fire_roll(k) && bullet)
        si_enemy_fire(k, bullet);
    if (si.rec[k].boss)
        si.boss_state = 0x1e;
    if (si.bounce_latch)
        si.bounce_dir = si.bounce_latch;
    Y(k) = (int16_t)(si.bounce_dir + y);
}

/* 0x25aaf2: a boss comes on until it is in place. */
static int boss_enter(uint8_t k)
{
    if (si.boss_state != 0x28 && si.boss_state != 0x14 && X(k) > (W / 3) * 2 - 5) {
        X(k)--;
        return 0;
    }
    return 1;
}

/* 0x25ab3e */
static void boss_charge(uint8_t k, uint8_t bullet, int reach)
{
    int x = X(k), y = Y(k);

    switch (si.boss_state) {
    case 10:
        if ((W / 3) * 2 - 5 <= x)
            si.boss_state = 0x1e;
        else
            X(k) = (int16_t)(x + 1);
        return;
    case 0x14:
        if (x <= W / 8) {
            si.boss_state = 10;
            return;
        }
        x += reach == 100 ? -2 : -4;
        if (si_terrain_collide(k))
            y += si.place == FLOOR ? -1 : 1;
        si_pic_move(PIC(k), x, y);
        return;
    case 0x28:
        if (x < W)
            X(k) = (int16_t)(x + 1);
        else
            si.boss_state = 0x14;
        return;
    }
    move_bounce(k, bullet);
    if ((unsigned)game_rand() % 0x32 || si.bottom <= y || y <= si.top || x > (W / 3) * 2)
        return;
    si.boss_state = reach == 100 ? 0x14 : 0x28;
}

/* 0x25ac44: the boss's own projectile, from the chapter's settings. */
static void boss_fire(uint8_t k)
{
    uint8_t type = SETTINGS(2);
    int dy = (int8_t)SETTINGS(3), dx = (int8_t)SETTINGS(4);

    if (width(k) < dx)
        dx = 0;
    if (height(k) < dy)
        dy = 0;
    if (!fire_roll(k)) {
        if (!si.boss_cooldown)
            return;
    } else if (!si.boss_cooldown) {
        uint8_t j = si_spawn(type, si.mode, (int8_t)(X(k) + dx - type_frames(type)[0].w), Y(k) + dy);

        si.rec[j].no_score = 1;
        si.rec[j].pattern = SETTINGS(5);
        si.rec[j].speed = SETTINGS(6);
        si.rec[j].hp = 3;
        si.boss_cooldown = 6;
        return;
    }
    si.boss_cooldown--;
}

/* 0x25ad28 */
static void move_boss(uint8_t k)
{
    uint8_t bullet = SETTINGS(1);

    if (!si.boss_state || si.boss_state == 0x7f)
        return;
    if (boss_enter(k)) {
        switch (SETTINGS(0)) {
        case 0:
            if (fire_roll(k) == 1 && bullet)
                si_enemy_fire(k, bullet);
            break;
        case 1:
            move_bounce(k, bullet);
            break;
        case 2:
            boss_charge(k, bullet, 100);
            break;
        case 3:
            boss_charge(k, bullet, 150);
            break;
        }
        if (SETTINGS(2))
            boss_fire(k);
    }
    si_pic_set_mode(PIC(k), SI_MODE_COPY);
}

/* 0x25ae30: along the screen, turning off the ceiling or the floor. */
static void move_slope(uint8_t k, int speed, int side)
{
    int x = X(k) - speed, y = Y(k), turned = 0;

    if (side == FLOOR) {
        if (x > W) {
            y = si.bottom;
        } else if (y > (H / 5) * 3) {
            y--;
            turned = 1;
        }
    } else {
        if (x > W) {
            y = si.top;
        } else if (y < H / 5) {
            y++;
            turned = 1;
        }
    }
    si_pic_move(PIC(k), x, y);
    if (!free_at_left(k) && turned && fire_roll(k))
        si_enemy_fire(k, TYPE_BULLET);
}

/* 0x25af08: leftwards with y read from a table by x. */
static void move_path(uint8_t k, int speed, int relative, int offset)
{
    int x = X(k) - speed, base, step, y;
    uint8_t pattern;

    base = relative ? offset : si.place == CEILING ? 10 : 0;
    base = row_adjust(base);
    if (x < 0)
        x = 0;
    step = (int8_t)si.path[x < W ? x : x % W];
    y = (int16_t)(base + step);
    if (si.place == CEILING) {
        if (y + height(k) > H - 8 - 6)
            y = H - 8 - height(k) - 6;
    } else if (si.place == FLOOR && y < 10) {
        y = 10;
    }
    si_pic_move(PIC(k), x, y);
    if (free_at_left(k))
        return;
    pattern = si.rec[k].pattern;
    if (pattern == 1) {
        if (y != 0x1b && y != 9)
            return;
    } else if (pattern >= 10 && pattern <= 13) {
        if (step != 4 && step != -4)
            return;
    } else if (pattern != 15 && pattern != 16) {
        return;
    }
    if (fire_roll(k))
        si_enemy_fire(k, TYPE_BULLET);
}

/* 0x25b37c */
static void move_track_ship(uint8_t k, int speed)
{
    int y = Y(k), ship_y = si_pics[si.ship_pic].y;

    if (y < ship_y)
        y++;
    else if (y > ship_y)
        y--;
    si_pic_move(PIC(k), X(k) - speed, y);
    free_at_left(k);
}

/* 0x25b520 */
void si_boss_destroyed(uint8_t k) SI_FAR
{
    int w = width(k), h = height(k), cx = X(k) + (w >> 1), cy = Y(k) + (h >> 1);
    uint8_t frames = si.rec[k].frames, i, pairs = (uint8_t)(IS_LAST_CHAPTER() + 5);

    object_free(k);
    si.boss_state = 0x7f;
    si.timer = 0x1e;
    si_spawn(TYPE_EXPLOSION, si.mode, cx, cy);
    if (frames)
        si.rec[k].frame = (uint8_t)((unsigned)game_rand() % frames);
    else
        game_rand();
    for (i = 0; i < pairs; i++) {
        int dx, dy;

        if (IS_LAST_CHAPTER()) {
            dx = (int8_t)((unsigned)game_rand() % 0x14 + 1);
            dy = (int8_t)((unsigned)game_rand() % 0xd + 1);
        } else {
            dx = (int8_t)(((unsigned)game_rand() & 3) + 1);
            dy = 6;
        }
        si_spawn(TYPE_EXPLOSION, si.mode, dx + cx, dy + cy);
        if (frames)
            si.rec[k].frame = (uint8_t)((unsigned)game_rand() % frames);
        si_spawn(TYPE_EXPLOSION, si.mode, cx - dx, cy - dy);
        if (frames)
            si.rec[k].frame = (uint8_t)((unsigned)game_rand() % frames);
    }
    si_award(0x7e, 100);
    vibrate();
}

/* 0x25b680: everything within three columns of the beam. */
static void beam_scan(void)
{
    uint8_t k;

    for (k = 0; k < RECORDS; k++) {
        struct object *o = &si.rec[k];
        int d;

        if (o->type == FREE || o->type == TYPE_SHIP || o->type == TYPE_EXPLOSION || o->side == SIDE_PLAYER)
            continue;
        d = X(k) - si.beam_col;
        if (d < -3 || d > 3)
            continue;
        if (o->type == TYPE_BONUS)
            si_award(TYPE_BONUS, 1);
        if (!o->boss) {
            explode(k);
        } else if (o->hp < 3) {
            si_boss_destroyed(k);
            continue;
        } else {
            o->hp -= 2;
        }
        si_award(0x7e, 10);
    }
}

/* 0x25b750: records 0 to 58, in order. */
void si_objects_step(void) SI_FAR
{
    uint8_t k;
    struct object *o = si.rec;

    for (k = 0; k < RECORDS - 1; k++, o++) {
        uint8_t pic;

        if (o->type == FREE)
            continue;
        pic = o->pic;
        si_pic_next_frame(pic);
        o->frame = si_pics[pic].frame;
        switch (o->type) {
        case TYPE_SHIP:
            continue;
        case TYPE_EXPLOSION:
            if (o->frame != 4)
                continue;
            if (pic != si.ship_pic) {
                object_free(k);
                continue;
            }
            if (--si.lives < 1) {
                if (si.continues < 2) {
                    si.phase = PHASE_OVER;
                    si.countdown = 0x1e;
                    si_high_score_save();
                    return;
                }
                si.continues--;
                si_continue_enter();
                return;
            }
            si_ship_spawn(0x14);
            continue;
        case TYPE_WALL:
            si_pic_move(pic, width(si.ship) + si_pics[si.ship_pic].x + 2,
                        si_pics[si.ship_pic].y - (height(k) >> 1) + 3);
            if (o->frame == 6)
                object_free(k);
            continue;
        case TYPE_BEAM:
            if (si.beam_col < W) {
                si.beam_col += 2;
                si_pics[pic].x = si_pics[pic].x2 = si.beam_col;
                beam_scan();
            } else {
                si_pic_free(pic);
                o->type = FREE;
                si.beam_col = 0;
            }
            continue;
        }
        switch (o->pattern) {
        case 1:
            si.path = PATH(0x4ad094);
            move_path(k, o->speed, 0, 0);
            break;
        case 2:
            si.path = PATH(0x4ad0f4);
            move_path(k, o->speed, 0, 0);
            break;
        case 3:
            move_track_ship(k, o->speed);
            break;
        case 4:
            si.path = PATH(0x4ad154);
            move_path(k, o->speed, 0, 0);
            break;
        case 5:
            if (!free_at_right(k))
                si_pic_move_by(pic, o->speed, 0);
            break;
        case 6:
            if (!free_at_left(k))
                si_pic_move_by(pic, -o->speed, 0);
            break;
        case 7:
            si_move_dive(k, o->speed);
            break;
        case 9:
            si_move_missile(k);
            break;
        case 10:
        case 11:
        case 12:
        case 13: {
            static const uint8_t offset[4] = { 0x23, 0x0b, 0x0e, 0x19 };

            si.path = PATH(0x4ad1b4);
            move_path(k, o->speed, 0x80, offset[o->pattern - 10]);
            break;
        }
        case 14:
            si.path = PATH(0x4ad1b4);
            move_path(k, o->speed, 0x80, 0x12);
            break;
        case 15:
            si.path = PATH(0x4ad31c);
            move_path(k, o->speed, 0, 0);
            break;
        case 16:
            si.path = PATH(0x4ad2bc);
            move_path(k, o->speed, 0, 0);
            break;
        case 17:
            move_slope(k, o->speed, CEILING);
            break;
        case 18:
            move_slope(k, o->speed, FLOOR);
            break;
        case 23:
            move_boss(k);
            break;
        case 24:
            if (X(k) <= (W / 3) * 2)
                move_bounce(k, TYPE_BULLET);
            else
                X(k)--;
            if (si.boss_state) {
                o->pattern = 3;
                o->speed = 1;
            }
            break;
        }
    }
}
