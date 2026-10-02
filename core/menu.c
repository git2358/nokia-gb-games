#include "menu.h"

#include "font.h"
#include "game.h"
#include "game_assets.h"
#include "games.h"
#include "lcd.h"
#include "rand.h"
#include "si.h"

/* Screen geometry shared by the phone's menu pages. */
#define CONTENT_WIDTH 78 /* left of the scrollbar */
#define HEADER_Y 0
#define LIST_Y 7
#define ROW_HEIGHT 10
#define VISIBLE_ROWS 3
#define SCROLLBAR_X 81
#define SCROLLBAR_HEIGHT 30
#define THUMB_HEIGHT 7
#define SOFTKEY_Y 40

#define GAMES_MENU_NUMBER 8 /* Games is entry 8 of the phone's main menu */
#define GAMES_MENU_THUMB 14 /* where that puts the main menu's scrollbar */

enum {
    SCREEN_MAIN,
    SCREEN_GAMES,
    SCREEN_GAME,
    SCREEN_TOP_SCORE,
    SCREEN_HELP,
    SCREEN_PLAY,
    SCREEN_GAME_OVER
};

#define HELP_Y 7
#define HELP_LINE_HEIGHT 9
#define HELP_LINES 3

/* Microseconds per scheduler tick of the phone. */
#define PHONE_TICK_US 7781

/* The Top score and Game over pages close by themselves: after 768 and
   385 phone ticks, about six and three seconds. In menu_tick calls. */
#define TOP_SCORE_TICKS ((uint16_t)(768ul * PHONE_TICK_US / MENU_FRAME_US))
#define GAME_OVER_TICKS ((uint16_t)(385ul * PHONE_TICK_US / MENU_FRAME_US))

/* The top score before anyone has played. The firmware has no default of
   its own: the 4075 MAME shows is a score saved in the PMM dump. */
#define DEFAULT_TOP_SCORE 0

/* The Top score page's animation in its top right corner: stars gather
   into a cup, which then flashes. A new picture every 25 phone ticks; the
   last one stays. */
#define SPARKLE_X 63
#define SPARKLE_WIDTH 21
#define SPARKLE_HEIGHT 24
#define SPARKLE_FRAME_BYTES 84
#define SPARKLE_TICKS 25
static const uint8_t sparkle_frames[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 10, 10, 9, 9, 10, 10, 9, 10 };

/* Space Impact's menu. Continue is there only while a game is paused. */
enum {
    ITEM_CONTINUE,
    ITEM_NEW_GAME,
    ITEM_TOP_SCORE,
    ITEM_INSTRUCTIONS
};

/* The phone's list of games ends with a Settings entry. */
#define LIST_COUNT (GAME_COUNT + 1)

#define NO_KEY 0xff

static uint8_t screen;
static uint8_t game;     /* selection in the list of games */
static uint8_t game_top; /* first visible row of that list */
static uint8_t item;     /* selection in the game's menu, counting visible items */
static uint8_t item_top; /* first visible row of the game's menu */
static uint8_t paused;   /* a game is waiting behind the menu */
static uint8_t new_top_score;  /* the game just ended beat the top score */
static uint16_t final_score;   /* its score */
static uint8_t held = NO_KEY;  /* the button the game sees as held */
static uint8_t seed_fixed;     /* games start from fixed_seed, not from the time */
static uint16_t fixed_seed;
static uint16_t play_us;       /* time not yet turned into phone ticks */
static uint16_t uptime;        /* menu_tick calls so far; seeds the games' generator */
static uint8_t board_drawn;    /* the LCD holds the running game's picture */
/* The full-screen menu the LCD holds and the selection drawn on it;
   NO_SCREEN when it holds something else. */
#define NO_SCREEN 0xff
static uint8_t drawn_screen = NO_SCREEN, drawn_selection;
static uint8_t full_screen;    /* the full-screen variant was chosen */
static uint8_t view_mode;      /* which of the views below the LCD is set up for */

/* Columns of the phone's LCD a platform with LCD_GAME_ZOOM shows; what
   does not fit is left off on the right. That costs Space Impact nothing
   it needs: its score ends at column 75, and what is cut is where enemies
   come on and the last columns the ship can fly into. The game is drawn
   with its first column on a whole cell of the framebuffer, GAME_ZOOM_X,
   which is what a platform can magnify. */
#define GAME_ZOOM_WIDTH (LCD_FB_WIDTH / LCD_GAME_ZOOM < LCD_WIDTH ? LCD_FB_WIDTH / LCD_GAME_ZOOM : LCD_WIDTH)
#define GAME_ZOOM_X ((LCD_FB_WIDTH - GAME_ZOOM_WIDTH) / 2 / 8 * 8)

/* The phone's LCD; the full-screen variant's own menus. */
enum {
    VIEW_PHONE,
    VIEW_NATIVE
};

/* The full-screen variant's menus: a title bar, a list with every entry
   visible, and a line of button hints. The port's own design and words; the
   entries and their text are the phone's. A screen with room uses the
   phone's large font (13 rows: 10 above the baseline, 3 below); a small
   one, shown magnified by its platform, uses the phone's menu fonts. */
#if LCD_FB_HEIGHT >= 120
#define NATIVE_FONT font_large_bold
#define NATIVE_BODY_FONT font_large_bold
#define NATIVE_TITLE_HEIGHT 15
#define NATIVE_LIST_Y 21
#define NATIVE_ROW_HEIGHT 15
#define NATIVE_HINT_Y (LCD_FB_HEIGHT - 14)
#define NATIVE_MARGIN 4
#define NATIVE_CURSOR_HEIGHT 9
#define NATIVE_NOTE_Y 48
#define NATIVE_NOTE_PITCH 16
#else
#define NATIVE_FONT font_small_bold
#define NATIVE_BODY_FONT font_small_plain
#define NATIVE_TITLE_HEIGHT 9
#define NATIVE_LIST_Y 12
#define NATIVE_ROW_HEIGHT 10
#define NATIVE_HINT_Y (LCD_FB_HEIGHT - 9)
#define NATIVE_MARGIN 2
#define NATIVE_CURSOR_HEIGHT 7
#define NATIVE_NOTE_Y 24
#define NATIVE_NOTE_PITCH 11
#endif
#define NATIVE_CURSOR_WIDTH ((NATIVE_CURSOR_HEIGHT + 1) / 2)
#define NATIVE_TEXT_X (NATIVE_MARGIN + NATIVE_CURSOR_WIDTH + 5)
#define NATIVE_HELP_LINES ((NATIVE_HINT_Y - 3 - NATIVE_LIST_Y) / NATIVE_ROW_HEIGHT)

static const char text_hint_select[] = "B back   A select";
static const char text_hint_more[] = "B back   A more";
static uint8_t surround_used;  /* something is drawn around the phone's LCD */

#if LCD_HAS_SURROUND
/* Shown under the phone's LCD on the first screen. The port's own words. */
static const char text_full_screen_hint[] = "START: full screen";
#endif
static struct game_settings settings; /* Space Impact's top score */
static uint16_t page_ticks;   /* ticks left on a timed page */
static uint8_t sparkle_step;  /* picture of the Top score page's animation */
static uint8_t sparkle_ticks; /* phone ticks it has been shown */
static uint8_t sparkle_only;  /* nothing else on the page needs drawing */
static const char *help_page; /* first character of the Instructions page shown */

static void settings_load(void)
{
    if (!platform_settings_load(GAME_SPACE_IMPACT, &settings))
        settings.top_score = DEFAULT_TOP_SCORE;
}

static const char *list_name(uint8_t index)
{
    switch (index) {
    case GAME_SNAKE:
        return text_snake;
    case GAME_SPACE_IMPACT:
        return text_space_impact;
    case GAME_BANTUMI:
        return text_bantumi;
    case GAME_PAIRS:
        return text_pairs;
    default:
        return text_settings;
    }
}

static uint8_t item_count(void)
{
    return paused ? 4 : 3;
}

/* The item at visible position `index`. */
static uint8_t item_id(uint8_t index)
{
    return (uint8_t)(paused ? index : index + 1);
}

static const char *item_name(uint8_t id)
{
    switch (id) {
    case ITEM_CONTINUE:
        return text_continue;
    case ITEM_NEW_GAME:
        return text_new_game;
    case ITEM_TOP_SCORE:
        return text_top_score;
    default:
        return text_instructions;
    }
}

static void draw_scrollbar(uint8_t thumb)
{
    lcd_fill_rect(SCROLLBAR_X, LIST_Y, 1, SCROLLBAR_HEIGHT, 1);
    lcd_fill_rect(SCROLLBAR_X, LIST_Y + thumb, 3, THUMB_HEIGHT, 0);
    lcd_fill_rect(SCROLLBAR_X, LIST_Y + thumb, 2, 1, 1);
    lcd_fill_rect(SCROLLBAR_X + 2, LIST_Y + thumb + 1, 1, THUMB_HEIGHT - 2, 1);
    lcd_fill_rect(SCROLLBAR_X, LIST_Y + thumb + THUMB_HEIGHT - 1, 2, 1, 1);
}

/* Thumb position for entry `index` of `count`. */
static uint8_t thumb_for(uint8_t index, uint8_t count)
{
    return (uint8_t)(index * (SCROLLBAR_HEIGHT - THUMB_HEIGHT) / (count - 1));
}

/* The menu path in the top right corner, such as 8-2-1. */
static void draw_path(uint8_t depth)
{
    char path[6];
    uint8_t n = 0;

    path[n++] = '0' + GAMES_MENU_NUMBER;
    if (depth > 0) {
        path[n++] = '-';
        /* The phone numbers Settings 6: an entry before it is not shown. */
        path[n++] = (char)((game < GAME_COUNT ? '1' : '2') + game);
    }
    if (depth > 1) {
        path[n++] = '-';
        path[n++] = (char)('1' + item);
    }
    path[n] = 0;
    font_draw(&font_tiny_plain, LCD_WIDTH - font_text_width(&font_tiny_plain, path), HEADER_Y, path, 1);
}

static void draw_softkey(const char *label)
{
    font_draw(&font_small_bold, (LCD_WIDTH - font_text_width(&font_small_bold, label)) / 2, SOFTKEY_Y, label, 1);
}

static void draw_row(uint8_t row, const char *label, uint8_t selected)
{
    uint8_t y = (uint8_t)(LIST_Y + row * ROW_HEIGHT);

    if (selected)
        lcd_fill_rect(0, y, CONTENT_WIDTH, ROW_HEIGHT, 1);
    font_draw(&font_small_bold, 2, y + 1, label, !selected);
}

/* The hint for the full-screen variant, in the space under the phone's LCD. */
static void draw_hint(void)
{
#if LCD_HAS_SURROUND
    lcd_view_full();
    font_draw(&font_small_plain, (LCD_FB_WIDTH - font_text_width(&font_small_plain, text_full_screen_hint)) / 2,
              LCD_BELOW_PHONE + 8, text_full_screen_hint, 1);
    lcd_view_phone();
    surround_used = 1;
#endif
}

static void draw_main(void)
{
    draw_hint();
    draw_path(0);
    font_draw(&font_large_bold, (CONTENT_WIDTH - font_text_width(&font_large_bold, text_games)) / 2, LIST_Y, text_games, 1);
    lcd_blit_strips(10, 23, 64, 16, menu_games_icon);
    draw_scrollbar(GAMES_MENU_THUMB);
    draw_softkey(text_select);
}

static void draw_games(void)
{
    uint8_t row;

    draw_path(1);
    for (row = 0; row < VISIBLE_ROWS; row++) {
        uint8_t index = (uint8_t)((game_top + row) % LIST_COUNT);

        draw_row(row, list_name(index), index == game);
    }
    draw_scrollbar(thumb_for(game, LIST_COUNT));
    draw_softkey(text_select);
}

static void draw_game(void)
{
    uint8_t row;

    draw_path(2);
    for (row = 0; row < VISIBLE_ROWS; row++) {
        uint8_t index = (uint8_t)((item_top + row) % item_count());

        draw_row(row, item_name(item_id(index)), index == item);
    }
    draw_scrollbar(thumb_for(item, item_count()));
    draw_softkey(text_select);
}

static void native_title(const char *title)
{
    lcd_fill_rect(0, 0, LCD_FB_WIDTH, NATIVE_TITLE_HEIGHT, 1);
    font_draw(&NATIVE_FONT, NATIVE_MARGIN, 1, title, 0);
}

static void native_hint(const char *hint)
{
    lcd_fill_rect(0, NATIVE_HINT_Y - 3, LCD_FB_WIDTH, 1, 1);
    font_draw(&NATIVE_FONT, NATIVE_MARGIN, NATIVE_HINT_Y, hint, 1);
}

/* The selection is a cursor beside the entry, not an inverted row, so
   moving it changes only a few cells of the screen. */
static void native_cursor(uint8_t row, uint8_t on)
{
    uint8_t y = (uint8_t)(NATIVE_LIST_Y + row * NATIVE_ROW_HEIGHT + 2), i;

    lcd_fill_rect(NATIVE_MARGIN, y, NATIVE_CURSOR_WIDTH, NATIVE_CURSOR_HEIGHT, 0);
    if (on)
        for (i = 0; i < NATIVE_CURSOR_WIDTH; i++)
            lcd_fill_rect(NATIVE_MARGIN + i, y + i, 1, NATIVE_CURSOR_HEIGHT - 2 * i, 1);
}

static void native_row(uint8_t row, const char *label, uint8_t selected)
{
    font_draw(&NATIVE_FONT, NATIVE_TEXT_X, NATIVE_LIST_Y + row * NATIVE_ROW_HEIGHT + 1, label, 1);
    native_cursor(row, selected);
}

static void native_games(void)
{
    uint8_t i;

    native_title(text_games);
    for (i = 0; i < LIST_COUNT; i++)
        native_row(i, list_name(i), i == game);
    native_hint(text_hint_select);
}

static void native_game(void)
{
    uint8_t i;

    native_title(text_space_impact);
    for (i = 0; i < item_count(); i++)
        native_row(i, item_name(item_id(i)), i == item);
    native_hint(text_hint_select);
}

/* When only the selection moved on a full-screen menu, redraws just that
   and returns nonzero. */
static uint8_t native_update(void)
{
    uint8_t selection;

    if (drawn_screen != screen || (screen != SCREEN_GAMES && screen != SCREEN_GAME))
        return 0;
    selection = screen == SCREEN_GAMES ? game : item;
    native_cursor(drawn_selection, 0);
    native_cursor(selection, 1);
    drawn_selection = selection;
    return 1;
}

static void draw_number(const struct font *font, int x, int y, uint16_t value, uint8_t centred)
{
    char digits[6];
    uint8_t n = sizeof digits - 1;

    digits[n] = 0;
    do {
        digits[--n] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    if (centred)
        x = (LCD_FB_WIDTH - font_text_width(font, digits + n)) / 2;
    font_draw(font, x, y, digits + n, 1);
}

/* A note: its lines centred on the screen; %N is the number. */
static void native_note(const char *title, const char *text, uint16_t number)
{
    uint8_t y = NATIVE_NOTE_Y;

    if (title)
        native_title(title);
    for (;;) {
        if (text[0] == '%' && text[1] == 'N')
            draw_number(&NATIVE_FONT, 0, y, number, 1);
        else
            font_draw(&NATIVE_FONT, (LCD_FB_WIDTH - font_text_width(&NATIVE_FONT, text)) / 2, y, text, 1);
        while (*text && *text != '\n')
            text++;
        if (!*text++)
            break;
        y += NATIVE_NOTE_PITCH;
    }
}

static void draw_sparkle(void)
{
    lcd_blit_strips(SPARKLE_X, 0, SPARKLE_WIDTH, SPARKLE_HEIGHT,
                    top_score_sparkle + sparkle_frames[sparkle_step] * SPARKLE_FRAME_BYTES);
}

/* A note in the large font, one line every 15 rows; %N is the number. */
static void draw_note(const char *text, uint16_t number)
{
    uint8_t y = 3;

    for (;;) {
        if (text[0] == '%' && text[1] == 'N')
            draw_number(&font_large_bold, 0, y, number, 0);
        else
            font_draw(&font_large_bold, 0, y, text, 1);
        while (*text && *text != '\n')
            text++;
        if (!*text++)
            break;
        y += 15;
    }
}

static const struct font *help_font(void)
{
    return full_screen ? &NATIVE_BODY_FONT : &font_small_plain;
}

static uint8_t help_lines(void)
{
    return full_screen ? NATIVE_HELP_LINES : HELP_LINES;
}

/* Returns the start of the line after the one starting at `text`: as many
   whole words as fit across the screen. */
static const char *help_next_line(const char *text)
{
    const char *end = text, *p = text;
    uint8_t limit = full_screen ? LCD_FB_WIDTH - 2 * NATIVE_MARGIN : LCD_WIDTH;
    uint8_t width = 0;

    for (;;) {
        while (*p && *p != ' ')
            width += font_char_width(help_font(), *p++);
        if (width > limit && end != text)
            break;
        end = p;
        if (!*p)
            return p;
        width += font_char_width(help_font(), *p++);
    }
    return end + 1; /* skip the space the line broke at */
}

static void draw_help_line(int x, int y, const char *text, const char *end)
{
    for (; text != end && *text; text++) {
        char one[2];

        one[0] = *text;
        one[1] = 0;
        x = font_draw(help_font(), x, y, one, 1);
    }
}

static void draw_help(void)
{
    const char *line = help_page;
    uint8_t row;

    if (full_screen)
        native_title(text_instructions);
    for (row = 0; row < help_lines() && *line; row++) {
        const char *next = help_next_line(line);

        if (full_screen)
            draw_help_line(NATIVE_MARGIN, NATIVE_LIST_Y + row * NATIVE_ROW_HEIGHT, line, next);
        else
            draw_help_line(0, HELP_Y + row * HELP_LINE_HEIGHT, line, next);
        line = next;
    }
    if (full_screen)
        native_hint(text_hint_more);
    else
        draw_softkey(text_more);
}

/* More: the next page, or the first one again after the last. */
static void help_more(void)
{
    uint8_t row;

    for (row = 0; row < help_lines() && *help_page; row++)
        help_page = help_next_line(help_page);
    if (!*help_page)
        help_page = text_help_space_impact;
}

void menu_init(void)
{
    screen = SCREEN_MAIN;
    game = game_top = 0;
    paused = 0;
    full_screen = 0;
    held = NO_KEY;
    settings_load();
}

/* Opens the game's menu on its first entry: Continue when a game is
   paused, else New game. */
static void game_menu_open(void)
{
    screen = SCREEN_GAME;
    item = item_top = 0;
}

static void play_over(void)
{
    final_score = (uint16_t)games_score;
    new_top_score = final_score > settings.top_score;
    if (new_top_score) {
        settings.top_score = final_score;
        platform_settings_save(GAME_SPACE_IMPACT, &settings);
    }
    paused = 0;
    held = NO_KEY;
    screen = SCREEN_GAME_OVER;
    page_ticks = GAME_OVER_TICKS;
}

static void play_start(void)
{
    /* The phone seeds the games' generator from its clock; here the time
       of the choice does the same. */
    game_rand16_seed = seed_fixed ? fixed_seed : (uint16_t)(uptime % 0xfff0 + 1);
    held = NO_KEY;
    games_start();
    board_drawn = 0;
    screen = SCREEN_PLAY;
}

/* The phone key a button is in the game, or 0. */
static uint8_t play_phone_key(uint8_t key)
{
    switch (key) {
    case MENU_KEY_UP:
        return SI_KEY_8;
    case MENU_KEY_DOWN:
        return SI_KEY_0;
    case MENU_KEY_LEFT:
        return SI_KEY_STAR;
    case MENU_KEY_RIGHT:
        return SI_KEY_HASH;
    case MENU_KEY_SELECT:
        return SI_KEY_1;
    case MENU_KEY_BACK:
        return SI_KEY_4;
    default:
        return 0;
    }
}

static uint8_t play_key(uint8_t key)
{
    uint8_t phone_key = play_phone_key(key), changed;

    if (!phone_key) {
        /* Start and Select pause into the game's menu, as the phone's Navi
           key does. */
        games_key_up();
        held = NO_KEY;
        paused = 1;
        game_menu_open();
        return 1;
    }
    held = key;
    changed = games_key_down(phone_key);
    if (games_over)
        play_over();
    return changed;
}

uint8_t menu_held(uint8_t keys)
{
    uint8_t key, changed = 0;

    if (screen != SCREEN_PLAY || held == NO_KEY || (keys & (1 << held)))
        return 0;
    /* The held button came up; another one still down takes over. */
    games_key_up();
    held = NO_KEY;
    for (key = MENU_KEY_UP; key <= MENU_KEY_RIGHT && held == NO_KEY; key++) {
        if ((keys & (1 << key)) && play_phone_key(key)) {
            held = key;
            changed = games_key_down(play_phone_key(key));
        }
    }
    if (games_over) {
        play_over();
        return 1;
    }
    return changed;
}

static void game_menu_select(void)
{
    switch (item_id(item)) {
    case ITEM_CONTINUE:
        paused = 0;
        board_drawn = 0;
        screen = SCREEN_PLAY;
        break;
    case ITEM_NEW_GAME:
        paused = 0;
        play_start();
        break;
    case ITEM_TOP_SCORE:
        screen = SCREEN_TOP_SCORE;
        page_ticks = TOP_SCORE_TICKS;
        play_us = 0;
        sparkle_step = sparkle_ticks = 0;
        break;
    default:
        screen = SCREEN_HELP;
        help_page = text_help_space_impact;
        break;
    }
}

/* Moves a selection through a list of `count` that shows three rows; the
   window follows the selection, wrapping with it. */
static void list_move(uint8_t key, uint8_t count, uint8_t *selection, uint8_t *top)
{
    if (key == MENU_KEY_DOWN) {
        *selection = (uint8_t)((*selection + 1) % count);
        if ((uint8_t)((*selection + count - *top) % count) >= VISIBLE_ROWS)
            *top = (uint8_t)((*selection + count - (VISIBLE_ROWS - 1)) % count);
    } else if (key == MENU_KEY_UP) {
        *selection = (uint8_t)((*selection + count - 1) % count);
        if ((uint8_t)((*selection + count - *top) % count) >= VISIBLE_ROWS)
            *top = *selection;
    }
}

static uint8_t handle_key(uint8_t key)
{
    /* Start picks the full-screen variant on the first screen, where there
       is one. Outside the game it and the console's Select button are more
       Navi keys. */
    uint8_t start = key == MENU_KEY_START;

    if (screen == SCREEN_PLAY)
        return play_key(key);
    if (start || key == MENU_KEY_ALT)
        key = MENU_KEY_SELECT;
    switch (screen) {
    case SCREEN_MAIN:
        if (key == MENU_KEY_SELECT) {
            full_screen = start && LCD_HAS_SURROUND;
            screen = SCREEN_GAMES;
            game = game_top = 0;
        }
        break;
    case SCREEN_GAMES:
        if (key == MENU_KEY_DOWN || key == MENU_KEY_UP) {
            list_move(key, LIST_COUNT, &game, &game_top);
        } else if (key == MENU_KEY_SELECT) {
            /* Only Space Impact is here so far. */
            if (game == GAME_SPACE_IMPACT)
                game_menu_open();
        } else if (key == MENU_KEY_BACK) {
            screen = SCREEN_MAIN;
        }
        break;
    case SCREEN_HELP:
        if (key == MENU_KEY_SELECT)
            help_more();
        else if (key == MENU_KEY_BACK)
            screen = SCREEN_GAME;
        break;
    case SCREEN_TOP_SCORE:
        /* Any key closes the page; all but C then act on the menu under it. */
        screen = SCREEN_GAME;
        if (key != MENU_KEY_BACK)
            handle_key(key);
        break;
    case SCREEN_GAME_OVER:
        game_menu_open();
        break;
    default:
        if (key == MENU_KEY_DOWN || key == MENU_KEY_UP)
            list_move(key, item_count(), &item, &item_top);
        else if (key == MENU_KEY_SELECT)
            game_menu_select();
        else if (key == MENU_KEY_BACK)
            screen = SCREEN_GAMES;
        break;
    }
    return 0;
}

uint8_t menu_key(uint8_t key)
{
    uint8_t was_screen = screen, was_game = game, was_item = item, was_top = item_top;
    const char *was_page = help_page;
    uint8_t changed;

    sparkle_only = 0;
    changed = handle_key(key);
    return changed || screen != was_screen || game != was_game || item != was_item || item_top != was_top
           || help_page != was_page;
}

void menu_seed(uint16_t seed)
{
    fixed_seed = seed;
    seed_fixed = 1;
}

void menu_redraw_all(void)
{
    sparkle_only = 0;
    drawn_screen = NO_SCREEN;
    board_drawn = 0;
}

uint8_t menu_tick(void)
{
    uint8_t changed = 0;

    uptime++;
    switch (screen) {
    case SCREEN_TOP_SCORE:
        if (--page_ticks == 0) {
            screen = SCREEN_GAME;
            return 1;
        }
        play_us += MENU_FRAME_US;
        while (play_us >= PHONE_TICK_US) {
            play_us -= PHONE_TICK_US;
            if (++sparkle_ticks == SPARKLE_TICKS) {
                sparkle_ticks = 0;
                if (!full_screen && sparkle_step < sizeof sparkle_frames - 1) {
                    sparkle_step++;
                    sparkle_only = 1;
                    changed = 1;
                }
            }
        }
        break;
    case SCREEN_GAME_OVER:
        if (--page_ticks == 0) {
            game_menu_open();
            return 1;
        }
        break;
    case SCREEN_PLAY:
        changed = games_elapse(MENU_FRAME_US);
        if (games_over) {
            play_over();
            return 1;
        }
        break;
    default:
        break;
    }
    return changed;
}

void menu_draw(void)
{
    /* The full-screen variant has its own menus over the whole framebuffer;
       all else is drawn in the phone's LCD. Changing between them, or
       leaving a screen that drew around the LCD, clears everything. */
    uint8_t mode = full_screen && screen != SCREEN_MAIN && screen != SCREEN_PLAY ? VIEW_NATIVE : VIEW_PHONE;

    if (mode != view_mode || surround_used) {
        lcd_view_full();
        lcd_clear();
        view_mode = mode;
        surround_used = 0;
        board_drawn = 0;
    }
    /* A platform that magnifies shows the phone's LCD bigger, the game in
       the full-screen variant bigger still where it can, and the
       full-screen menus as they are. */
    if (mode == VIEW_PHONE) {
        lcd_view_phone();
        if (LCD_GAME_ZOOM > LCD_ZOOM && full_screen && screen != SCREEN_MAIN) {
            /* The game in the full-screen variant: as much of the LCD as
               fits across the screen. */
            lcd_view_set(GAME_ZOOM_X, LCD_PHONE_Y, LCD_WIDTH, LCD_HEIGHT);
            lcd_zoom_set(GAME_ZOOM_X, LCD_PHONE_Y, GAME_ZOOM_WIDTH, LCD_HEIGHT, LCD_GAME_ZOOM);
        } else if (LCD_ZOOM > 1) {
            lcd_zoom_set(LCD_PHONE_X, LCD_PHONE_Y, LCD_WIDTH, LCD_HEIGHT, LCD_ZOOM);
        } else {
            lcd_zoom_set(0, 0, 0, 0, 1);
        }
    } else {
        lcd_view_full();
        lcd_zoom_set(0, 0, 0, 0, 1);
    }

    if (mode == VIEW_NATIVE && native_update())
        return;
    /* A step of the Top score page's animation leaves the rest as it is. */
    if (sparkle_only) {
        sparkle_only = 0;
        if (screen == SCREEN_TOP_SCORE && !full_screen) {
            draw_sparkle();
            return;
        }
    }
    drawn_screen = NO_SCREEN;

    if (screen != SCREEN_PLAY) {
        lcd_clear();
        board_drawn = 0;
    }
    switch (screen) {
    case SCREEN_MAIN:
        draw_main();
        break;
    case SCREEN_GAMES:
        if (full_screen)
            native_games();
        else
            draw_games();
        break;
    case SCREEN_TOP_SCORE:
        if (full_screen) {
            native_note(text_top_score, "%N", settings.top_score);
        } else {
            draw_note(text_top_score_value, settings.top_score);
            draw_sparkle();
        }
        break;
    case SCREEN_GAME_OVER:
        if (full_screen)
            native_note(0, new_top_score ? text_game_over_top_score : text_game_over_score, final_score);
        else
            draw_note(new_top_score ? text_game_over_top_score : text_game_over_score, final_score);
        break;
    case SCREEN_PLAY:
        games_draw(!board_drawn);
        board_drawn = 1;
        break;
    case SCREEN_HELP:
        draw_help();
        break;
    default:
        if (full_screen)
            native_game();
        else
            draw_game();
        break;
    }
    if (mode == VIEW_NATIVE) {
        drawn_screen = screen;
        drawn_selection = screen == SCREEN_GAMES ? game : item;
    }
}

void menu_script(const char *keys)
{
    uint8_t n;

    for (; *keys; keys++) {
        menu_draw();
        switch (*keys) {
        case 'p':
            menu_init();
            break;
        case '1': case '2': case '3': case '4': case '5': case '6': case '7':
            si_first_level = (uint8_t)(*keys - '0');
            break;
        case 'w':
        case 't':
            for (n = *keys == 'w' ? MENU_TICKS_PER_SECOND : MENU_TICKS_PER_SECOND / 10; n; n--)
                if (menu_tick())
                    menu_draw();
            break;
        default:
            menu_key(*keys == 'u' ? MENU_KEY_UP : *keys == 'd' ? MENU_KEY_DOWN : *keys == 'l' ? MENU_KEY_LEFT
                     : *keys == 'r' ? MENU_KEY_RIGHT : *keys == 's' ? MENU_KEY_SELECT
                     : *keys == 'a' ? MENU_KEY_START : *keys == 'e' ? MENU_KEY_ALT
                     : MENU_KEY_BACK);
            /* The scripted button is let go at once. */
            menu_held(0);
            break;
        }
    }
}
