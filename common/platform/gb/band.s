;; Putting a band of a picture into another, as core/sprite.c's draw_band
;; does, for a Game Boy port that defines SPRITE_PLATFORM_BAND: the
;; compiler makes far too slow work of it. In the ROM's first bank.
	.module band
	.globl	_sprite_band, _sprite_band_dst, _sprite_band_src
	.globl	_sprite_band_n, _sprite_band_up, _sprite_band_valid, _sprite_band_mode

	.area	_DATA
_sprite_band_dst::
	.ds	2
_sprite_band_src::
	.ds	2
_sprite_band_n::
	.ds	1
_sprite_band_up::
	.ds	1
_sprite_band_valid::
	.ds	1
_sprite_band_mode::
	.ds	1
;; The rotates that move a byte of bitmap to its rows, then a return: code
;; written here by sprite_band for each call.
band_rotate:
	.ds	8

	.area	_CODE

;; void sprite_band(void): sprite_band_n (at least 1) bytes of bitmap from
;; sprite_band_src onto the picture's bytes from sprite_band_dst. Each is
;; first moved up sprite_band_up rows (down, when negative); only the rows
;; in sprite_band_valid are touched, as sprite_band_mode says. The move is
;; a rotate: the rows that come round the other end are never valid.
_sprite_band::
	ld	a, (#_sprite_band_src)
	ld	e, a
	ld	a, (#_sprite_band_src + 1)
	ld	d, a
	ld	a, (#_sprite_band_n)
	ld	b, a
	ld	a, (#_sprite_band_valid)
	ld	c, a
	ld	a, (#_sprite_band_up)
	or	a, a
	jr	z, plain_band

	;; Write the rotates: rlca to move up, rrca to move down.
	push	bc
	ld	c, #0x07		; rlca
	bit	7, a
	jr	z, 1$
	cpl
	inc	a
	ld	c, #0x0f		; rrca
1$:
	ld	b, a
	ld	hl, #band_rotate
2$:
	ld	(hl), c
	inc	hl
	dec	b
	jr	nz, 2$
	ld	(hl), #0xc9		; ret
	pop	bc

	ld	a, (#_sprite_band_dst)
	ld	l, a
	ld	a, (#_sprite_band_dst + 1)
	ld	h, a
	ld	a, (#_sprite_band_mode)
	or	a, a
	jr	z, moved_background
	dec	a
	jr	z, moved_set
	dec	a
	jr	z, moved_flip
	dec	a
	jr	z, moved_inverse
moved_opaque:				; valid rows become the bitmap's
	ld	a, (de)
	inc	de
	call	band_rotate
	xor	a, (hl)
	and	a, c
	xor	a, (hl)
	ld	(hl+), a
	dec	b
	jr	nz, moved_opaque
	ret
moved_inverse:				; valid rows become the bitmap's, inverted
	ld	a, (de)
	inc	de
	call	band_rotate
	cpl
	xor	a, (hl)
	and	a, c
	xor	a, (hl)
	ld	(hl+), a
	dec	b
	jr	nz, moved_inverse
	ret
moved_flip:				; set bits flip
	ld	a, (de)
	inc	de
	call	band_rotate
	and	a, c
	xor	a, (hl)
	ld	(hl+), a
	dec	b
	jr	nz, moved_flip
	ret
moved_set:				; set bits set
	ld	a, (de)
	inc	de
	call	band_rotate
	and	a, c
	or	a, (hl)
	ld	(hl+), a
	dec	b
	jr	nz, moved_set
	ret
moved_background:			; clear bits clear
	ld	a, (de)
	inc	de
	call	band_rotate
	cpl
	and	a, c
	cpl
	and	a, (hl)
	ld	(hl+), a
	dec	b
	jr	nz, moved_background
	ret

plain_band:
	ld	a, (#_sprite_band_dst)
	ld	l, a
	ld	a, (#_sprite_band_dst + 1)
	ld	h, a
	ld	a, (#_sprite_band_mode)
	or	a, a
	jr	z, plain_background
	dec	a
	jr	z, plain_set
	dec	a
	jr	z, plain_flip
	dec	a
	jr	z, plain_inverse
plain_opaque:
	ld	a, (de)
	inc	de
	xor	a, (hl)
	and	a, c
	xor	a, (hl)
	ld	(hl+), a
	dec	b
	jr	nz, plain_opaque
	ret
plain_inverse:
	ld	a, (de)
	inc	de
	cpl
	xor	a, (hl)
	and	a, c
	xor	a, (hl)
	ld	(hl+), a
	dec	b
	jr	nz, plain_inverse
	ret
plain_flip:
	ld	a, (de)
	inc	de
	and	a, c
	xor	a, (hl)
	ld	(hl+), a
	dec	b
	jr	nz, plain_flip
	ret
plain_set:
	ld	a, (de)
	inc	de
	and	a, c
	or	a, (hl)
	ld	(hl+), a
	dec	b
	jr	nz, plain_set
	ret
plain_background:
	ld	a, (de)
	inc	de
	cpl
	and	a, c
	cpl
	and	a, (hl)
	ld	(hl+), a
	dec	b
	jr	nz, plain_background
	ret
