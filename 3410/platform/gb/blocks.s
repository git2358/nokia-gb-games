;; The Game Boy's own version of the one thing screen.c does that the
;; compiler makes far too slow: turning an 8x8 block of the phone's picture,
;; eight column bytes (bit y of byte x the pixel (x, y)), into eight row
;; bytes of lcd_fb (bit 7 - x of byte y), 20 bytes apart. In the ROM's
;; first bank.
	.module blocks
	.globl	_gb_block_rows

	.area	_DATA
columns:
	.ds	8

	.area	_HOME
;; void gb_block_rows(const uint8_t *columns, uint8_t *row)
;; de: the column bytes, bc: the first row byte. Each row takes bit 0 of
;; every column in turn, leftmost first, and the columns are shifted down
;; a bit for the next row.
_gb_block_rows::
	ld	hl, #columns
	ld	a, #8
1$:
	push	af
	ld	a, (de)
	inc	de
	ld	(hl+), a
	pop	af
	dec	a
	jr	nz, 1$
	ld	h, b
	ld	l, c
	ld	b, #8			; rows
2$:
	push	hl
	ld	hl, #columns
	ld	c, #8			; columns
	ld	e, #0			; the row's byte
3$:
	ld	a, (hl)
	rra				; the column's pixel in this row to the carry
	ld	(hl+), a
	rl	e			; and into the row, the first column at the top
	dec	c
	jr	nz, 3$
	pop	hl
	ld	(hl), e
	ld	a, l			; the next row of lcd_fb
	add	a, #20
	ld	l, a
	jr	nc, 4$
	inc	h
4$:
	dec	b
	jr	nz, 2$
	ret
