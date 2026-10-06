;; The Game Boy's own versions of the two things screen.c does that the
;; compiler makes far too slow: finding the 8x8 blocks of the phone's
;; picture that changed, and turning one, eight column bytes (bit y of byte
;; x the pixel (x, y)), into eight row bytes of lcd_fb (bit 7 - x of byte
;; y), 20 bytes apart. In the ROM's first bank.
	.module blocks
	.globl	_gb_block_rows, _gb_block_changed

	.area	_HOME
;; uint8_t gb_block_changed(const uint8_t *now, uint8_t *was)
;; de: the block's eight bytes, bc: what they were. Returns 0 when they are
;; the same; else 1, with them copied over what they were.
_gb_block_changed::
	ld	h, b
	ld	l, c
	ld	c, #8
1$:
	ld	a, (de)
	cp	a, (hl)
	jr	nz, 2$
	inc	de
	inc	hl
	dec	c
	jr	nz, 1$
	xor	a, a
	ret
2$:					; the rest copied, from the first that differs
	ld	a, (de)
	inc	de
	ld	(hl+), a
	dec	c
	jr	nz, 2$
	ld	a, #1
	ret

;; void gb_block_rows(const uint8_t *columns, uint8_t *row)
;; de: the column bytes, bc: the first row byte. Four rows at a time, in b,
;; c, d and e: each column's bits 0 to 3 (then 4 to 7) shifted into them in
;; turn, leftmost column first, so that it ends at the top bit.
_gb_block_rows::
	push	bc			; the first row byte
	push	de			; the columns, again for rows 4 to 7
	ld	h, d
	ld	l, e
	ld	a, (hl+)
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	pop	hl
	push	bc			; rows 0 and 1
	push	de			; rows 2 and 3
	ld	a, (hl+)
	swap	a
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	swap	a
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	swap	a
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	swap	a
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	swap	a
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	swap	a
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	swap	a
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	ld	a, (hl+)
	swap	a
	rra
	rl	b
	rra
	rl	c
	rra
	rl	d
	rra
	rl	e
	push	de			; rows 6 and 7
	push	bc			; rows 4 and 5
	;; Rows 0 to 3 back from the stack, then the first row byte.
	ldhl	sp, #4
	ld	a, (hl+)		; row 3 (e)
	ld	e, a
	ld	a, (hl+)		; row 2 (d)
	ld	d, a
	ld	a, (hl+)		; row 1 (c)
	ld	c, a
	ld	b, (hl)			; row 0 (b)
	ldhl	sp, #8
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	(hl), b
	ld	a, l
	add	a, #20
	ld	l, a
	ld	a, h
	adc	a, #0
	ld	h, a
	ld	(hl), c
	ld	a, l
	add	a, #20
	ld	l, a
	ld	a, h
	adc	a, #0
	ld	h, a
	ld	(hl), d
	ld	a, l
	add	a, #20
	ld	l, a
	ld	a, h
	adc	a, #0
	ld	h, a
	ld	(hl), e
	ld	a, l
	add	a, #20
	ld	l, a
	ld	a, h
	adc	a, #0
	ld	h, a
	pop	bc			; rows 4 and 5
	pop	de			; rows 6 and 7
	ld	(hl), b
	ld	a, l
	add	a, #20
	ld	l, a
	ld	a, h
	adc	a, #0
	ld	h, a
	ld	(hl), c
	ld	a, l
	add	a, #20
	ld	l, a
	ld	a, h
	adc	a, #0
	ld	h, a
	ld	(hl), d
	ld	a, l
	add	a, #20
	ld	l, a
	ld	a, h
	adc	a, #0
	ld	h, a
	ld	(hl), e
	add	sp, #6
	ret
