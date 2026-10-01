#include "lcd.h"

#include <string.h>

uint8_t lcd_fb[LCD_WIDTH * LCD_HEIGHT];

static void put_pixel(int x, int y, uint8_t color)
{
    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT)
        lcd_fb[y * LCD_WIDTH + x] = color;
}

void lcd_clear(void)
{
    memset(lcd_fb, 0, sizeof lcd_fb);
}

void lcd_fill_rect(int x, int y, int w, int h, uint8_t color)
{
    int i, j;

    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            put_pixel(x + i, y + j, color);
}

void lcd_blit_bitmap(int x, int y, int w, int h, const uint8_t *data)
{
    int bytes_per_column = (h + 7) / 8;
    int i, j;

    for (i = 0; i < w; i++)
        for (j = 0; j < h; j++)
            put_pixel(x + i, y + j, (data[i * bytes_per_column + (j >> 3)] >> (j & 7)) & 1);
}
