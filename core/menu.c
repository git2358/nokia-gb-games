#include "menu.h"

#include "font.h"
#include "game.h"
#include "game_assets.h"
#include "lcd.h"
#include "rand.h"
#include "snake.h"

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

#define GAMES_MENU_NUMBER 6 /* Games is entry 6 of the phone's main menu */
#define GAMES_MENU_THUMB 12 /* where that puts the main menu's scrollbar */

enum {
    SCREEN_MAIN,
    SCREEN_GAMES,
    SCREEN_GAME,
    SCREEN_LEVEL,
    SCREEN_TOP_SCORE,
    SCREEN_HELP,
    SCREEN_PLAY,
    SCREEN_GAME_OVER,
    SCREEN_LAST_VIEW
};

#define LEVEL_COUNT 9
#define LEVEL_BASE_Y 36 /* bottom row of the level bars */

#define HELP_Y 7
#define HELP_LINE_HEIGHT 9
#define HELP_LINES 3

/* The Top score and Game over pages close by themselves after this long. */
#define NOTE_TICKS (5 * MENU_TICKS_PER_SECOND)

/* Microseconds per menu_tick call and per scheduler tick of the phone. */
#define FRAME_US (1000000ul / MENU_TICKS_PER_SECOND)
#define PHONE_TICK_US 7781

/* A game's menu in item-number order. The second entry is there only when
   a game is paused (Continue) or has just ended (Last view). */
enum {
    ITEM_LEVEL,
    ITEM_RESUME,
    ITEM_NEW_GAME,
    ITEM_TOP_SCORE,
    ITEM_INSTRUCTIONS
};

enum {
    RESUME_NONE,
    RESUME_CONTINUE,
    RESUME_LAST_VIEW
};

#define GAME_COUNT 3

static uint8_t screen;
static uint8_t game;     /* selection in the list of games */
static uint8_t item;     /* selection in a game's menu, counting visible items */
static uint8_t item_top; /* first visible row of a game's menu */
static uint8_t resume;   /* what the menu's second entry is */
static uint8_t new_top_score;  /* the game just ended beat the top score */
static uint16_t play_ticks;    /* phone ticks until Snake's next move */
static uint16_t play_us;       /* time not yet turned into phone ticks */
static uint16_t uptime;        /* menu_tick calls so far; seeds rand */
static struct game_settings settings; /* Snake's level and top score */
static uint8_t level_choice;  /* level shown on the Level page */
static uint16_t page_ticks;   /* ticks left on a timed page */
static const char *help_page; /* first character of the Instructions page shown */

static const char *game_name(uint8_t index)
{
    switch (index) {
    case 0:
        return text_rotation;
    case 1:
        return text_snake;
    default:
        return text_memory;
    }
}

static uint8_t item_count(void)
{
    return resume ? 5 : 4;
}

/* The item at visible position `index`. */
static uint8_t item_id(uint8_t index)
{
    return (uint8_t)(resume || index == 0 ? index : index + 1);
}

static const char *item_name(uint8_t id)
{
    switch (id) {
    case ITEM_LEVEL:
        return text_level;
    case ITEM_RESUME:
        return resume == RESUME_CONTINUE ? text_continue : text_last_view;
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

/* The menu path in the top right corner, such as 6-1-2. */
static void draw_path(uint8_t depth)
{
    char path[6];
    uint8_t n = 0;

    path[n++] = '0' + GAMES_MENU_NUMBER;
    if (depth > 0) {
        path[n++] = '-';
        path[n++] = (char)('1' + game);
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

static void draw_main(void)
{
    draw_path(0);
    font_draw(&font_large_bold, (CONTENT_WIDTH - font_text_width(&font_large_bold, text_games)) / 2, LIST_Y, text_games, 1);
    lcd_blit_strips(10, 23, 64, 16, menu_games_icon);
    draw_scrollbar(GAMES_MENU_THUMB);
    draw_softkey(text_select);
}

static void draw_games(void)
{
    uint8_t i;

    draw_path(1);
    for (i = 0; i < GAME_COUNT; i++)
        draw_row(i, game_name(i), i == game);
    draw_scrollbar(thumb_for(game, GAME_COUNT));
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

/* One bar per level: an outline that grows by two pixels a level, filled
   up to the chosen level. */
static void draw_level(void)
{
    uint8_t i;

    font_draw(&font_small_bold, 5, 0, text_level_title, 1);
    for (i = 0; i < LEVEL_COUNT; i++) {
        uint8_t x = (uint8_t)(5 + i * 8);
        uint8_t height = (uint8_t)(6 + i * 2);

        lcd_fill_rect(x + 5, LEVEL_BASE_Y - height, 1, height, 1);
        lcd_fill_rect(x + 1, LEVEL_BASE_Y, 5, 1, 1);
        if (i <= level_choice)
            lcd_fill_rect(x, LEVEL_BASE_Y - height - 1, 4, height, 1);
    }
    draw_softkey(text_ok);
}

static void draw_number(const struct font *font, int x, int y, uint16_t value)
{
    char digits[6];
    uint8_t n = sizeof digits - 1;

    digits[n] = 0;
    do {
        digits[--n] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    font_draw(font, x, y, digits + n, 1);
}

/* A note in the large font, one line every 15 rows; %N is the number. */
static void draw_note(const char *text, uint16_t number)
{
    uint8_t y = 3;

    for (;;) {
        if (text[0] == '%' && text[1] == 'N')
            draw_number(&font_large_bold, 0, y, number);
        else
            font_draw(&font_large_bold, 0, y, text, 1);
        while (*text && *text != '\n')
            text++;
        if (!*text++)
            break;
        y += 15;
    }
}

static void draw_play(void)
{
    snake_draw();
}

/* Returns the start of the line after the one starting at `text`: as many
   whole words as fit across the screen. */
static const char *help_next_line(const char *text)
{
    const char *end = text, *p = text;
    uint8_t width = 0;

    for (;;) {
        while (*p && *p != ' ')
            width += font_char_width(&font_small_plain, *p++);
        if (width > LCD_WIDTH && end != text)
            break;
        end = p;
        if (!*p)
            return p;
        width += font_char_width(&font_small_plain, *p++);
    }
    return end + 1; /* skip the space the line broke at */
}

static void draw_help_line(uint8_t row, const char *text, const char *end)
{
    int x = 0;

    for (; text != end && *text; text++) {
        char one[2];

        one[0] = *text;
        one[1] = 0;
        x = font_draw(&font_small_plain, x, HELP_Y + row * HELP_LINE_HEIGHT, one, 1);
    }
}

static void draw_help(void)
{
    const char *line = help_page;
    uint8_t row;

    for (row = 0; row < HELP_LINES && *line; row++) {
        const char *next = help_next_line(line);

        draw_help_line(row, line, next);
        line = next;
    }
    draw_softkey(text_more);
}

/* More: the next page, or the first one again after the last. */
static void help_more(void)
{
    uint8_t row;

    for (row = 0; row < HELP_LINES && *help_page; row++)
        help_page = help_next_line(help_page);
    if (!*help_page)
        help_page = text_help_snake;
}

void menu_init(void)
{
    screen = SCREEN_MAIN;
    game = 0;
    resume = RESUME_NONE;
    platform_settings_load(GAME_SNAKE, &settings);
    if (settings.level >= LEVEL_COUNT)
        settings.level = 0;
}

/* Opens Snake's menu on New game, or on Continue or Last view if there. */
static void game_menu_open(void)
{
    screen = SCREEN_GAME;
    item = item_top = 1;
}

static void play_schedule(uint8_t ticks)
{
    play_ticks = ticks;
    play_us = 0;
}

static void play_start(void)
{
    game_srand(uptime);
    snake_init(settings.level);
    screen = SCREEN_PLAY;
    play_schedule((uint8_t)((uint16_t)game_speed_table[settings.level] * 320 / 249));
}

static void play_over(void)
{
    new_top_score = snake.score > settings.top_score;
    if (new_top_score) {
        settings.top_score = snake.score;
        platform_settings_save(GAME_SNAKE, &settings);
    }
    resume = RESUME_LAST_VIEW;
    screen = SCREEN_GAME_OVER;
    page_ticks = NOTE_TICKS;
}

void menu_game_step(void)
{
    uint8_t delay;

    if (screen != SCREEN_PLAY)
        return;
    delay = snake_step();
    if (delay)
        play_schedule(delay);
    else
        play_over();
}

static void play_key(uint8_t key)
{
    switch (key) {
    case MENU_KEY_UP:
        snake_key('2');
        break;
    case MENU_KEY_DOWN:
        snake_key('8');
        break;
    case MENU_KEY_LEFT:
        snake_key('4');
        break;
    case MENU_KEY_RIGHT:
        snake_key('6');
        break;
    case MENU_KEY_BACK:
        /* The C key pauses into the game's menu. */
        resume = RESUME_CONTINUE;
        game_menu_open();
        break;
    default:
        break;
    }
}

static void game_menu_select(void)
{
    switch (item_id(item)) {
    case ITEM_RESUME:
        if (resume == RESUME_CONTINUE) {
            screen = SCREEN_PLAY;
            play_schedule((uint8_t)((uint16_t)game_speed_table[snake.level] * 320 / 249));
        } else {
            screen = SCREEN_LAST_VIEW;
        }
        break;
    case ITEM_NEW_GAME:
        resume = RESUME_NONE;
        play_start();
        break;
    case ITEM_LEVEL:
        screen = SCREEN_LEVEL;
        level_choice = settings.level;
        break;
    case ITEM_TOP_SCORE:
        screen = SCREEN_TOP_SCORE;
        page_ticks = NOTE_TICKS;
        break;
    case ITEM_INSTRUCTIONS:
        screen = SCREEN_HELP;
        help_page = text_help_snake;
        break;
    default:
        break;
    }
}

static void game_menu_move(uint8_t key)
{
    uint8_t count = item_count();

    if (key == MENU_KEY_DOWN) {
        item = (uint8_t)((item + 1) % count);
        /* The window follows the selection, wrapping with it. */
        if ((uint8_t)((item + count - item_top) % count) >= VISIBLE_ROWS)
            item_top = (uint8_t)((item + count - (VISIBLE_ROWS - 1)) % count);
    } else if (key == MENU_KEY_UP) {
        item = (uint8_t)((item + count - 1) % count);
        if ((uint8_t)((item + count - item_top) % count) >= VISIBLE_ROWS)
            item_top = item;
    }
}

void menu_key(uint8_t key)
{
    switch (screen) {
    case SCREEN_MAIN:
        if (key == MENU_KEY_SELECT) {
            screen = SCREEN_GAMES;
            game = 0;
        }
        break;
    case SCREEN_GAMES:
        if (key == MENU_KEY_DOWN) {
            game = (uint8_t)((game + 1) % GAME_COUNT);
        } else if (key == MENU_KEY_UP) {
            game = (uint8_t)((game + GAME_COUNT - 1) % GAME_COUNT);
        } else if (key == MENU_KEY_SELECT) {
            if (game == GAME_SNAKE) /* the only game wired up so far */
                game_menu_open();
        } else {
            screen = SCREEN_MAIN;
        }
        break;
    case SCREEN_LEVEL:
        if (key == MENU_KEY_UP) {
            if (level_choice < LEVEL_COUNT - 1)
                level_choice++;
        } else if (key == MENU_KEY_DOWN) {
            if (level_choice > 0)
                level_choice--;
        } else {
            if (key == MENU_KEY_SELECT) {
                settings.level = level_choice;
                platform_settings_save(GAME_SNAKE, &settings);
            }
            screen = SCREEN_GAME;
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
            menu_key(key);
        break;
    case SCREEN_GAME_OVER:
    case SCREEN_LAST_VIEW:
        game_menu_open();
        break;
    case SCREEN_PLAY:
        play_key(key);
        break;
    default:
        if (key == MENU_KEY_DOWN || key == MENU_KEY_UP)
            game_menu_move(key);
        else if (key == MENU_KEY_SELECT)
            game_menu_select();
        else if (key == MENU_KEY_BACK)
            screen = SCREEN_GAMES;
        break;
    }
}

uint8_t menu_tick(void)
{
    uptime++;
    switch (screen) {
    case SCREEN_TOP_SCORE:
        if (--page_ticks == 0) {
            screen = SCREEN_GAME;
            return 1;
        }
        break;
    case SCREEN_GAME_OVER:
        if (--page_ticks == 0) {
            game_menu_open();
            return 1;
        }
        break;
    case SCREEN_PLAY:
        play_us += FRAME_US;
        while (play_us >= PHONE_TICK_US) {
            play_us -= PHONE_TICK_US;
            if (play_ticks && --play_ticks == 0) {
                menu_game_step();
                return 1;
            }
        }
        break;
    default:
        break;
    }
    return 0;
}

void menu_draw(void)
{
    lcd_clear();
    switch (screen) {
    case SCREEN_MAIN:
        draw_main();
        break;
    case SCREEN_GAMES:
        draw_games();
        break;
    case SCREEN_LEVEL:
        draw_level();
        break;
    case SCREEN_TOP_SCORE:
        draw_note(text_top_score_value, settings.top_score);
        break;
    case SCREEN_GAME_OVER:
        draw_note(new_top_score ? text_game_over_top_score : text_game_over_score, snake.score);
        break;
    case SCREEN_PLAY:
    case SCREEN_LAST_VIEW:
        draw_play();
        break;
    case SCREEN_HELP:
        draw_help();
        break;
    default:
        draw_game();
        break;
    }
}
