#include "testcard.h"

#include "lcd.h"

/* 8x8 arrow pointing up and to the right, column-major. */
static const uint8_t arrow[8] = { 0x80, 0x40, 0x20, 0x11, 0x09, 0x05, 0x03, 0x1f };

void testcard_frame(void)
{
    lcd_clear();
    lcd_fill_rect(0, 0, LCD_WIDTH, 1, 1);
    lcd_fill_rect(0, LCD_HEIGHT - 1, LCD_WIDTH, 1, 1);
    lcd_fill_rect(0, 0, 1, LCD_HEIGHT, 1);
    lcd_fill_rect(LCD_WIDTH - 1, 0, 1, LCD_HEIGHT, 1);
}

void testcard_draw(void)
{
    int i;

    testcard_frame();

    /* Corner marks of different sizes, so a flip or rotation shows. */
    lcd_fill_rect(2, 2, 2, 2, 1);
    lcd_fill_rect(LCD_WIDTH - 6, 2, 4, 4, 1);
    lcd_fill_rect(2, LCD_HEIGHT - 8, 6, 6, 1);
    lcd_blit_bitmap(LCD_WIDTH - 10, LCD_HEIGHT - 10, 8, 8, arrow);

    /* Snake-like row of 3x3 blocks on a 4 px pitch. */
    for (i = 0; i < 12; i++)
        lcd_fill_rect(18 + i * 4, 22, 3, 3, 1);

    /* One-pixel checkerboard, to show scaling errors. */
    for (i = 0; i < 16 * 8; i++)
        lcd_fill_rect(34 + i % 16, 30 + i / 16, 1, 1, (uint8_t)((i % 16 + i / 16) & 1));
}
