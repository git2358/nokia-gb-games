#ifndef HOST_PGM_H
#define HOST_PGM_H

/* Writes the screen as a platform would show it, magnification applied, as
   a binary PGM (P5, maxval 255; set pixels are 0, clear pixels are 255). At
   the phone's 84x48 that is the layout of the fork's MAME LCD frames.
   Returns 0 on success. */
int pgm_write_lcd(const char *path);

#endif
