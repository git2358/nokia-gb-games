#include "pgm.h"

#include <stdio.h>

#include "lcd.h"

int pgm_write_lcd(const char *path)
{
    FILE *f = fopen(path, "wb");
    int i;

    if (!f)
        return -1;
    fprintf(f, "P5\n%d %d\n255\n", LCD_WIDTH, LCD_HEIGHT);
    for (i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++)
        fputc(lcd_fb[i] ? 0 : 255, f);
    return fclose(f);
}
