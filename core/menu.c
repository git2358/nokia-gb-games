#include "menu.h"

#include "font.h"
#include "game_assets.h"
#include "lcd.h"

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
    SCREEN_GAME
};

/* A game's menu in item-number order. It opens on New game. */
enum {
    ITEM_LEVEL,
    ITEM_NEW_GAME,
    ITEM_TOP_SCORE,
    ITEM_INSTRUCTIONS,
    ITEM_COUNT
};

#define GAME_COUNT 3

static uint8_t screen;
static uint8_t game;     /* selection in the list of games */
static uint8_t item;     /* selection in a game's menu */
static uint8_t item_top; /* first visible row of a game's menu */

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

static const char *item_name(uint8_t index)
{
    switch (index) {
    case ITEM_LEVEL:
        return text_level;
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
        uint8_t index = (uint8_t)((item_top + row) % ITEM_COUNT);

        draw_row(row, item_name(index), index == item);
    }
    draw_scrollbar(thumb_for(item, ITEM_COUNT));
    draw_softkey(text_select);
}

void menu_init(void)
{
    screen = SCREEN_MAIN;
    game = 0;
}

static void game_menu_move(uint8_t key)
{
    if (key == MENU_KEY_DOWN) {
        item = (uint8_t)((item + 1) % ITEM_COUNT);
        /* The window follows the selection, wrapping with it. */
        if ((uint8_t)((item + ITEM_COUNT - item_top) % ITEM_COUNT) >= VISIBLE_ROWS)
            item_top = (uint8_t)((item + ITEM_COUNT - (VISIBLE_ROWS - 1)) % ITEM_COUNT);
    } else {
        item = (uint8_t)((item + ITEM_COUNT - 1) % ITEM_COUNT);
        if ((uint8_t)((item + ITEM_COUNT - item_top) % ITEM_COUNT) >= VISIBLE_ROWS)
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
            screen = SCREEN_GAME;
            item = item_top = ITEM_NEW_GAME;
        } else {
            screen = SCREEN_MAIN;
        }
        break;
    default:
        if (key == MENU_KEY_DOWN || key == MENU_KEY_UP)
            game_menu_move(key);
        else if (key == MENU_KEY_BACK)
            screen = SCREEN_GAMES;
        break;
    }
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
    default:
        draw_game();
        break;
    }
}
