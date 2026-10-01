#include "lcd.h"

uint8_t lcd_fb[LCD_STRIDE * LCD_HEIGHT];

const uint8_t lcd_bit[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

static void put_pixel(int x, int y, uint8_t color)
{
    uint8_t *p;

    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT)
        return;
    p = lcd_fb + y * LCD_STRIDE + (x >> 3);
    if (color)
        *p |= lcd_bit[x & 7];
    else
        *p &= (uint8_t)~lcd_bit[x & 7];
}

void lcd_clear(void)
{
    unsigned i;

    for (i = 0; i < sizeof lcd_fb; i++)
        lcd_fb[i] = 0;
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
    if (w > LCD_WIDTH - x)
        w = LCD_WIDTH - x;
    if (h > LCD_HEIGHT - y)
        h = LCD_HEIGHT - y;
    if (w <= 0 || h <= 0)
        return;

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
