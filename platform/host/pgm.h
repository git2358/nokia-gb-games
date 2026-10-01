#ifndef HOST_PGM_H
#define HOST_PGM_H

/* Writes lcd_fb as a binary PGM in the layout of the fork's MAME LCD frames
   (P5, 84x48, maxval 255; set pixels are 0, clear pixels are 255). Returns 0
   on success. */
int pgm_write_lcd(const char *path);

#endif
