#include "lcd.h"

uint8_t lcd_fb[LCD_STRIDE * LCD_FB_HEIGHT];
uint8_t lcd_dirty[LCD_CELLS_X * LCD_CELLS_Y];

const uint8_t lcd_bit[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

uint8_t lcd_view_x = LCD_PHONE_X, lcd_view_y = LCD_PHONE_Y;
uint8_t lcd_view_w = LCD_WIDTH, lcd_view_h = LCD_HEIGHT;

void lcd_view_phone(void)
{
    lcd_view_x = LCD_PHONE_X;
    lcd_view_y = LCD_PHONE_Y;
    lcd_view_w = LCD_WIDTH;
    lcd_view_h = LCD_HEIGHT;
}

void lcd_view_full(void)
{
    lcd_view_x = 0;
    lcd_view_y = 0;
    lcd_view_w = LCD_FB_WIDTH;
    lcd_view_h = LCD_FB_HEIGHT;
}

void lcd_view_set(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    lcd_view_x = x;
    lcd_view_y = y;
    lcd_view_w = w;
    lcd_view_h = h;
}

void lcd_mark_dirty(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint8_t first = x >> 3, last = (uint8_t)((x + w - 1) >> 3);
    uint8_t cy = y >> 3, last_y = (uint8_t)((y + h - 1) >> 3), cx;
    uint8_t *row = lcd_dirty + cy * LCD_CELLS_X;

    for (; cy <= last_y; cy++, row += LCD_CELLS_X)
        for (cx = first; cx <= last; cx++)
            row[cx] = 1;
}

static void put_pixel(int x, int y, uint8_t color)
{
    uint8_t *p;

    if (x < 0 || x >= lcd_view_w || y < 0 || y >= lcd_view_h)
        return;
    x += lcd_view_x;
    y += lcd_view_y;
    lcd_dirty[(x >> 3) + LCD_CELLS_X * (y >> 3)] = 1;
    p = lcd_fb + y * LCD_STRIDE + (x >> 3);
    if (color)
        *p |= lcd_bit[x & 7];
    else
        *p &= (uint8_t)~lcd_bit[x & 7];
}

void lcd_clear(void)
{
    unsigned i;

    if (lcd_view_w != LCD_FB_WIDTH || lcd_view_h != LCD_FB_HEIGHT) {
        lcd_fill_rect(0, 0, lcd_view_w, lcd_view_h, 0);
        return;
    }
    for (i = 0; i < sizeof lcd_fb; i++)
        lcd_fb[i] = 0;
    for (i = 0; i < sizeof lcd_dirty; i++)
        lcd_dirty[i] = 1;
}

void lcd_fill_rect(int x, int y, int w, int h, uint8_t color)
{
    uint8_t *row;
    uint8_t first, count, i, bit;

    /* Clip once, then walk the rows without a multiply per pixel. */
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (w > lcd_view_w - x)
        w = lcd_view_w - x;
    if (h > lcd_view_h - y)
        h = lcd_view_h - y;
    if (w <= 0 || h <= 0)
        return;
    x += lcd_view_x;
    y += lcd_view_y;

    lcd_mark_dirty((uint8_t)x, (uint8_t)y, (uint8_t)w, (uint8_t)h);
    first = (uint8_t)(x & 7);
    count = (uint8_t)w;
    for (row = lcd_fb + y * LCD_STRIDE + (x >> 3); h; h--, row += LCD_STRIDE) {
        uint8_t *p = row;

        bit = first;
        for (i = count; i; i--) {
            if (color)
                *p |= lcd_bit[bit];
            else
                *p &= (uint8_t)~lcd_bit[bit];
            if (++bit == 8) {
                bit = 0;
                p++;
            }
        }
    }
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
