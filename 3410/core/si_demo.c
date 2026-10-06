/* Space Impact: the Instructions' demos, which play the game with keys of
   their own (si_int.h). */
#include "si_int.h"

/* The end of a demo, by itself or at a key going down. */
static uint8_t demo_end(void)
{
    si_pic_reset();
    si.demo = 0;
    return SI_DONE_CLOSE;
}

/* The keys demo's pointer, made afresh at each place it moves to. */
static void pointer_at(int x, int y)
{
    if (si.demo_pointer)
        si_pic_free(si.demo_pointer);
    si.demo_pointer = si_pic_create(SI_MODE_COPY, x, y, &si_pictures[SI_PIC_POINTER], 1);
}

/* A key going down, then the step it shares with the timer. */
static uint8_t key_then_tick(uint8_t key)
{
    si_play_event(SI_EVENT_KEY_DOWN, key);
    return si_play_event(SI_EVENT_TICK, 0);
}

/* A key going down in place of the step: the picture may have changed. */
static uint8_t key_alone(uint8_t key)
{
    si_play_event(SI_EVENT_KEY_DOWN, key);
    return SI_DONE_REDRAW;
}

/* 0x25c6d4: a demo's step, at each event of play. A key going down ends
   it; any other event counts a step, and the demo plays the step or
   presses a key, by its count. */
uint8_t si_demo_step(uint8_t event) SI_FAR
{
    int c;

    if (event == SI_EVENT_KEY_DOWN)
        return demo_end();
    c = ++si.demo_count;
    switch (si.demo) {
    case SI_DEMO_FIRE:
        /* Up every third step for a while, fire every fifth. */
        if (c == 100)
            return demo_end();
        if (c >= 20) {
            if (c < 38 && c % 3 == 0)
                return key_alone(8);
            if (c < 100 && c % 5 == 0)
                return key_alone(1);
        }
        break;
    case SI_DEMO_KEYS: {
        /* The keypad with a pointer on the key pressed: 8, 0, # and *,
           then 1 and 4. */
        static const uint8_t at[] = { 10, 68, 38, 20, 74, 38, 35, 62, 38, 50, 62, 23,
                                      65, 74, 23, 80, 62, 28, 100, 74, 28 };
        uint8_t i;

        if (c == 1) {
            si_pic_create(SI_MODE_COPY, 60, 21, &si_pictures[SI_PIC_KEYPAD], 1);
            si.demo_pointer = 0;
            pointer_at(68, 33);
        }
        for (i = 0; i < sizeof at; i += 3)
            if (c == at[i])
                pointer_at(at[i + 1], at[i + 2]);
        if (c < 12)
            return key_then_tick(8);
        if (c < 20)
            return key_then_tick(0);
        if (c < 35)
            return key_then_tick(SI_KEY_HASH);
        if (c < 50)
            return key_then_tick(SI_KEY_STAR);
        if (c < 76 && c % 3 == 0) {
            si.fire_count = 0;
            return key_alone(1);
        }
        if (c >= 85 && c < 110 && c % 15 == 0) {
            si.special_latch = 0;
            return key_alone(4);
        }
        if (c >= 115)
            return demo_end();
        break;
    }
    case SI_DEMO_BONUS:
        /* Right, along the bonuses' row. */
        if (c > 30 && c < 51)
            si_play_event(SI_EVENT_KEY_DOWN, SI_KEY_HASH);
        if (c >= 60)
            return demo_end();
        break;
    default:
        return 0;
    }
    return si_play_event(SI_EVENT_TICK, 0);
}

/* 0x259d30: a key going down, which only the demos send (play polls the
   keys held, si_keys_poll): a pixel's move, a shot or the special. */
void si_demo_key(uint8_t key) SI_FAR
{
    int x, y, moved = 0;

    if (si.ship_lost == 1)
        return;
    x = si_pics[si.ship_pic].x;
    y = si_pics[si.ship_pic].y;
    if (key == 0 && (si.place == FLOOR ? y < si.bottom + 9 : si.place == CEILING && y < si.bottom - 7)) {
        si_pic_move_by(si.ship_pic, 0, 1);
        moved = 1;
    }
    if (key == 8 && (si.place == CEILING ? y >= 2 : si.place == FLOOR && y > si.top)) {
        si_pic_move_by(si.ship_pic, 0, -1);
        moved = 1;
    }
    if (key == SI_KEY_STAR && x > 2) {
        si_pic_move_by(si.ship_pic, -1, 0);
        moved = 1;
    }
    if (key == SI_KEY_HASH && x < W - 10) {
        si_pic_move_by(si.ship_pic, 1, 0);
        moved = 1;
    }
    if ((key == 1 || key == 3) && !si.cooldown && si.fire_count < 3) {
        si_spawn(si.shot_type, si.mode, x + 6, y + 3);
        si.cooldown = 1;
        si.fire_count++;
        sound(SI_SOUND_SHOT);
    }
    if ((key == 4 || key == 6) && !si.cooldown && si.specials > 0) {
        if (si.special == TYPE_BEAM) {
            if (!si.beam_col) {
                uint8_t k;

                si.beam_col = (uint8_t)(width(si.ship) + x);
                k = find_free();
                if (k == NO_RECORD)
                    return;
                if (si.top == 0x10)
                    si.beam_line = si_pic_create_line(si.mode, si.beam_col, 0, si.beam_col, si.bottom);
                else
                    si.beam_line = si_pic_create_line(si.mode, si.beam_col, si.top, si.beam_col, H - 1);
                si_set_template(&si.rec[k], TYPE_BEAM);
                si.rec[k].pic = si.beam_line;
                si.specials--;
                sound(SI_SOUND_BEAM);
            }
        } else {
            si_spawn(si.special, si.mode, x + 6, y + 3);
            si.specials--;
            sound(SI_SOUND_SPECIAL);
        }
        si.cooldown = 1;
        draw_number(si.count_digits, (unsigned)(int)si.specials & 0xffff, 2);
    }
    if (moved && si.shield != NO_RECORD)
        si_pic_move(PIC(si.shield), si_pics[si.ship_pic].x - 2, si_pics[si.ship_pic].y - 2);
}
