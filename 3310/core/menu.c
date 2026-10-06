#include "menu.h"

#include "bantumi.h"
#include "fireworks.h"
#include "font.h"
#include "game.h"
#include "game_assets.h"
#include "games.h"
#include "lcd.h"
#include "native_tiles.h"
#include "rand.h"
#include "si.h"
#include "sound.h"
#include "pairs2.h"
#include "snake2.h"
#include "sprite.h"
#include "title.h"
#include "version.h"

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

/* The main menu's Games icon is animated: its first picture stays for
   GAMES_ICON_FIRST phone ticks, then the four pictures follow one another
   every GAMES_ICON_STEP, three times round, and it stops on the first.
   Measured in MAME, where the first picture stays 0.84 s and the steps
   come 0.155 s apart. Showing the entry again starts it over. */
#define GAMES_ICON_X 10
#define GAMES_ICON_Y 23
#define GAMES_ICON_WIDTH 64
#define GAMES_ICON_HEIGHT 16
#define GAMES_ICON_BYTES 128
#define GAMES_ICON_PICTURES 4
#define GAMES_ICON_STEPS 12
#define GAMES_ICON_FIRST 108
#define GAMES_ICON_STEP 20

enum {
    SCREEN_MAIN,
    SCREEN_GAMES,
    SCREEN_ABOUT, /* the full-screen list's last entry: version and repository */
    SCREEN_SETTINGS,      /* the phone's Settings: one page per setting */
    SCREEN_SETTING_VALUE, /* a setting's Off and On */
    SCREEN_DONE,          /* the phone's Done note after a setting is changed */
    SCREEN_LEVEL,         /* Snake II's Level: bars of rising height */
    SCREEN_MAZES,         /* Snake II's list of mazes */
    SCREEN_MAZE_DONE,     /* its note that a maze was chosen */
    SCREEN_TITLE,         /* the game's title animation, before its menu */
    SCREEN_MODES,         /* Pairs II's list: Time trial, Puzzle */
    SCREEN_GAME,
    SCREEN_TOP_SCORE,
    SCREEN_HELP,
    SCREEN_PLAY,
    SCREEN_FIREWORKS, /* after a new top score or a won Bantumi */
    SCREEN_GAME_OVER
};

/* The settings, in the phone's order. The last is the phone's Club Nokia
   score ID, which has no ID and does nothing here. */
enum {
    SETTING_SOUNDS,
    SETTING_LIGHTS,
    SETTING_SHAKES,
    SETTING_CLUB_NOKIA_ID,
    SETTING_COUNT
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

/* The fireworks: a new picture every FIREWORKS_STEP phone ticks, the six
   twice over, with a sound as they start and again as the Game over page
   follows them. Measured in MAME: 233 ms a picture, the sound 0x23 after a
   new top score and 0x22 after a won Bantumi. */
#define FIREWORKS_STEP 30
#define FIREWORKS_STEPS (2 * FIREWORKS_PICTURES)
#define SOUND_TOP_SCORE 0x23
#define SOUND_WON 0x22

/* The top score before anyone has played. The firmware has no default of
   its own: the 4075 MAME shows is a score saved in the PMM dump. */
#define DEFAULT_TOP_SCORE 0

/* The Done note: a box in its top right corner is ticked in three
   pictures, the second and third shown after DONE_STEP_1 and DONE_STEP_2
   phone ticks, and the note closes after DONE_TICKS. Measured in MAME:
   0.70 s, 0.92 s and 1.47 s. */
#define DONE_X 62
#define DONE_WIDTH 22
#define DONE_HEIGHT 32
#define DONE_FRAME_BYTES 88
#define DONE_STEP_1 90
#define DONE_STEP_2 118
#define DONE_TICKS ((uint16_t)(190ul * PHONE_TICK_US / MENU_FRAME_US))
/* Where a setting's value is shown on its page: right-aligned here, above
   the soft key. */
#define SETTING_VALUE_RIGHT 78
#define SETTING_VALUE_Y 29

/* The Top score page's animation in its top right corner: stars gather
   into a cup, which then flashes. A new picture every 25 phone ticks; the
   last one stays. */
#define SPARKLE_X 63
#define SPARKLE_WIDTH 21
#define SPARKLE_HEIGHT 24
#define SPARKLE_FRAME_BYTES 84
#define SPARKLE_TICKS 25
static const uint8_t sparkle_frames[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 10, 10, 9, 9, 10, 10, 9, 10 };

/* The games' menus. Continue is there only while a game is paused. */
enum {
    ITEM_CONTINUE,
    ITEM_NEW_GAME,
    ITEM_LEVEL,
    ITEM_MAZES,
    ITEM_TOP_SCORE,
    ITEM_INSTRUCTIONS
};
static const uint8_t space_impact_items[] = { ITEM_NEW_GAME, ITEM_TOP_SCORE, ITEM_INSTRUCTIONS };
static const uint8_t snake_items[] = { ITEM_NEW_GAME, ITEM_LEVEL, ITEM_MAZES, ITEM_TOP_SCORE, ITEM_INSTRUCTIONS };
static const uint8_t pairs_items[] = { ITEM_NEW_GAME, ITEM_LEVEL, ITEM_TOP_SCORE, ITEM_INSTRUCTIONS };
/* Bantumi keeps no top score. */
static const uint8_t bantumi_items[] = { ITEM_NEW_GAME, ITEM_LEVEL, ITEM_INSTRUCTIONS };

/* Snake II's Level page: a bar for each level, the ones up to the level
   filled, each two rows taller than the one before and with a shadow on
   its right and below. */
#define LEVEL_BAR_X 5
#define LEVEL_BAR_PITCH 8
#define LEVEL_BAR_WIDTH 4
#define LEVEL_BAR_BOTTOM 34
#define LEVEL_BAR_FIRST_HEIGHT 6

/* The phone's list of games ends with a Settings entry. */
#define LIST_COUNT (GAME_COUNT + 1)

#define NO_KEY 0xff

static uint8_t screen;
static uint8_t game;     /* selection in the list of games */
static uint8_t game_top; /* first visible row of that list */
static uint8_t item;     /* selection in the game's menu, counting visible items */
static uint8_t item_top; /* first visible row of the game's menu */
static uint8_t setting;  /* the setting whose page is shown */
static uint8_t value;    /* selection on its Off and On page */
static uint8_t done_step;  /* picture of the Done note's animation */
static uint8_t done_ticks; /* phone ticks the note has been shown */
static uint8_t paused;   /* a game is waiting behind the menu */
static uint8_t paused_game; /* which one */
static uint8_t paused_mode; /* and its mode, for Pairs II */
static uint8_t pairs_mode; /* Pairs II's mode, the selection in its list */
static uint8_t pairs_mode_top;
static uint8_t level;    /* selection on the Level page, 0 is the first */
static uint8_t maze;     /* selection in the list of mazes */
static uint8_t maze_top;
static uint8_t new_top_score;  /* the game just ended beat the top score */
static uint16_t final_score;   /* its score */
static int16_t final_result;   /* Bantumi's: the player's beans less the phone's */
static uint8_t held = NO_KEY;  /* the button the game sees as held */
static uint8_t seed_fixed;     /* the games' generator is never reseeded from the time */
static uint16_t play_us;       /* time not yet turned into phone ticks */
static uint16_t uptime;        /* menu_tick calls so far; seeds the games' generator */
static uint8_t board_drawn;    /* the LCD holds the running game's picture */
uint8_t menu_drew_picture;
/* The full-screen menu the LCD holds and the selection drawn on it;
   NO_SCREEN when it holds something else. */
#define NO_SCREEN 0xff
static uint8_t drawn_screen = NO_SCREEN, drawn_selection;
static uint8_t full_screen;    /* the full-screen variant was chosen */
#ifdef NATIVE_PLATFORM_TILES
static uint8_t native_tiled;   /* the platform showed that menu from its tiles */
#endif
static uint8_t view_mode;      /* which of the views below the LCD is set up for */

/* Columns of the phone's LCD a platform with LCD_GAME_ZOOM shows; what
   does not fit is left off on the right. That costs Space Impact nothing
   it needs: its score ends at column 75, and what is cut is where enemies
   come on and the last columns the ship can fly into. The game is drawn
   with its first column on a whole cell of the framebuffer, GAME_ZOOM_X,
   which is what a platform can magnify. */
#define GAME_ZOOM_WIDTH (LCD_FB_WIDTH / LCD_GAME_ZOOM < LCD_WIDTH ? LCD_FB_WIDTH / LCD_GAME_ZOOM : LCD_WIDTH)
#define GAME_ZOOM_X ((LCD_FB_WIDTH - GAME_ZOOM_WIDTH) / 2 / 8 * 8)

/* The phone's LCD; the full-screen variant's own menus; its Snake II
   board. */
enum {
    VIEW_PHONE,
    VIEW_NATIVE,
    VIEW_BOARD
};

/* The full-screen Snake II board's part of the framebuffer: all of it, or
   the middle 1/LCD_ZOOM of it magnified, on whole 8x8 cells. */
#define BOARD_AREA_W (LCD_FB_WIDTH / LCD_ZOOM)
#define BOARD_AREA_H (LCD_FB_HEIGHT / LCD_ZOOM)
#define BOARD_AREA_X ((LCD_FB_WIDTH - BOARD_AREA_W) / 2 / 8 * 8)
#define BOARD_AREA_Y ((LCD_FB_HEIGHT - BOARD_AREA_H) / 2 / 8 * 8)

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
static const char text_hint_back[] = "B back";
/* The full-screen list's extra entry and its page. The port's own words. */
static const char text_about[] = "About";
static const char text_about_body[] = "Nokia 3310 games " GAME_VERSION "\ngithub.com/lukesau/\nnokia-gb-games";
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
static uint8_t icon_steps;    /* steps of the Games icon's animation so far */
static uint8_t fireworks_step;  /* picture of the fireworks shown */
static uint8_t fireworks_ticks; /* phone ticks it has been shown */
static uint8_t icon_ticks;    /* phone ticks to its next step */
static uint16_t icon_us;      /* time not yet turned into phone ticks */

/* Shows the main menu, its Games icon starting over. */
static void main_open(void)
{
    screen = SCREEN_MAIN;
    icon_steps = 0;
    icon_ticks = GAMES_ICON_FIRST;
    icon_us = 0;
}
static const char *help_page; /* first character of the Instructions page shown */

/* Where the settings of the game whose menu is open are kept: each of
   Pairs II's modes has its own. */
static uint8_t settings_slot(void)
{
    return game == GAME_PAIRS && pairs_mode == PAIRS2_PUZZLE ? GAME_SLOT_PAIRS_PUZZLE : game;
}

/* The settings of the game whose menu is open. */
/* The top score shown and beaten: the record's, or for Snake II on the
   full-screen variant's board one kept apart. */
static uint16_t top_score;

static uint8_t full_snake(void)
{
    return (uint8_t)(full_screen && game == GAME_SNAKE && LCD_HAS_SURROUND);
}

static void settings_load(void)
{
    struct game_settings full;

    if (!platform_settings_load(settings_slot(), &settings))
        settings.top_score = DEFAULT_TOP_SCORE;
    top_score = settings.top_score;
    if (full_snake()) {
        platform_settings_load(GAME_SLOT_SNAKE_FULL, &full);
        top_score = full.top_score;
    }
    if (game == GAME_SNAKE) {
        /* The phone's own default: the fastest level, no maze. */
        if (settings.level >= SNAKE2_LEVELS)
            settings.level = SNAKE2_LEVELS - 1;
        if (settings.option >= SNAKE2_MAZE_COUNT)
            settings.option = 0;
    }
    if (game == GAME_PAIRS && settings.level >= PAIRS2_LEVELS)
        settings.level = PAIRS2_LEVELS - 1;
    if (game == GAME_BANTUMI && settings.level >= BANTUMI_LEVELS)
        settings.level = BANTUMI_LEVELS - 1;
}

/* The bars of the Level page. */
static uint8_t levels(void)
{
    return game == GAME_PAIRS ? PAIRS2_LEVELS : game == GAME_BANTUMI ? BANTUMI_LEVELS : SNAKE2_LEVELS;
}

static const char *mode_name(uint8_t which)
{
    return which == PAIRS2_PUZZLE ? text_puzzle : text_time_trial;
}

/* Pairs II's menus are its modes' own. */
static const char *game_name(void)
{
    if (game == GAME_PAIRS)
        return mode_name(pairs_mode);
    if (game == GAME_BANTUMI)
        return text_bantumi;
    return game == GAME_SNAKE ? text_snake : text_space_impact;
}

static const char *game_help(void)
{
    if (game == GAME_PAIRS)
        return pairs_mode == PAIRS2_PUZZLE ? text_help_puzzle : text_help_time_trial;
    if (game == GAME_BANTUMI)
        return text_help_bantumi;
    return game == GAME_SNAKE ? text_help_snake : text_help_space_impact;
}

static const char *maze_name(uint8_t which)
{
    switch (which) {
    case 0:
        return text_no_maze;
    case 1:
        return text_maze_1;
    case 2:
        return text_maze_2;
    case 3:
        return text_maze_3;
    case 4:
        return text_maze_4;
    default:
        return text_maze_5;
    }
}

static const char *setting_name(uint8_t which)
{
    switch (which) {
    case SETTING_SOUNDS:
        return text_sounds;
    case SETTING_LIGHTS:
        return text_lights;
    case SETTING_SHAKES:
        return text_shakes;
    default:
        return text_club_nokia_id;
    }
}

/* The switch a setting is, or null for the score ID. */
static uint8_t *setting_switch(uint8_t which)
{
    switch (which) {
    case SETTING_SOUNDS:
        return &games_options.sounds;
    case SETTING_LIGHTS:
        return &games_options.lights;
    case SETTING_SHAKES:
        return &games_options.shakes;
    default:
        return 0;
    }
}

/* What a setting's page shows as its value. */
static const char *setting_value(uint8_t which)
{
    const uint8_t *on = setting_switch(which);

    return !on ? text_no_id : *on ? text_on : text_off;
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

/* A game of the menu's own is waiting behind it. */
static uint8_t can_continue(void)
{
    return (uint8_t)(paused && paused_game == game && (game != GAME_PAIRS || paused_mode == pairs_mode));
}

static const uint8_t *items(void)
{
    switch (game) {
    case GAME_SNAKE:
        return snake_items;
    case GAME_PAIRS:
        return pairs_items;
    case GAME_BANTUMI:
        return bantumi_items;
    default:
        return space_impact_items;
    }
}

static uint8_t item_count(void)
{
    uint8_t n = game == GAME_SNAKE ? sizeof snake_items : game == GAME_PAIRS ? sizeof pairs_items
                : game == GAME_BANTUMI ? sizeof bantumi_items : sizeof space_impact_items;

    return (uint8_t)(n + can_continue());
}

/* The item at visible position `index`. */
static uint8_t item_id(uint8_t index)
{
    if (can_continue()) {
        if (!index)
            return ITEM_CONTINUE;
        index--;
    }
    return items()[index];
}

static const char *item_name(uint8_t id)
{
    switch (id) {
    case ITEM_CONTINUE:
        return text_continue;
    case ITEM_NEW_GAME:
        return text_new_game;
    case ITEM_LEVEL:
        return text_level;
    case ITEM_MAZES:
        return text_mazes;
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

/* The menu path in the top right corner, such as 8-2-1 or 8-6-1-2. */
static void draw_path(uint8_t depth)
{
    char path[8];
    uint8_t n = 0;

    path[n++] = '0' + GAMES_MENU_NUMBER;
    if (depth > 0) {
        path[n++] = '-';
        /* The phone numbers Settings 6: an entry before it is not shown. */
        path[n++] = (char)((game < GAME_COUNT ? '1' : '2') + game);
    }
    /* Pairs II's menus are a level deeper, under its modes. */
    if (depth > 1) {
        path[n++] = '-';
        path[n++] = (char)('1' + (game == GAME_PAIRS ? pairs_mode : game < GAME_COUNT ? item : setting));
    }
    if (depth > 2) {
        path[n++] = '-';
        path[n++] = (char)('1' + (game == GAME_PAIRS ? item : game < GAME_COUNT ? maze : value));
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

/* The hint for the full-screen variant, in the space under the phone's LCD,
   and the port's version in the bottom right corner of the screen. */
static void draw_hint(void)
{
#if LCD_HAS_SURROUND
    lcd_view_full();
    font_draw(&font_small_plain, (LCD_FB_WIDTH - font_text_width(&font_small_plain, text_full_screen_hint)) / 2,
              LCD_BELOW_PHONE + 8, text_full_screen_hint, 1);
    font_draw(&font_small_plain, LCD_FB_WIDTH - font_text_width(&font_small_plain, GAME_VERSION) - 2,
              LCD_FB_HEIGHT - font_small_plain.height - 2, GAME_VERSION, 1);
    lcd_view_phone();
    surround_used = 1;
#endif
}

static void draw_games_icon(void)
{
    lcd_blit_strips(GAMES_ICON_X, GAMES_ICON_Y, GAMES_ICON_WIDTH, GAMES_ICON_HEIGHT,
                    menu_games_icon + icon_steps % GAMES_ICON_PICTURES * GAMES_ICON_BYTES);
}

static void draw_main(void)
{
    draw_hint();
    draw_path(0);
    font_draw(&font_large_bold, (CONTENT_WIDTH - font_text_width(&font_large_bold, text_games)) / 2, LIST_Y, text_games, 1);
    draw_games_icon();
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

    draw_path(game == GAME_PAIRS ? 3 : 2);
    for (row = 0; row < VISIBLE_ROWS; row++) {
        uint8_t index = (uint8_t)((item_top + row) % item_count());

        draw_row(row, item_name(item_id(index)), index == item);
    }
    draw_scrollbar(thumb_for(item, item_count()));
    draw_softkey(text_select);
}

/* A setting's page: its name, and its value in the plain font at the
   bottom right. The three pages are a list the scrollbar shows. */
static void draw_setting(void)
{
    const char *shown = setting_value(setting);

    draw_path(2);
    font_draw(&font_small_bold, 0, LIST_Y, setting_name(setting), 1);
    font_draw(&font_small_plain, SETTING_VALUE_RIGHT - font_text_width(&font_small_plain, shown), SETTING_VALUE_Y, shown, 1);
    draw_scrollbar(thumb_for(setting, SETTING_COUNT));
    draw_softkey(text_select);
}

static void draw_setting_value(void)
{
    draw_path(3);
    draw_row(0, text_off, value == 0);
    draw_row(1, text_on, value == 1);
    draw_scrollbar(thumb_for(value, 2));
    draw_softkey(text_ok);
}

static void draw_level(void)
{
    uint8_t i, x, top;

    font_draw(&font_small_bold, LEVEL_BAR_X, HEADER_Y, text_level_value, 1);
    for (i = 0; i < levels(); i++) {
        x = (uint8_t)(LEVEL_BAR_X + i * LEVEL_BAR_PITCH);
        top = (uint8_t)(LEVEL_BAR_BOTTOM + 1 - LEVEL_BAR_FIRST_HEIGHT - 2 * i);
        if (i <= level)
            lcd_fill_rect(x, top, LEVEL_BAR_WIDTH, LEVEL_BAR_BOTTOM + 1 - top, 1);
        lcd_fill_rect(x + LEVEL_BAR_WIDTH + 1, top + 1, 1, LEVEL_BAR_BOTTOM + 1 - top, 1);
        lcd_fill_rect(x + 1, LEVEL_BAR_BOTTOM + 2, LEVEL_BAR_WIDTH + 1, 1, 1);
    }
    draw_softkey(text_ok);
}

static void draw_modes(void)
{
    draw_path(2);
    draw_row(0, mode_name(PAIRS2_TIME_TRIAL), pairs_mode == PAIRS2_TIME_TRIAL);
    draw_row(1, mode_name(PAIRS2_PUZZLE), pairs_mode == PAIRS2_PUZZLE);
    draw_scrollbar(thumb_for(pairs_mode, 2));
    draw_softkey(text_select);
}

static void draw_mazes(void)
{
    uint8_t row;

    draw_path(3);
    for (row = 0; row < VISIBLE_ROWS; row++) {
        uint8_t index = (uint8_t)((maze_top + row) % SNAKE2_MAZE_COUNT);

        draw_row(row, maze_name(index), index == maze);
    }
    draw_scrollbar(thumb_for(maze, SNAKE2_MAZE_COUNT));
    draw_softkey(text_ok);
}

/* "Maze 1 selected", the name on a line of its own. */
static void draw_maze_done(void)
{
    const char *rest = text_maze_selected;

    while (*rest && *rest != ' ')
        rest++;
    font_draw(&font_large_bold, 0, 3, maze_name(maze), 1);
    font_draw(&font_large_bold, 0, 18, *rest ? rest + 1 : rest, 1);
}

static void draw_done_tick(void)
{
    lcd_blit_strips(DONE_X, 0, DONE_WIDTH, DONE_HEIGHT, done_tick + done_step * DONE_FRAME_BYTES);
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

/* The full-screen list has an About entry after the phone's entries. */
static uint8_t list_count(void)
{
    return full_screen ? LIST_COUNT + 1 : LIST_COUNT;
}

static void native_games(void)
{
    uint8_t i;

    native_title(text_games);
    for (i = 0; i < LIST_COUNT; i++)
        native_row(i, list_name(i), i == game);
    native_row(LIST_COUNT, text_about, game == LIST_COUNT);
    native_hint(text_hint_select);
}

static void native_note(const char *title, const char *text, uint16_t number);

static void native_about(void)
{
    native_note(text_about, text_about_body, 0);
    native_hint(text_hint_back);
}

/* The full-screen variant's settings: every setting on one page with its
   value, which A switches. The port's own layout. */
static void native_settings(void)
{
    uint8_t i;

    native_title(text_settings);
    for (i = 0; i < SETTING_COUNT; i++) {
        const char *shown = setting_value(i);

        native_row(i, setting_name(i), i == setting);
        font_draw(&NATIVE_FONT, LCD_FB_WIDTH - NATIVE_MARGIN - font_text_width(&NATIVE_FONT, shown),
                  NATIVE_LIST_Y + i * NATIVE_ROW_HEIGHT + 1, shown, 1);
    }
    native_hint(text_hint_select);
}

static void native_modes(void)
{
    native_title(text_pairs);
    native_row(0, mode_name(PAIRS2_TIME_TRIAL), pairs_mode == PAIRS2_TIME_TRIAL);
    native_row(1, mode_name(PAIRS2_PUZZLE), pairs_mode == PAIRS2_PUZZLE);
    native_hint(text_hint_select);
}

static void native_mazes(void)
{
    uint8_t i;

    native_title(text_mazes);
    for (i = 0; i < SNAKE2_MAZE_COUNT; i++)
        native_row(i, maze_name(i), i == maze);
    native_hint(text_hint_select);
}

static void native_game(void)
{
    uint8_t i;

    native_title(game_name());
    for (i = 0; i < item_count(); i++)
        native_row(i, item_name(item_id(i)), i == item);
    native_hint(text_hint_select);
}

/* The full-screen menus with a cursor. */
static uint8_t native_listed(void)
{
    return (uint8_t)(screen == SCREEN_GAMES || screen == SCREEN_GAME || screen == SCREEN_SETTINGS
                     || screen == SCREEN_MAZES || screen == SCREEN_MODES);
}

static uint8_t native_selection(void)
{
    switch (screen) {
    case SCREEN_GAMES:
        return game;
    case SCREEN_GAME:
        return item;
    case SCREEN_MAZES:
        return maze;
    case SCREEN_MODES:
        return pairs_mode;
    default:
        return setting;
    }
}

/* When only the selection moved on a full-screen menu, redraws just that
   and returns nonzero. */
static uint8_t native_update(void)
{
    uint8_t selection;

    if (drawn_screen != screen || !native_listed())
        return 0;
    selection = native_selection();
#ifdef NATIVE_PLATFORM_TILES
    if (native_tiled) {
        platform_native_cursor(drawn_selection, 0);
        platform_native_cursor(selection, 1);
    } else
#endif
    {
        native_cursor(drawn_selection, 0);
        native_cursor(selection, 1);
    }
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
    /* Wider than a byte: the full-screen limit is close to 255 and a line
       measured past it must still compare as too long. */
    uint16_t limit = full_screen ? LCD_FB_WIDTH - 2 * NATIVE_MARGIN : LCD_WIDTH;
    uint16_t width = 0;

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
        help_page = game_help();
}

/* The full-screen menus made at build time (native_tiles.h), numbered:
   the list of games, About, Snake II's mazes, Pairs II's modes, the
   settings with each combination of the switches' values, each game's menu
   with and without Continue, and each game's instructions page by page.
   Pairs II's menus are its modes' own, so a "menu" here is a game or, past
   GAME_COUNT, Pairs II's Puzzle. */
#define NATIVE_ID_GAMES 0
#define NATIVE_ID_ABOUT 1
#define NATIVE_ID_MAZES 2
#define NATIVE_ID_MODES 3
#define NATIVE_ID_SETTINGS 4
#define NATIVE_SWITCHES 3
#define NATIVE_ID_GAME (NATIVE_ID_SETTINGS + (1 << NATIVE_SWITCHES))
#define NATIVE_MENUS (2 * GAME_COUNT)
#define NATIVE_ID_HELP (NATIVE_ID_GAME + 2 * NATIVE_MENUS)
#define NATIVE_HELP_PAGES 16
#define NATIVE_ID_END (NATIVE_ID_HELP + NATIVE_MENUS * NATIVE_HELP_PAGES)

static uint8_t native_menu(void)
{
    return (uint8_t)(game + (game == GAME_PAIRS && pairs_mode == PAIRS2_PUZZLE ? GAME_COUNT : 0));
}

#ifndef NATIVE_PLATFORM_TILES
/* Sets game and pairs_mode for a menu; 0 if there is no such menu. */
static uint8_t native_menu_set(uint8_t menu)
{
    game = (uint8_t)(menu % GAME_COUNT);
    pairs_mode = menu >= GAME_COUNT ? PAIRS2_PUZZLE : PAIRS2_TIME_TRIAL;
    return (uint8_t)(menu < GAME_COUNT || game == GAME_PAIRS);
}
#endif

/* The instructions page shown, counting from 0; NATIVE_HELP_PAGES if it
   is past those numbered. */
static uint8_t help_page_number(void)
{
    const char *page = game_help();
    uint8_t n, row;

    for (n = 0; n < NATIVE_HELP_PAGES && *page; n++) {
        if (page == help_page)
            return n;
        for (row = 0; row < help_lines() && *page; row++)
            page = help_next_line(page);
    }
    return NATIVE_HELP_PAGES;
}

uint16_t menu_native_id(void)
{
    uint8_t i, bits = 0, n = 0;

    switch (screen) {
    case SCREEN_GAMES:
        return NATIVE_ID_GAMES;
    case SCREEN_ABOUT:
        return NATIVE_ID_ABOUT;
    case SCREEN_MAZES:
        return NATIVE_ID_MAZES;
    case SCREEN_MODES:
        return NATIVE_ID_MODES;
    case SCREEN_SETTINGS:
        for (i = 0; i < SETTING_COUNT; i++) {
            const uint8_t *on = setting_switch(i);

            if (on)
                bits |= (uint8_t)((*on ? 1 : 0) << n++);
        }
        return (uint16_t)(NATIVE_ID_SETTINGS + bits);
    case SCREEN_GAME:
        return (uint16_t)(NATIVE_ID_GAME + native_menu() * 2 + can_continue());
    case SCREEN_HELP:
        n = help_page_number();
        if (n >= NATIVE_HELP_PAGES)
            return NATIVE_NONE;
        return (uint16_t)(NATIVE_ID_HELP + native_menu() * NATIVE_HELP_PAGES + n);
    default:
        return NATIVE_NONE;
    }
}

/* The host draws them for native_gen.c; the platform that shows them has
   no use for these. */
#ifndef NATIVE_PLATFORM_TILES
uint8_t menu_native_draw(uint16_t id)
{
    uint8_t i, n = 0;

    if (id >= NATIVE_ID_END)
        return NATIVE_END;
    full_screen = 1;
    /* No selection: the cursor is drawn over the picture. */
    item = setting = maze = 0xff;
    if (id == NATIVE_ID_GAMES) {
        screen = SCREEN_GAMES;
        game = 0xff;
        native_games();
    } else if (id == NATIVE_ID_ABOUT) {
        screen = SCREEN_ABOUT;
        native_about();
    } else if (id == NATIVE_ID_MAZES) {
        screen = SCREEN_MAZES;
        native_mazes();
    } else if (id == NATIVE_ID_MODES) {
        screen = SCREEN_MODES;
        pairs_mode = 0xff;
        native_modes();
    } else if (id < NATIVE_ID_GAME) {
        screen = SCREEN_SETTINGS;
        for (i = 0; i < SETTING_COUNT; i++) {
            uint8_t *on = setting_switch(i);

            if (on)
                *on = (uint8_t)(((id - NATIVE_ID_SETTINGS) >> n++) & 1);
        }
        native_settings();
    } else if (id < NATIVE_ID_HELP) {
        screen = SCREEN_GAME;
        if (!native_menu_set((uint8_t)((id - NATIVE_ID_GAME) / 2)))
            return NATIVE_SKIP;
        paused = (uint8_t)((id - NATIVE_ID_GAME) & 1);
        paused_game = game;
        paused_mode = pairs_mode;
        native_game();
    } else {
        screen = SCREEN_HELP;
        if (!native_menu_set((uint8_t)((id - NATIVE_ID_HELP) / NATIVE_HELP_PAGES)))
            return NATIVE_SKIP;
        help_page = game_help();
        for (n = (uint8_t)((id - NATIVE_ID_HELP) % NATIVE_HELP_PAGES); n; n--) {
            uint8_t row;

            for (row = 0; row < help_lines() && *help_page; row++)
                help_page = help_next_line(help_page);
            if (!*help_page)
                return NATIVE_SKIP;
        }
        draw_help();
    }
    return NATIVE_DRAWN;
}

void menu_native_cursor(uint8_t row)
{
    full_screen = 1;
    native_cursor(row, 1);
}
#endif

void menu_init(void)
{
    main_open();
    game = game_top = 0;
    paused = 0;
    full_screen = 0;
    held = NO_KEY;
    games_quiet();
    platform_options_load(&games_options);
}

/* Opens the game's menu on its first entry: Continue when a game is
   paused, else New game. */
static void game_menu_open(void)
{
    screen = SCREEN_GAME;
    item = item_top = 0;
    settings_load();
}

/* After the title, the game's menu, or Pairs II's list of modes. */
static void title_over(void)
{
    if (game == GAME_PAIRS) {
        screen = SCREEN_MODES;
        pairs_mode = pairs_mode_top = 0;
    } else {
        game_menu_open();
    }
}

static void fireworks_sound(void)
{
    if (games_options.sounds)
        sound_play(game == GAME_BANTUMI ? SOUND_WON : SOUND_TOP_SCORE);
}

static void play_over(void)
{
    final_score = (uint16_t)games_score;
    final_result = (int16_t)games_score;
    /* Bantumi ends on who won. */
    new_top_score = game != GAME_BANTUMI && final_score > top_score;
    if (new_top_score) {
        top_score = final_score;
        if (full_snake()) {
            struct game_settings full = { 0, 0, 0 };

            full.top_score = final_score;
            platform_settings_save(GAME_SLOT_SNAKE_FULL, &full);
        } else {
            settings.top_score = final_score;
            platform_settings_save(settings_slot(), &settings);
        }
    }
    paused = 0;
    held = NO_KEY;
    if (new_top_score || (game == GAME_BANTUMI && final_result > 0)) {
        screen = SCREEN_FIREWORKS;
        fireworks_step = fireworks_ticks = 0;
        play_us = 0;
        fireworks_sound();
        return;
    }
    screen = SCREEN_GAME_OVER;
    page_ticks = GAME_OVER_TICKS;
}

/* The Game over page's words: Bantumi's say who won. */
static const char *game_over_text(void)
{
    if (game == GAME_BANTUMI)
        return final_result > 0 ? text_game_over_won : final_result < 0 ? text_game_over_lost : text_game_over;
    return new_top_score ? text_game_over_top_score : text_game_over_score;
}

static void play_start(void)
{
    /* The phone seeds the games' generator from its clock, when it is
       set; here the time of the choice does the same. */
    if (!seed_fixed)
        game_rand16_seed = (uint16_t)(uptime % 0xfff0 + 1);
    held = NO_KEY;
    snake2_full = full_snake();
    games_start(game, (uint8_t)(settings.level + 1), game == GAME_PAIRS ? pairs_mode : (uint8_t)(settings.option + 1));
    board_drawn = 0;
    screen = SCREEN_PLAY;
}

/* The phone key a button is in the game, or 0. Snake II: the pad steers,
   A turns clockwise and B anticlockwise, as # and * do on the phone. */
static uint8_t play_phone_key(uint8_t key)
{
    /* Bantumi: left and right, or down and up as the phone's scroll key,
       move the hand; A sows, B asks for a hint (*). */
    if (game == GAME_BANTUMI) {
        switch (key) {
        case MENU_KEY_UP:
            return GAME_KEY_SCROLL_UP;
        case MENU_KEY_DOWN:
            return GAME_KEY_SCROLL_DOWN;
        case MENU_KEY_LEFT:
            return GAME_KEY_4;
        case MENU_KEY_RIGHT:
            return GAME_KEY_6;
        case MENU_KEY_SELECT:
            return GAME_KEY_5;
        case MENU_KEY_BACK:
            return GAME_KEY_STAR;
        default:
            return 0;
        }
    }
    /* Pairs II: the pad moves the cursor, A or B opens a card. */
    if (game == GAME_PAIRS) {
        switch (key) {
        case MENU_KEY_UP:
            return GAME_KEY_2;
        case MENU_KEY_DOWN:
            return GAME_KEY_8;
        case MENU_KEY_LEFT:
            return GAME_KEY_4;
        case MENU_KEY_RIGHT:
            return GAME_KEY_6;
        case MENU_KEY_SELECT:
        case MENU_KEY_BACK:
            return GAME_KEY_5;
        default:
            return 0;
        }
    }
    if (game == GAME_SNAKE) {
        switch (key) {
        case MENU_KEY_UP:
            return GAME_KEY_2;
        case MENU_KEY_DOWN:
            return GAME_KEY_8;
        case MENU_KEY_LEFT:
            return GAME_KEY_4;
        case MENU_KEY_RIGHT:
            return GAME_KEY_6;
        case MENU_KEY_SELECT:
            return GAME_KEY_HASH;
        case MENU_KEY_BACK:
            return GAME_KEY_STAR;
        default:
            return 0;
        }
    }
    switch (key) {
    case MENU_KEY_UP:
        return GAME_KEY_8;
    case MENU_KEY_DOWN:
        return GAME_KEY_0;
    case MENU_KEY_LEFT:
        return GAME_KEY_STAR;
    case MENU_KEY_RIGHT:
        return GAME_KEY_HASH;
    case MENU_KEY_SELECT:
        return GAME_KEY_1;
    case MENU_KEY_BACK:
        return GAME_KEY_4;
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
        paused_game = game;
        paused_mode = pairs_mode;
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
        games_continue();
        break;
    case ITEM_NEW_GAME:
        paused = 0;
        play_start();
        break;
    case ITEM_LEVEL:
        screen = SCREEN_LEVEL;
        level = settings.level;
        break;
    case ITEM_MAZES:
        screen = SCREEN_MAZES;
        maze = maze_top = settings.option;
        break;
    case ITEM_TOP_SCORE:
        screen = SCREEN_TOP_SCORE;
        page_ticks = TOP_SCORE_TICKS;
        play_us = 0;
        sparkle_step = sparkle_ticks = 0;
        break;
    default:
        screen = SCREEN_HELP;
        help_page = game_help();
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
            list_move(key, list_count(), &game, &game_top);
        } else if (key == MENU_KEY_SELECT) {
            if (game < GAME_COUNT) {
                screen = SCREEN_TITLE;
                board_drawn = 0;
                title_start(game, !seed_fixed, (uint16_t)(uptime % 0xfff0 + 1));
            } else if (game == GAME_COUNT) {
                screen = SCREEN_SETTINGS;
                setting = 0;
            } else if (game == LIST_COUNT) {
                screen = SCREEN_ABOUT;
            }
        } else if (key == MENU_KEY_BACK) {
            main_open();
        }
        break;
    case SCREEN_ABOUT:
        screen = SCREEN_GAMES;
        break;
    case SCREEN_TITLE:
        /* Any key ends the title. */
        title_over();
        break;
    case SCREEN_SETTINGS:
        if (key == MENU_KEY_DOWN) {
            setting = (uint8_t)((setting + 1) % SETTING_COUNT);
        } else if (key == MENU_KEY_UP) {
            setting = (uint8_t)((setting + SETTING_COUNT - 1) % SETTING_COUNT);
        } else if (key == MENU_KEY_SELECT && setting_switch(setting)) {
            if (full_screen) {
                /* Switched on the spot; the full page is drawn again. */
                *setting_switch(setting) ^= 1;
                platform_options_save(&games_options);
                drawn_screen = NO_SCREEN;
                return 1;
            }
            screen = SCREEN_SETTING_VALUE;
            value = *setting_switch(setting);
        } else if (key == MENU_KEY_BACK) {
            screen = SCREEN_GAMES;
        }
        break;
    case SCREEN_SETTING_VALUE:
        if (key == MENU_KEY_DOWN || key == MENU_KEY_UP) {
            value ^= 1;
        } else if (key == MENU_KEY_SELECT) {
            *setting_switch(setting) = value;
            platform_options_save(&games_options);
            screen = SCREEN_DONE;
            page_ticks = DONE_TICKS;
            play_us = 0;
            done_step = done_ticks = 0;
        } else if (key == MENU_KEY_BACK) {
            screen = SCREEN_SETTINGS;
        }
        break;
    case SCREEN_DONE:
        /* Any key closes the note; all but C then act on the page under it. */
        screen = SCREEN_SETTINGS;
        if (key != MENU_KEY_BACK)
            handle_key(key);
        break;
    case SCREEN_LEVEL:
        /* Up and down change the level by one, stopping at the ends; OK
           keeps it. */
        if (key == MENU_KEY_UP && level < levels() - 1) {
            level++;
        } else if (key == MENU_KEY_DOWN && level > 0) {
            level--;
        } else if (key == MENU_KEY_SELECT) {
            settings.level = level;
            platform_settings_save(settings_slot(), &settings);
            screen = SCREEN_GAME;
        } else if (key == MENU_KEY_BACK) {
            screen = SCREEN_GAME;
        }
        drawn_screen = NO_SCREEN;
        return 1;
    case SCREEN_MAZES:
        if (key == MENU_KEY_DOWN || key == MENU_KEY_UP) {
            list_move(key, SNAKE2_MAZE_COUNT, &maze, &maze_top);
        } else if (key == MENU_KEY_SELECT) {
            settings.option = maze;
            platform_settings_save(settings_slot(), &settings);
            /* The full-screen variant has no note. */
            if (full_screen) {
                screen = SCREEN_GAME;
                break;
            }
            screen = SCREEN_MAZE_DONE;
            page_ticks = DONE_TICKS;
            play_us = 0;
            done_step = done_ticks = 0;
        } else if (key == MENU_KEY_BACK) {
            screen = SCREEN_GAME;
        }
        break;
    case SCREEN_MAZE_DONE:
        screen = SCREEN_GAME;
        if (key != MENU_KEY_BACK)
            handle_key(key);
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
    case SCREEN_FIREWORKS:
        /* The phone's own: keys wait for the page after them. */
        break;
    case SCREEN_MODES:
        if (key == MENU_KEY_DOWN || key == MENU_KEY_UP)
            list_move(key, 2, &pairs_mode, &pairs_mode_top);
        else if (key == MENU_KEY_SELECT)
            game_menu_open();
        else if (key == MENU_KEY_BACK)
            screen = SCREEN_GAMES;
        break;
    default:
        if (key == MENU_KEY_DOWN || key == MENU_KEY_UP)
            list_move(key, item_count(), &item, &item_top);
        else if (key == MENU_KEY_SELECT)
            game_menu_select();
        else if (key == MENU_KEY_BACK)
            screen = game == GAME_PAIRS ? SCREEN_MODES : SCREEN_GAMES;
        break;
    }
    return 0;
}

uint8_t menu_key(uint8_t key)
{
    uint8_t was_screen = screen, was_game = game, was_item = item, was_top = item_top;
    uint8_t was_setting = setting, was_value = value, was_maze = maze, was_mode = pairs_mode;
    const char *was_page = help_page;
    uint8_t changed;

    sparkle_only = 0;
    changed = handle_key(key);
    return changed || screen != was_screen || game != was_game || item != was_item || item_top != was_top
           || setting != was_setting || value != was_value || maze != was_maze || pairs_mode != was_mode || help_page != was_page;
}

void menu_seed(uint16_t seed)
{
    game_rand16_seed = seed;
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
    games_rumble_elapse(MENU_FRAME_US);
    switch (screen) {
    case SCREEN_MAIN:
        if (icon_steps == GAMES_ICON_STEPS)
            break;
        for (icon_us += MENU_FRAME_US; icon_us >= PHONE_TICK_US; icon_us -= PHONE_TICK_US) {
            if (--icon_ticks)
                continue;
            icon_ticks = GAMES_ICON_STEP;
            sparkle_only = 1;
            changed = 1;
            if (++icon_steps == GAMES_ICON_STEPS)
                break;
        }
        break;
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
    case SCREEN_DONE:
    case SCREEN_MAZE_DONE:
        if (--page_ticks == 0) {
            screen = screen == SCREEN_DONE ? SCREEN_SETTINGS : SCREEN_GAME;
            return 1;
        }
        play_us += MENU_FRAME_US;
        while (play_us >= PHONE_TICK_US) {
            play_us -= PHONE_TICK_US;
            done_ticks++;
            if (done_ticks == DONE_STEP_1 || done_ticks == DONE_STEP_2) {
                done_step++;
                sparkle_only = 1;
                changed = 1;
            }
        }
        break;
    case SCREEN_FIREWORKS:
        play_us += MENU_FRAME_US;
        while (play_us >= PHONE_TICK_US) {
            play_us -= PHONE_TICK_US;
            if (++fireworks_ticks < FIREWORKS_STEP)
                continue;
            fireworks_ticks = 0;
            changed = 1;
            if (++fireworks_step == FIREWORKS_STEPS) {
                screen = SCREEN_GAME_OVER;
                page_ticks = GAME_OVER_TICKS;
                fireworks_sound();
                break;
            }
        }
        break;
    case SCREEN_TITLE: {
        uint8_t what = title_elapse(MENU_FRAME_US);

        if (what & TITLE_OVER) {
            title_over();
            return 1;
        }
        changed = what;
        break;
    }
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
    uint8_t mode = full_screen && screen != SCREEN_MAIN && screen != SCREEN_PLAY && screen != SCREEN_TITLE
                   && screen != SCREEN_FIREWORKS ? VIEW_NATIVE : VIEW_PHONE;

    uint8_t selection;

    /* Snake II in the full-screen variant has a board of its own, drawn
       into the framebuffer as the menus are. */
    if (screen == SCREEN_PLAY && snake2_full && games_playing == GAME_SNAKE)
        mode = VIEW_BOARD;
    menu_drew_picture = (screen == SCREEN_PLAY && mode != VIEW_BOARD) || screen == SCREEN_TITLE
                        || screen == SCREEN_FIREWORKS;
    /* A step of the Games icon's animation changes only the icon, and the
       first screen's hint around the LCD stays as it is. */
    if (sparkle_only && screen == SCREEN_MAIN) {
        sparkle_only = 0;
        lcd_view_phone();
        draw_games_icon();
        return;
    }
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
    } else if (mode == VIEW_BOARD) {
        lcd_view_set(BOARD_AREA_X, BOARD_AREA_Y, BOARD_AREA_W, BOARD_AREA_H);
        if (LCD_ZOOM > 1)
            lcd_zoom_set(BOARD_AREA_X, BOARD_AREA_Y, BOARD_AREA_W, BOARD_AREA_H, LCD_ZOOM);
        else
            lcd_zoom_set(0, 0, 0, 0, 1);
    } else {
        lcd_view_full();
        lcd_zoom_set(0, 0, 0, 0, 1);
    }

    if (mode == VIEW_NATIVE && native_update())
        return;
    /* A step of the Top score or Done page's animation leaves the rest as
       it is. */
    if (sparkle_only) {
        sparkle_only = 0;
        if (screen == SCREEN_TOP_SCORE && !full_screen) {
            draw_sparkle();
            return;
        }
        if (screen == SCREEN_DONE || screen == SCREEN_MAZE_DONE) {
            draw_done_tick();
            return;
        }
    }
#ifdef NATIVE_PLATFORM_TILES
    native_tiled = 0;
    if (mode == VIEW_NATIVE) {
        uint16_t id = menu_native_id();

        if (id != NATIVE_NONE && platform_native_show(id)) {
            native_tiled = 1;
            drawn_screen = screen;
            drawn_selection = native_selection();
            if (native_listed())
                platform_native_cursor(drawn_selection, 1);
            return;
        }
    }
    platform_native_blank();
#endif
    drawn_screen = NO_SCREEN;

    if (screen != SCREEN_PLAY && screen != SCREEN_TITLE && screen != SCREEN_FIREWORKS) {
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
            native_note(text_top_score, "%N", top_score);
        } else {
            draw_note(text_top_score_value, top_score);
            draw_sparkle();
        }
        break;
    case SCREEN_GAME_OVER:
        if (full_screen)
            native_note(0, game_over_text(), final_score);
        else
            draw_note(game_over_text(), final_score);
        break;
    case SCREEN_PLAY:
        games_draw(!board_drawn);
        board_drawn = 1;
        break;
    case SCREEN_TITLE:
        /* The title is the phone's picture, shown as the game is. */
        title_draw();
        games_strip = 0;
        sprite_present(!board_drawn);
        board_drawn = 1;
        break;
    case SCREEN_FIREWORKS:
        fireworks_draw((uint8_t)(fireworks_step % FIREWORKS_PICTURES));
        games_strip = 0;
        sprite_present(!board_drawn);
        board_drawn = 1;
        break;
    case SCREEN_HELP:
        draw_help();
        break;
    case SCREEN_ABOUT:
        native_about();
        break;
    case SCREEN_SETTINGS:
        if (full_screen)
            native_settings();
        else
            draw_setting();
        break;
    case SCREEN_SETTING_VALUE:
        draw_setting_value();
        break;
    case SCREEN_DONE:
        draw_note(text_done, 0);
        draw_done_tick();
        break;
    case SCREEN_LEVEL:
        if (full_screen) {
            native_note(text_level, "%N", (uint16_t)(level + 1));
            native_hint(text_hint_select);
        } else {
            draw_level();
        }
        break;
    case SCREEN_MAZES:
        if (full_screen)
            native_mazes();
        else
            draw_mazes();
        break;
    case SCREEN_MODES:
        if (full_screen)
            native_modes();
        else
            draw_modes();
        break;
    case SCREEN_MAZE_DONE:
        draw_maze_done();
        draw_done_tick();
        break;
    default:
        if (full_screen)
            native_game();
        else
            draw_game();
        break;
    }
    if (mode == VIEW_NATIVE) {
        selection = native_selection();
        drawn_screen = screen;
        drawn_selection = selection;
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
        case 'z':
            /* The seed the phone's games start from after power-on. */
            menu_seed(1);
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
