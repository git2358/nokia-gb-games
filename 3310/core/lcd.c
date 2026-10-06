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

uint8_t lcd_zoom_x, lcd_zoom_y, lcd_zoom_w, lcd_zoom_h, lcd_zoom_by = 1;

void lcd_zoom_set(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t by)
{
    lcd_zoom_x = x;
    lcd_zoom_y = y;
    lcd_zoom_w = w;
    lcd_zoom_h = h;
    lcd_zoom_by = by;
}

uint8_t lcd_screen_pixel(uint8_t x, uint8_t y)
{
    if (lcd_zoom_w) {
        int zx = x - (LCD_FB_WIDTH - lcd_zoom_w * lcd_zoom_by) / 2;
        int zy = y - (LCD_FB_HEIGHT - lcd_zoom_h * lcd_zoom_by) / 2;

        if (zx >= 0 && zx < lcd_zoom_w * lcd_zoom_by && zy >= 0 && zy < lcd_zoom_h * lcd_zoom_by) {
            x = (uint8_t)(lcd_zoom_x + zx / lcd_zoom_by);
            y = (uint8_t)(lcd_zoom_y + zy / lcd_zoom_by);
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

uint8_t lcd_column_rows, lcd_column_color, lcd_column_bits;

#ifndef LCD_PLATFORM_COLUMNS
void lcd_column_fill(uint8_t *p, uint8_t mask)
{
    uint8_t rows;

    for (rows = lcd_column_rows; rows; rows--, p += LCD_STRIDE) {
        if (lcd_column_color == LCD_INVERT)
            *p ^= mask;
        else if (lcd_column_color)
            *p |= mask;
        else
            *p &= (uint8_t)~mask;
    }
}

void lcd_column_blit(uint8_t *p, uint8_t mask)
{
    uint8_t rows, bits = lcd_column_bits;

    for (rows = lcd_column_rows; rows; rows--, p += LCD_STRIDE, bits >>= 1) {
        if (bits & 1)
            *p |= mask;
        else
            *p &= (uint8_t)~mask;
    }
}
#endif

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
    uint8_t *p;
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

    /* A column of bytes at a time: the first and last are masked, the ones
       between are written whole. */
    first = from_bit[x & 7];
    last = up_to_bit[(x + w - 1) & 7];
    count = (uint8_t)(((x + w - 1) >> 3) - (x >> 3));
    if (!count)
        first &= last;
    p = lcd_fb + y * LCD_STRIDE + (x >> 3);
    lcd_column_rows = (uint8_t)h;
    lcd_column_color = color;
    lcd_column_fill(p, first);
    if (!count)
        return;
    for (i = count - 1; i; i--)
        lcd_column_fill(++p, 0xff);
    lcd_column_fill(++p, last);
}

void lcd_blit_bitmap(int x, int y, int w, int h, const uint8_t *data)
{
    int bytes_per_column = (h + 7) / 8;
    int i, j;

    if (x >= 0 && y >= 0 && x + w <= lcd_view_w && y + h <= lcd_view_h) {
        /* All of it is inside the view: a column at a time, eight rows of
           it at once, without the checks a pixel at a time needs. */
        uint8_t fx = (uint8_t)(x + lcd_view_x), fy = (uint8_t)(y + lcd_view_y);
        uint8_t *top = lcd_fb + fy * LCD_STRIDE;
        uint8_t columns, left;

        lcd_mark_dirty(fx, fy, (uint8_t)w, (uint8_t)h);
        if (h <= 8) {
            /* The usual case: a byte of the bitmap is a whole column. */
            lcd_column_rows = (uint8_t)h;
            for (columns = (uint8_t)w; columns; columns--, fx++) {
                lcd_column_bits = *data++;
                lcd_column_blit(top + (fx >> 3), lcd_bit[fx & 7]);
            }
            return;
        }
        for (columns = (uint8_t)w; columns; columns--, fx++) {
            uint8_t *p = top + (fx >> 3);
            uint8_t mask = lcd_bit[fx & 7];

            for (left = (uint8_t)h; left; left -= lcd_column_rows, p += 8 * LCD_STRIDE) {
                lcd_column_rows = left > 8 ? 8 : left;
                lcd_column_bits = *data++;
                lcd_column_blit(p, mask);
            }
        }
        return;
    }
    for (i = 0; i < w; i++)
        for (j = 0; j < h; j++)
            put_pixel(x + i, y + j, (data[i * bytes_per_column + (j >> 3)] >> (j & 7)) & 1);
}

void lcd_blit_strips(int x, int y, int w, int h, const uint8_t *data)
{
    int i, j;

    if (x >= 0 && y >= 0 && x + w <= lcd_view_w && y + h <= lcd_view_h) {
        /* All of it is inside the view: a band of 8 rows at a time, a byte
           of it a column of the band, as lcd_blit_bitmap does. */
        uint8_t fx = (uint8_t)(x + lcd_view_x), fy = (uint8_t)(y + lcd_view_y), cx, columns, left;
        uint8_t *top = lcd_fb + fy * LCD_STRIDE;

        lcd_mark_dirty(fx, fy, (uint8_t)w, (uint8_t)h);
        for (left = (uint8_t)h; left; left -= lcd_column_rows, top += 8 * LCD_STRIDE) {
            lcd_column_rows = left > 8 ? 8 : left;
            for (columns = (uint8_t)w, cx = fx; columns; columns--, cx++) {
                lcd_column_bits = *data++;
                lcd_column_blit(top + (cx >> 3), lcd_bit[cx & 7]);
            }
        }
        return;
    }
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            put_pixel(x + i, y + j, (data[i + w * (j >> 3)] >> (j & 7)) & 1);
}
