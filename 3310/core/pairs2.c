#include "pairs2.h"

#include <string.h>

#include "game_assets.h"
#include "rand.h"
#include "sprite.h"

/* The pictures, by the firmware address of their 12-byte descriptors
   (bitmap address, four unused bytes, width, height). */
#define STRIP_TOP 0x31ad24ul      /* 84x6, the wipe's strips */
#define STRIP_BOTTOM 0x31ad30ul
#define EXPLOSION 0x31ad3cul      /* 84x48, two pictures */
#define REMOVAL 0x31ad60ul        /* Puzzle: seven pictures of a card going */
#define PICTURES 0x31adc0ul       /* 8x11, by picture */
#define DYNAMITE 0x31af70ul       /* 9x18 */
#define FLAME 0x31af7cul          /* 9x5, two pictures */
#define DOOR 0x31afa0ul           /* 19x23 */
#define SALOON 0x31afacul         /* 84x48 */
#define PUZZLE_PICTURE 0x31afb8ul /* 84x48, behind Puzzle's cards */
#define CARD_BACK 0x31afc4ul      /* 7x9 */
#define CURSOR 0x31afd0ul         /* 7x9, flipped over the card */

/* Time trial's nine boards: a mask per column of which rows hold a card,
   the board's time in ticks, and its number of cards. */
#define BOARD_MASKS 0x32fe74ul
#define BOARD_TIMES 0x32fed0ul
#define BOARD_COUNTS 0x32fee4ul
#define BOARDS 9

#define CARDS 60
#define DEAL_X 39 /* where every card starts its flight */
#define DEAL_Y 20

/* Card states, as the firmware's. */
#define CARD_REMOVED 7 /* Puzzle: the last removal picture shown */
#define CARD_ON_BOARD 8
#define CARD_GONE 9

/* Phases. */
enum {
    PHASE_DOOR = 0,     /* Time trial: the board behind the saloon's door */
    PHASE_NEXT = 1,     /* Time trial: before the next board */
    PHASE_DEAL = 2,
    PHASE_COLLECT = 3,  /* Time trial: a tick after the board is cleared */
    PHASE_PLAY = 4,
    PHASE_TIME_OUT = 5, /* Time trial: the explosion */
    PHASE_DONE = 7,     /* Puzzle: the last pair is going */
    PHASE_WIPE = 8,     /* Time trial: from the saloon to the play field */
    PHASE_CLEARED = 9   /* Time trial */
};

/* The sprite ids the game keeps, as the phone's table at 0x1090dc. */
enum {
    ID_DOOR,
    ID_FLAME,
    ID_BACK,     /* the saloon or Puzzle's picture, then the explosion */
    ID_DYNAMITE,
    ID_FUSE,
    ID_CURSOR,
    ID_FIRST,    /* the two enlarged pictures */
    ID_SECOND,
    ID_STRIPS,   /* eight from the top, then eight from the bottom */
    IDS = ID_STRIPS + 16
};

struct card {
    uint8_t picture;
    uint8_t state; /* CARD_ON_BOARD, CARD_GONE, or Puzzle's removal step 0..7 */
    uint8_t up;    /* face up: opened or matched */
    uint8_t x, y;  /* where it is while dealt */
    uint8_t board_x, board_y;
};

struct pairs2 {
    uint16_t time;     /* Time trial: ticks left */
    uint8_t count;     /* cards on the board */
    uint8_t counter;   /* a phase's countdown, the wipe's place, the explosion's step */
    uint8_t divisor;   /* the fuse burns a pixel every this many ticks */
    uint8_t fuse_top;
    uint8_t cursor;    /* a card */
    uint8_t first;     /* the first card opened */
    uint8_t open;      /* cards open: 0, 1, 2 (a miss showing), 3 while the game moves the cursor */
    uint8_t board;     /* Time trial: 0..8 */
    uint8_t level;     /* 1..7 */
    uint8_t phase;
    uint8_t mode;
    uint8_t running;   /* Time trial: the tick has the level's period */
    uint8_t flame;
    uint8_t wipe[8];
    struct card cards[CARDS];
    uint8_t columns[CARDS]; /* the cards in column order, for up and down */
    uint8_t sprites[CARDS]; /* each card's sprite */
    uint8_t ids[IDS];
};

/* A platform short of work RAM keeps the state elsewhere: the Game Boy
   in cartridge RAM, with SDCC's __at. (Not as a cast address: SDCC 4.6
   then reads a field back through HL after a store moved it.) */
#ifdef PAIRS2_STATE_AT
static __at(PAIRS2_STATE_AT) struct pairs2 p;
#ifdef PAIRS2_STATE_END
typedef char pairs2_state_fits[PAIRS2_STATE_AT + sizeof(struct pairs2) <= PAIRS2_STATE_END ? 1 : -1];
#endif
#else
static struct pairs2 p;
#endif

/* The data, by firmware address. */

static const uint8_t *data(uint32_t address)
{
    return pairs2_data + (uint16_t)(address - PAIRS2_DATA_BASE);
}

static const uint8_t *boards(uint32_t address)
{
    return pairs2_boards + (uint16_t)(address - PAIRS2_BOARDS_BASE);
}

static void image(struct sprite_image *out, uint32_t descriptor)
{
    const uint8_t *d = data(descriptor);

    /* The bitmaps lie within 64 KiB of the data's start. */
    out->bitmap = pairs2_data + (uint16_t)(((uint16_t)d[2] << 8 | d[3]) - (uint16_t)PAIRS2_DATA_BASE);
    out->w = d[8];
    out->h = d[9];
}

static uint8_t create(uint32_t descriptor, uint8_t mode, uint8_t layer, int x, int y)
{
    struct sprite_image im;

    image(&im, descriptor);
    return (uint8_t)sprite_create(&im, mode, layer, x, y);
}

static void set_image(uint8_t id, uint32_t descriptor)
{
    struct sprite_image im;

    image(&im, descriptor);
    sprite_set_image(id, &im);
}

/* The phone frees ids it no longer holds now and then; freeing an id that
   is not in the list does nothing, and one that has been made again frees
   that sprite, as on the phone. */
static void free_id(uint8_t id)
{
    if (id)
        sprite_free(id);
}

/* Dealing. */

static void deal(uint8_t n)
{
    uint8_t half, i;

    p.count = n;
    p.open = 0;
    p.phase = PHASE_DEAL;
    p.cursor = 0;
    p.flame = 0;
    p.time = (uint16_t)(boards(BOARD_TIMES)[2 * p.board] << 8 | boards(BOARD_TIMES)[2 * p.board + 1]);
    p.divisor = (uint8_t)(p.time / 25);
    p.fuse_top = 8;
    for (half = 0; half < 2; half++)
        for (i = 0; i < n / 2; i++) {
            struct card *c = &p.cards[half * (n / 2) + i];

            c->picture = i;
            c->up = 0;
            c->state = CARD_ON_BOARD;
        }
}

static void shuffle(void)
{
    uint8_t i, j;
    struct card t;

    for (i = 0; i < p.count; i++) {
        j = (uint8_t)(game_rand16() % p.count);
        t = p.cards[i];
        p.cards[i] = p.cards[j];
        p.cards[j] = t;
    }
}

static void place(uint8_t i, uint8_t x, uint8_t y)
{
    struct card *c = &p.cards[i];

    p.sprites[i] = create(CARD_BACK, SPRITE_MODE_OPAQUE, 1, DEAL_X, DEAL_Y);
    c->x = DEAL_X;
    c->y = DEAL_Y;
    c->board_x = x;
    c->board_y = y;
}

/* Time trial: the cards where the board's masks have them, row by row. */
static void layout_time_trial(void)
{
    const uint8_t *masks = boards(BOARD_MASKS) + 10 * p.board;
    uint8_t row, column, n = 0;

    for (row = 0; row < 5; row++)
        for (column = 0; column < 10; column++)
            if (masks[column] >> row & 1)
                place(n++, (uint8_t)(12 + 7 * column), (uint8_t)(1 + 9 * row));
    p.count = n;
}

/* Puzzle: a full grid in the middle, then the picture behind it. */
static void layout_puzzle(uint8_t rows, uint8_t columns)
{
    uint8_t y = (uint8_t)((5 - rows) >> 1) * 9, row, column, n = 0;

    if (!(rows & 1))
        y = (uint8_t)(y + 4);
    for (row = 0; row < rows; row++, y = (uint8_t)(y + 9)) {
        uint8_t x = (uint8_t)(((12 - columns) >> 1) * 7);

        for (column = 0; column < columns; column++, x = (uint8_t)(x + 7)) {
            place(n, x, y);
            p.cards[n++].state = CARD_ON_BOARD;
        }
    }
    p.ids[ID_BACK] = create(PUZZLE_PICTURE, SPRITE_MODE_OPAQUE, 0, 0, 0);
}

/* The column order up and down go by: from card 0 down its column, then
   the next column to the right from its top, and so on. */
static void column_order(void)
{
    uint8_t n = 0, at = 0, i, found;

    for (;;) {
        for (;;) {
            p.columns[n++] = at;
            found = 0;
            for (i = 0; i < p.count; i++)
                if (p.cards[i].board_x == p.cards[at].board_x && p.cards[i].board_y > p.cards[at].board_y) {
                    found = 1;
                    break;
                }
            if (!found)
                break;
            at = i;
        }
        {
            uint8_t best = 0xff, next = 0xff;

            for (i = 0; i < p.count; i++)
                if (p.cards[i].board_x > p.cards[at].board_x && p.cards[i].board_x - p.cards[at].board_x < best) {
                    best = (uint8_t)(p.cards[i].board_x - p.cards[at].board_x);
                    next = i;
                }
            if (next == 0xff)
                return;
            at = next;
        }
    }
}

static void board_time_trial(void)
{
    deal(boards(BOARD_COUNTS)[p.board]);
    shuffle();
    layout_time_trial();
    column_order();
    p.ids[ID_BACK] = create(SALOON, SPRITE_MODE_OPAQUE, 0, 0, 0);
    p.ids[ID_DOOR] = create(DOOR, SPRITE_MODE_OPAQUE, 0, 32, 17);
}

static void board_puzzle(void)
{
    uint8_t rows = 2, columns = 2;

    if (p.level != 1) {
        columns = (uint8_t)(2 * (p.level - 1));
        rows = (uint8_t)(p.level / 2 + 2);
    }
    deal((uint8_t)(rows * columns));
    shuffle();
    layout_puzzle(rows, columns);
    column_order();
}

/* Moves every card a pixel nearer its place on each axis. Returns whether
   any moved; a card already in place has its sprite freed in the phase
   after a cleared board, as on the phone, though it went at its match. */
static uint8_t deal_step(void)
{
    uint8_t i, moved = 0;

    for (i = 0; i < p.count; i++) {
        struct card *c = &p.cards[i];

        if (c->x == c->board_x && c->y == c->board_y) {
            if (p.phase == PHASE_COLLECT)
                free_id(p.sprites[i]);
            continue;
        }
        c->x = (uint8_t)(c->x + (c->board_x > c->x) - (c->board_x < c->x));
        c->y = (uint8_t)(c->y + (c->board_y > c->y) - (c->board_y < c->y));
        sprite_move(p.sprites[i], c->x, c->y);
        moved = 1;
    }
    return moved;
}

/* The cursor. */

static void move_cursor(void)
{
    sprite_move(p.ids[ID_CURSOR], p.cards[p.cursor].board_x, p.cards[p.cursor].board_y);
}

/* A miss showing is turned face down by the next move. */
static void close_pair(void)
{
    struct sprite_image back;

    if (p.open != 2)
        return;
    image(&back, CARD_BACK);
    sprite_set_image(p.sprites[p.first], &back);
    sprite_set_image(p.sprites[p.cursor], &back);
    p.cards[p.cursor].up = 0;
    p.cards[p.first].up = 0;
    p.open = 0;
    free_id(p.ids[ID_FIRST]);
    free_id(p.ids[ID_SECOND]);
    sprite_set_mode(p.ids[ID_CURSOR], SPRITE_MODE_XOR);
}

/* Left (back) and right (on) through the cards, skipping those face up. */
static void cursor_row(uint8_t on)
{
    uint8_t at = p.cursor;

    close_pair();
    do {
        if (on) {
            if (++at == p.count)
                at = 0;
        } else {
            if (at == 0)
                at = p.count;
            at--;
        }
    } while (p.cards[at].up);
    p.cursor = at;
    move_cursor();
}

/* Up (back) and down (on) through the column order. */
static void cursor_column(uint8_t on)
{
    uint8_t place = 0, card;

    close_pair();
    while (p.columns[place] != p.cursor)
        place++;
    do {
        if (on)
            place = place == p.count - 1 ? 0 : (uint8_t)(place + 1);
        else
            place = place == 0 ? (uint8_t)(p.count - 1) : (uint8_t)(place - 1);
        card = p.columns[place];
    } while (p.cards[card].up);
    p.cursor = card;
    move_cursor();
}

/* Opening cards. */

static int clamp_x(int x)
{
    return x < 0 ? 0 : x > 76 ? 76 : x;
}

/* A picture is shown a row above its card, kept on the screen. */
static int clamp_y(int y)
{
    return y < 1 ? 0 : y > 38 ? 37 : y - 1;
}

static void show_first(uint8_t i)
{
    const struct card *c = &p.cards[i];

    p.ids[ID_FIRST] = create(PICTURES + 12ul * c->picture, SPRITE_MODE_OPAQUE, 2, clamp_x(c->board_x),
                             clamp_y(c->board_y));
}

/* The second picture, pushed off the first when they would overlap; what
   the screen's edge takes off the push is given to the first instead. */
static void show_second(void)
{
    const struct card *a = &p.cards[p.first], *b = &p.cards[p.cursor];
    int x1 = clamp_x(a->board_x), y1 = clamp_y(a->board_y);
    int x2 = clamp_x(b->board_x), y2 = clamp_y(b->board_y);
    int dx = 8, dy = 11;

    if (x2 > x1 && x2 - x1 < 8)
        dx = 8 - (x2 - x1);
    else if (x2 < x1 && x1 - x2 < 8)
        dx = (x1 - x2) - 8;
    else if (x2 == x1)
        dx = 0;
    if (y2 > y1 && y2 - y1 < 11)
        dy = 11 - (y2 - y1);
    else if (y2 < y1 && y1 - y2 < 11)
        dy = (y1 - y2) - 11;
    else if (y2 == y1)
        dy = 0;
    if ((dx < 0 ? -dx : dx) < 8 && (dy < 0 ? -dy : dy) < 11) {
        int nx = clamp_x(dx + x2), ny = clamp_y(dy + y2 + 1);

        if (nx - x2 == dx) {
            x2 += dx;
        } else {
            x1 = (nx - x2) + x1 - dx;
            x2 = nx;
        }
        if (ny - y2 == dy) {
            y2 += dy;
        } else {
            y1 = (ny - y2) + y1 - dy;
            y2 = ny;
        }
        sprite_move(p.ids[ID_FIRST], x1, y1);
    }
    p.ids[ID_SECOND] = create(PICTURES + 12ul * b->picture, SPRITE_MODE_OPAQUE, 2, x2, y2);
}

static uint8_t all_face_up(void)
{
    uint8_t i;

    for (i = 0; i < p.count; i++)
        if (!p.cards[i].up)
            return 0;
    return 1;
}

static void after_match(void)
{
    uint8_t i;

    if (!all_face_up()) {
        p.open = 3;
        cursor_row(1);
    } else {
        if (p.mode == PAIRS2_TIME_TRIAL) {
            free_id(p.ids[ID_CURSOR]);
            for (i = 0; i < p.count; i++) {
                p.cards[i].board_x = p.cards[i].x;
                p.cards[i].board_y = p.cards[i].y;
            }
            p.phase = PHASE_CLEARED;
        } else {
            p.phase = PHASE_DONE;
            p.running = 0;
        }
        p.counter = 0;
    }
    p.open = 0;
}

static void open_first(void)
{
    if (p.cards[p.cursor].up)
        return;
    show_first(p.cursor);
    p.cards[p.cursor].up = 1;
    p.first = p.cursor;
    p.open = 3;
    cursor_row(1);
    p.open = 1;
}

/* Returns whether the two make a pair. */
static uint8_t open_second(void)
{
    struct card *b = &p.cards[p.cursor], *a = &p.cards[p.first];

    if (b->state == CARD_GONE || b->up)
        return 0;
    show_second();
    b->up = 1;
    if (b->picture != a->picture) {
        p.open = 2;
        return 0;
    }
    if (p.mode == PAIRS2_PUZZLE) {
        b->state = 0;
        a->state = 0;
    } else {
        free_id(p.sprites[p.cursor]);
        free_id(p.sprites[p.first]);
        b->state = CARD_GONE;
        a->state = CARD_GONE;
    }
    after_match();
    free_id(p.ids[ID_FIRST]);
    free_id(p.ids[ID_SECOND]);
    return 1;
}

/* Time trial's fuse: a pixel shorter every so many ticks, the flame on
   its end flickering. */
static void fuse_step(void)
{
    if (p.time % p.divisor == 0)
        p.fuse_top++;
    p.flame ^= 1;
    set_image(p.ids[ID_FLAME], FLAME + 12ul * p.flame);
    free_id(p.ids[ID_FUSE]);
    p.ids[ID_FUSE] = (uint8_t)sprite_create_line(SPRITE_MODE_SET, 3, 4, 30, 4, p.fuse_top);
    sprite_move(p.ids[ID_FLAME], 0, p.fuse_top - 5);
}

/* The play period of each level, ms. */
static const uint16_t periods[PAIRS2_LEVELS] = { 300, 267, 233, 200, 167, 133, 100 };

static int tick(struct game_context *ctx)
{
    int result = GAME_RESULT_REDRAW;
    uint8_t i, running;

    if ((p.phase == PHASE_PLAY || p.phase == PHASE_DONE) && p.mode == PAIRS2_PUZZLE) {
        if (ctx->period != 200) {
            p.running = 0;
            ctx->period = 200;
            result = GAME_RESULT_RESTART_TICK;
        }
        for (i = 0; i < p.count; i++) {
            struct card *c = &p.cards[i];

            if (c->state == CARD_ON_BOARD || c->state == CARD_GONE)
                continue;
            if (c->state == CARD_REMOVED) {
                c->state = CARD_GONE;
                free_id(p.sprites[i]);
                if (p.phase == PHASE_DONE)
                    result = GAME_RESULT_GAME_OVER;
            } else {
                set_image(p.sprites[i], REMOVAL + 12ul * c->state);
                c->state++;
            }
        }
    }
    if (p.phase == PHASE_COLLECT || p.phase == PHASE_DEAL) {
        if (!deal_step()) {
            if (p.phase == PHASE_COLLECT) {
                p.phase = PHASE_NEXT;
                p.counter = 10;
            } else {
                p.phase = PHASE_DOOR;
                p.counter = 10;
                if (p.mode == PAIRS2_PUZZLE) {
                    p.phase = PHASE_PLAY;
                    p.counter = 0;
                    p.ids[ID_CURSOR] = create(CURSOR, SPRITE_MODE_XOR, 2, p.cards[p.cursor].board_x,
                                              p.cards[p.cursor].board_y);
                }
            }
        }
        result = GAME_RESULT_REDRAW;
    }
    if (p.mode != PAIRS2_TIME_TRIAL)
        return result;

    if (p.phase >> 1 == 0) {
        /* The door's and the next board's countdowns. */
        if (--p.counter == 0) {
            free_id(p.ids[ID_DOOR]);
            result = GAME_RESULT_REDRAW;
            if (p.phase == PHASE_NEXT) {
                if (p.board == BOARDS - 1) {
                    result = GAME_RESULT_GAME_OVER;
                } else {
                    free_id(p.ids[ID_BACK]);
                    p.board++;
                    ctx->score += p.time >> 1;
                    board_time_trial();
                }
            } else {
                p.phase = PHASE_WIPE;
                p.counter = 0;
            }
        }
    }
    if (p.phase == PHASE_WIPE) {
        if (p.counter < 48) {
            uint8_t k = (uint8_t)(p.counter / 6);

            p.ids[ID_STRIPS + k] = create(STRIP_TOP, SPRITE_MODE_CLEAR_BACKGROUND, 0, 0, p.counter);
            p.ids[ID_STRIPS + 8 + k] = create(STRIP_BOTTOM, SPRITE_MODE_CLEAR_BACKGROUND, 0, 0, 48 - p.counter);
            p.wipe[k] = 1;
        } else {
            p.phase = PHASE_PLAY;
            for (i = 0; i < 16; i++)
                free_id(p.ids[ID_STRIPS + i]);
            memset(p.wipe, 0, sizeof p.wipe);
            sprite_set_mode(p.ids[ID_BACK], SPRITE_MODE_HIDDEN);
            p.ids[ID_FUSE] = (uint8_t)sprite_create_line(SPRITE_MODE_SET, 2, 4, 30, 4, p.fuse_top);
            p.ids[ID_FLAME] = create(FLAME, SPRITE_MODE_SET, 0, 0, p.fuse_top - 5);
            p.ids[ID_DYNAMITE] = create(DYNAMITE, SPRITE_MODE_OPAQUE, 2, 0, 30);
            p.ids[ID_CURSOR] = create(CURSOR, SPRITE_MODE_XOR, 2, p.cards[p.cursor].board_x, p.cards[p.cursor].board_y);
        }
        p.counter = (uint8_t)(p.counter + 6);
        result = GAME_RESULT_REDRAW;
    } else if (p.phase == PHASE_CLEARED) {
        free_id(p.ids[ID_FUSE]);
        free_id(p.ids[ID_FLAME]);
        free_id(p.ids[ID_DYNAMITE]);
        sprite_set_mode(p.ids[ID_BACK], SPRITE_MODE_OPAQUE);
        p.phase = PHASE_COLLECT;
        result = GAME_RESULT_REDRAW;
    } else if (p.phase == PHASE_TIME_OUT) {
        /* Two pictures of an explosion, two ticks each. */
        if (p.counter == 3) {
            free_id(p.ids[ID_FUSE]);
            free_id(p.ids[ID_FLAME]);
            free_id(p.ids[ID_DYNAMITE]);
            free_id(p.ids[ID_BACK]);
            p.ids[ID_BACK] = create(EXPLOSION + 12ul * (3 >> 1), SPRITE_MODE_OPAQUE, 2, 0, 0);
        }
        set_image(p.ids[ID_BACK], EXPLOSION + 12ul * (p.counter >> 1));
        result = p.counter == 0 ? GAME_RESULT_GAME_OVER : GAME_RESULT_REDRAW;
        p.counter--;
    }

    running = p.running;
    if (running == 1 && p.phase == PHASE_PLAY && result != GAME_RESULT_GAME_OVER) {
        p.time--;
        fuse_step();
        if (p.time == 0) {
            p.phase = PHASE_TIME_OUT;
            p.counter = 3;
        }
        running = p.running;
        result = GAME_RESULT_REDRAW;
    }
    if (running == 0 && p.phase == PHASE_PLAY) {
        ctx->period = periods[p.level - 1];
        p.running = 1;
        return GAME_RESULT_RESTART_TICK;
    }
    if (running == 1 && p.phase != PHASE_PLAY) {
        ctx->period = 200;
        p.running = 0;
        return GAME_RESULT_RESTART_TICK;
    }
    return result;
}

static int new_game(struct game_context *ctx)
{
    ctx->period = 200;
    ctx->score = 0;
    memset(&p, 0, sizeof p);
    p.board = 0;
    p.mode = ctx->option;
    p.level = ctx->level;
    sprite_reset(SPRITE_COUNT);
    if (p.mode == PAIRS2_TIME_TRIAL)
        board_time_trial();
    else
        board_puzzle();
    return GAME_RESULT_NONE;
}

/* Back from the pause menu: the phone made its sprites afresh. */
static int resume(struct game_context *ctx)
{
    uint8_t i;

    sprite_reset(SPRITE_COUNT);
    if (p.mode == PAIRS2_PUZZLE) {
        p.ids[ID_BACK] = create(PUZZLE_PICTURE, SPRITE_MODE_OPAQUE, 0, 0, 0);
        if (p.phase == PHASE_DONE)
            return GAME_RESULT_GAME_OVER;
    } else {
        p.ids[ID_BACK] = create(SALOON, SPRITE_MODE_HIDDEN, 0, 0, 0);
        if (p.phase == PHASE_CLEARED || p.phase == PHASE_COLLECT || p.phase == PHASE_NEXT) {
            /* Between boards: the next one, or the end. */
            if (p.board == BOARDS - 1)
                return GAME_RESULT_GAME_OVER;
            p.board++;
            ctx->score += p.time >> 1;
            board_time_trial();
            return GAME_RESULT_NONE;
        }
        p.ids[ID_FUSE] = (uint8_t)sprite_create_line(SPRITE_MODE_SET, 3, 4, 30, 4, p.fuse_top);
        p.ids[ID_FLAME] = create(FLAME, SPRITE_MODE_OPAQUE, 1, 0, p.fuse_top);
        p.ids[ID_DYNAMITE] = create(DYNAMITE, SPRITE_MODE_OPAQUE, 2, 0, 30);
        sprite_move(p.ids[ID_FLAME], 0, p.fuse_top - 5);
    }
    if (p.phase != PHASE_TIME_OUT)
        p.phase = PHASE_PLAY;
    for (i = 0; i < p.count; i++) {
        struct card *c = &p.cards[i];

        if (c->state == CARD_ON_BOARD)
            p.sprites[i] = create(c->up ? PICTURES + 12ul * c->picture : CARD_BACK, SPRITE_MODE_OPAQUE, 2,
                                  c->board_x, c->board_y);
        else
            c->state = CARD_GONE;
    }
    p.ids[ID_CURSOR] = create(CURSOR, SPRITE_MODE_XOR, 2, 0, 0);
    move_cursor();
    return GAME_RESULT_NONE;
}

int pairs2_handler(int event, struct game_context *ctx)
{
    if (event == GAME_EVENT_START)
        return new_game(ctx);
    switch (event) {
    case GAME_EVENT_TICK:
        return tick(ctx);
    case GAME_EVENT_RESUME:
        return resume(ctx);
    case GAME_KEY_2:
    case GAME_KEY_8:
    case GAME_KEY_4:
    case GAME_KEY_6:
        if (p.phase != PHASE_PLAY)
            return GAME_RESULT_NONE;
        if (event == GAME_KEY_2 || event == GAME_KEY_8)
            cursor_column(event == GAME_KEY_8);
        else
            cursor_row(event == GAME_KEY_6);
        return GAME_RESULT_REDRAW;
    case GAME_KEY_5:
        if (p.phase != PHASE_PLAY)
            return GAME_RESULT_NONE;
        if (p.open == 0) {
            open_first();
            return GAME_RESULT_REDRAW;
        }
        if (p.open == 1) {
            if (open_second()) {
                ctx->sound = PAIRS2_SOUND_PAIR;
                ctx->score += (uint32_t)p.level + 4;
            } else {
                if (ctx->score)
                    ctx->score--;
                ctx->sound = PAIRS2_SOUND_MISS;
            }
            return GAME_RESULT_SOUND;
        }
        if (p.open == 2) {
            cursor_row(1);
            p.open = 0;
        }
        return GAME_RESULT_REDRAW;
    default:
        return GAME_RESULT_NONE;
    }
}

void pairs2_render(void)
{
    sprite_render();
}
