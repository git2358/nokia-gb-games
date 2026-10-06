;; Copying for the full-screen menus made at build time (native_gb.h).
;; In the first bank: native_get_far and native_row map the tiles' bank to
;; read, then native_bank again, from where they were called.
;;
;; Video RAM accepts writes only outside the part of each line where the
;; LCD controller is drawing, so each four bytes wait for a horizontal or
;; vertical blank, as in flush_tiles: the check takes 4 cycles after it
;; reads the status and the writes 16, so even when the blank ends right
;; after the read they land inside the 20 cycles of the following line's
;; OAM scan, which still accepts them. Interrupts are held off between the
;; check and the writes.
	.module native_put
	.globl	_native_bank, _native_tile_bank, _native_tiles

EMPTY_ID = 0xfe
FULL_ID = 0xfd

	.area	_DATA
src:
	.ds	2
four:
	.ds	8
_native_first::
	.ds	1
_native_map_row::
	.ds	20
_native_old::
	.ds	40
row_tiles:
	.ds	2
next:
	.ds	2
cells:
	.ds	1

	.area	_HOME

;; void native_put_tile(uint8_t *tile, const uint8_t *bytes)
_native_put_tile::
	ld	a, c
	ld	(#src), a
	ld	a, b
	ld	(#src + 1), a
	ld	h, d
	ld	l, e
	call	put_four
;; The next four bytes from src to every other byte from hl on.
put_four:
	push	hl
	ld	a, (#src)
	ld	l, a
	ld	a, (#src + 1)
	ld	h, a
	ld	a, (hl+)
	ld	b, a
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	ld	d, a
	ld	a, (hl+)
	ld	e, a
	ld	a, l
	ld	(#src), a
	ld	a, h
	ld	(#src + 1), a
	pop	hl
	di
1$:
	ld	a, (#0xff41)
	and	a, #0x02
	jr	nz, 1$
	ld	(hl), b
	inc	l
	inc	l
	ld	(hl), c
	inc	l
	inc	l
	ld	(hl), d
	inc	l
	inc	l
	ld	(hl), e
	inc	l
	inc	l
	ei
	ret

;; void native_get_far(uint8_t *out, const uint8_t *bytes)
_native_get_far::
	ld	h, b
	ld	l, c
	di
	ld	a, (#_native_tile_bank)
	ld	(#0x2000), a
	ld	c, #8
1$:
	ld	a, (hl+)
	ld	(de), a
	inc	de
	dec	c
	jr	nz, 1$
	ld	a, (#_native_bank)
	ld	(#0x2000), a
	ei
	ret

;; void native_put_row(uint8_t *map, const uint8_t *bytes)
_native_put_row::
	ld	h, b
	ld	l, c
	ld	c, #5
1$:
	push	bc
	ld	a, (hl+)
	ld	b, a
	ld	a, (hl+)
	ld	c, a
	push	de
	ld	a, (hl+)
	ld	d, a
	ld	a, (hl+)
	ld	e, a
	push	hl
	;; hl = where they go, from the stack under the source
	ld	hl, #2
	add	hl, sp
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	di
2$:
	ld	a, (#0xff41)
	and	a, #0x02
	jr	nz, 2$
	ld	(hl), b
	inc	l
	ld	(hl), c
	inc	l
	ld	(hl), d
	inc	l
	ld	(hl), e
	inc	l
	ei
	ld	d, h
	ld	e, l
	pop	hl
	pop	bc			; the old destination, dropped
	pop	bc
	dec	c
	jr	nz, 1$
	ret

;; void native_put_byte(uint8_t *at, uint8_t value)
_native_put_byte::
	ld	b, a
	di
1$:
	ld	a, (#0xff41)
	and	a, #0x02
	jr	nz, 1$
	ld	a, b
	ld	(de), a
	ei
	ret

;; const uint16_t *native_row(const uint16_t *row, uint8_t *tiles): a row
;; of a screen (native_gen.c): its made cells' tiles copied from the tiles'
;; bank to theirs, from `tiles` on, but where native_old has the same tile
;; for the cell, and native_map_row filled in. Returns the next row.
_native_row::
	ld	a, c
	ld	(#row_tiles), a
	ld	a, b
	ld	(#row_tiles + 1), a
	ld	hl, #_native_map_row
	ld	a, #EMPTY_ID
	ld	c, #20
1$:
	ld	(hl+), a
	dec	c
	jr	nz, 1$
	ld	h, d
	ld	l, e
	ld	a, (hl+)
	inc	hl
	ld	(#cells), a
	or	a, a
	jp	z, 9$
2$:
	ld	a, (hl+)
	ld	e, a			; the tile, low byte
	ld	a, (hl+)
	ld	d, a			; the column << 3, and the tile's high bits
	ld	a, l
	ld	(#next), a
	ld	a, h
	ld	(#next + 1), a
	ld	a, d
	rrca
	rrca
	rrca
	and	a, #0x1f
	ld	c, a			; the column
	ld	hl, #_native_map_row
	add	a, l
	ld	l, a
	jr	nc, 3$
	inc	h
3$:
	ld	a, d
	and	a, #0x07
	ld	d, a			; de = the tile
	or	a, a
	jr	nz, 4$
	ld	a, e
	cp	a, #1
	jr	nz, 4$
	ld	(hl), #FULL_ID
	jr	8$
4$:
	ld	a, (#_native_first)
	add	a, c
	ld	(hl), a			; the cell's own tile
	;; Nothing to copy when the screen up has the same tile there.
	ld	hl, #_native_old
	ld	a, c
	add	a, a
	add	a, l
	ld	l, a
	jr	nc, 41$
	inc	h
41$:
	ld	a, (hl+)
	cp	a, e
	jr	nz, 42$
	ld	a, (hl)
	cp	a, d
	jp	z, 8$
42$:
	dec	de
	dec	de
	ld	h, d
	ld	l, e
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	de, #_native_tiles
	add	hl, de			; its bytes, in the tiles' bank
	di
	ld	a, (#_native_tile_bank)
	ld	(#0x2000), a
	ld	de, #four
	ld	b, #8
5$:
	ld	a, (hl+)
	ld	(de), a
	inc	de
	dec	b
	jr	nz, 5$
	ld	a, (#_native_bank)
	ld	(#0x2000), a
	ei
	ld	h, #0
	ld	l, c
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	a, (#row_tiles)
	add	a, l
	ld	e, a
	ld	a, (#row_tiles + 1)
	adc	a, h
	ld	d, a			; where the cell's tile goes
	ld	bc, #four
	call	_native_put_tile
8$:
	ld	a, (#next)
	ld	l, a
	ld	a, (#next + 1)
	ld	h, a
	ld	a, (#cells)
	dec	a
	ld	(#cells), a
	jp	nz, 2$
9$:
	ld	b, h
	ld	c, l
	ret

;; void native_fb_row(uint8_t *tiles, const uint8_t *fb): the 20 cells of a
;; row of lcd_fb, from its first byte `fb`, to the first bit plane of their
;; tiles, from `tiles` on. A framebuffer row is 20 bytes.
_native_fb_row::
	ld	a, #20
	ld	(#cells), a
1$:
	push	de
	push	bc
	ld	h, b
	ld	l, c
	ld	de, #four
	ld	c, #8
2$:
	ld	a, (hl)
	ld	(de), a
	inc	de
	ld	a, l
	add	a, #20
	ld	l, a
	jr	nc, 3$
	inc	h
3$:
	dec	c
	jr	nz, 2$
	pop	bc
	pop	de
	push	de
	push	bc
	ld	bc, #four
	call	_native_put_tile
	pop	bc
	pop	de
	inc	bc
	ld	hl, #16
	add	hl, de
	ld	d, h
	ld	e, l
	ld	a, (#cells)
	dec	a
	ld	(#cells), a
	jr	nz, 1$
	ret
