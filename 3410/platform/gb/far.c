#include "far.h"

#include "snake2.h"
#include "title.h"

#define MBC_ROM_BANK (*(volatile uint8_t *)0x2000)

static uint8_t mapped = BANK_MENU;

void far_bank(uint8_t bank)
{
    mapped = bank;
    MBC_ROM_BANK = bank;
}

int far_snake2_handler(int event, struct game_context *ctx)
{
    uint8_t was = mapped;
    int result;

    far_bank(BANK_SNAKE);
    result = snake2_handler(event, ctx);
    far_bank(was);
    return result;
}

/* The title and the game-over picture are drawn from Snake II's data, in
   its bank. */
void far_title_start(void)
{
    uint8_t was = mapped;

    far_bank(BANK_SNAKE);
    title_start();
    far_bank(was);
}

uint8_t far_title_elapse(uint16_t us)
{
    uint8_t was = mapped, what;

    far_bank(BANK_SNAKE);
    what = title_elapse(us);
    far_bank(was);
    return what;
}

void far_title_draw(void)
{
    uint8_t was = mapped;

    far_bank(BANK_SNAKE);
    title_draw();
    far_bank(was);
}

void far_over_start(uint16_t score, uint8_t blink)
{
    uint8_t was = mapped;

    far_bank(BANK_SNAKE);
    over_start(score, blink);
    far_bank(was);
}

uint8_t far_over_elapse(uint16_t us)
{
    uint8_t was = mapped, what;

    far_bank(BANK_SNAKE);
    what = over_elapse(us);
    far_bank(was);
    return what;
}

void far_over_draw(void)
{
    uint8_t was = mapped;

    far_bank(BANK_SNAKE);
    over_draw();
    far_bank(was);
}

void far_snake2_redraw(void)
{
    uint8_t was = mapped;

    far_bank(BANK_SNAKE);
    snake2_redraw();
    far_bank(was);
}

void far_scores_start(uint16_t top, uint16_t last, uint8_t show_last, uint8_t kind)
{
    uint8_t was = mapped;

    far_bank(BANK_SNAKE);
    scores_start(top, last, show_last, kind);
    far_bank(was);
}

uint8_t far_scores_elapse(uint16_t us)
{
    uint8_t was = mapped, what;

    far_bank(BANK_SNAKE);
    what = scores_elapse(us);
    far_bank(was);
    return what;
}

void far_scores_draw(void)
{
    uint8_t was = mapped;

    far_bank(BANK_SNAKE);
    scores_draw();
    far_bank(was);
}
