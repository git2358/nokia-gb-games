#include "pgm.h"

#include <stdio.h>

#include "lcd.h"

int pgm_write_lcd(const char *path)
{
    FILE *f = fopen(path, "wb");
    int x, y;

    if (!f)
        return -1;
    fprintf(f, "P5\n%d %d\n255\n", LCD_WIDTH, LCD_HEIGHT);
    for (y = 0; y < LCD_HEIGHT; y++)
        for (x = 0; x < LCD_WIDTH; x++)
            fputc(lcd_pixel(x, y) ? 0 : 255, f);
    return fclose(f);
}
