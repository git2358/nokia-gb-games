#include "lcd.h"

uint8_t lcd_fb[LCD_WIDTH * LCD_HEIGHT];

static void put_pixel(int x, int y, uint8_t color)
{
    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT)
        lcd_fb[y * LCD_WIDTH + x] = color;
}

void lcd_clear(void)
{
    unsigned i;

    for (i = 0; i < sizeof lcd_fb; i++)
        lcd_fb[i] = 0;
}

void lcd_fill_rect(int x, int y, int w, int h, uint8_t color)
{
    uint8_t *row, *p;
    int i;

    /* Clip once, then walk the rows without a multiply per pixel. */
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (w > LCD_WIDTH - x)
        w = LCD_WIDTH - x;
    if (h > LCD_HEIGHT - y)
        h = LCD_HEIGHT - y;
    if (w <= 0 || h <= 0)
        return;

    row = lcd_fb + y * LCD_WIDTH + x;
    for (; h; h--, row += LCD_WIDTH)
        for (p = row, i = w; i; i--)
            *p++ = color;
}

void lcd_blit_bitmap(int x, int y, int w, int h, const uint8_t *data)
{
    int bytes_per_column = (h + 7) / 8;
    int i, j;

    for (i = 0; i < w; i++)
        for (j = 0; j < h; j++)
            put_pixel(x + i, y + j, (data[i * bytes_per_column + (j >> 3)] >> (j & 7)) & 1);
}

void lcd_blit_strips(int x, int y, int w, int h, const uint8_t *data)
{
    int i, j;

    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            put_pixel(x + i, y + j, (data[i + w * (j >> 3)] >> (j & 7)) & 1);
}
