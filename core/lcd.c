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

uint8_t lcd_zoom_x, lcd_zoom_y, lcd_zoom_w, lcd_zoom_h;

void lcd_zoom_set(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    lcd_zoom_x = x;
    lcd_zoom_y = y;
    lcd_zoom_w = w;
    lcd_zoom_h = h;
}

uint8_t lcd_screen_pixel(uint8_t x, uint8_t y)
{
    if (lcd_zoom_w) {
        int zx = x - (LCD_FB_WIDTH - lcd_zoom_w * LCD_ZOOM) / 2;
        int zy = y - (LCD_FB_HEIGHT - lcd_zoom_h * LCD_ZOOM) / 2;

        if (zx >= 0 && zx < lcd_zoom_w * LCD_ZOOM && zy >= 0 && zy < lcd_zoom_h * LCD_ZOOM) {
            x = (uint8_t)(lcd_zoom_x + zx / LCD_ZOOM);
            y = (uint8_t)(lcd_zoom_y + zy / LCD_ZOOM);
        }
    }
    return lcd_fb_pixel(x, y);
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
    /* The bits of a byte from pixel n rightwards, and up to pixel n. */
    static const uint8_t from_bit[8] = { 0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01 };
    static const uint8_t up_to_bit[8] = { 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };
    uint8_t *row;
    uint8_t first, last, count, i;

    /* Clip to the view. */
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

    /* Whole bytes at a time: the first and last byte of a row are masked,
       the ones between are written outright. */
    first = from_bit[x & 7];
    last = up_to_bit[(x + w - 1) & 7];
    count = (uint8_t)(((x + w - 1) >> 3) - (x >> 3));
    if (!count)
        first &= last;
    for (row = lcd_fb + y * LCD_STRIDE + (x >> 3); h; h--, row += LCD_STRIDE) {
        uint8_t *p = row;

        if (color == LCD_INVERT)
            *p ^= first;
        else if (color)
            *p |= first;
        else
            *p &= (uint8_t)~first;
        if (!count)
            continue;
        for (i = count - 1; i; i--) {
            p++;
            *p = color == LCD_INVERT ? (uint8_t)~*p : color ? 0xff : 0x00;
        }
        p++;
        if (color == LCD_INVERT)
            *p ^= last;
        else if (color)
            *p |= last;
        else
            *p &= (uint8_t)~last;
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
