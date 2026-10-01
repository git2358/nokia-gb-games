;; Game Boy startup: interrupt vectors, RAM set-up, and a vertical-blank
;; handler that counts frames. makebin fills in the cartridge header.
	.module crt0
	.globl	_main
	.globl	_flush_tiles
	.globl	_frame_count
	.globl	_staged, _staged_at, _staged_count
	.globl	s__INITIALIZER, s__INITIALIZED, l__INITIALIZER

	.area	_HEADER (ABS)
	.org	0x40		; vertical blank
	jp	vblank
	.org	0x48		; LCD status
	reti
	.org	0x50		; timer
	reti
	.org	0x58		; serial
	reti
	.org	0x60		; joypad
	reti

	.org	0x100
	nop
	jp	init

	.org	0x150
init:
	di
	ld	sp, #0xe000

	;; Clear work RAM.
	ld	hl, #0xc000
	ld	bc, #0x2000
1$:
	xor	a, a
	ld	(hl+), a
	dec	bc
	ld	a, b
	or	a, c
	jr	nz, 1$

	;; Copy the initial values of initialised variables.
	ld	hl, #s__INITIALIZER
	ld	de, #s__INITIALIZED
	ld	bc, #l__INITIALIZER
2$:
	ld	a, b
	or	a, c
	jr	z, 3$
	ld	a, (hl+)
	ld	(de), a
	inc	de
	dec	bc
	jr	2$
3$:
	call	_main
4$:
	halt
	jr	4$

vblank:
	push	af
	push	hl
	ld	hl, #_frame_count
	inc	(hl)
	pop	hl
	pop	af
	reti

;; void flush_tiles(void): waits for the start of the next vertical blank,
;; then copies staged_count 16-byte tiles from staged to the video RAM
;; addresses in staged_at. About 180 cycles a tile; a vertical blank is 1140.
_flush_tiles::
1$:
	ld	a, (#0xff44)
	cp	a, #144
	jr	z, 1$
2$:
	ld	a, (#0xff44)
	cp	a, #144
	jr	nz, 2$
	ld	a, (#_staged_count)
	or	a, a
	ret	z
	ld	c, a
	ld	de, #_staged
	ld	hl, #_staged_at
3$:
	push	hl
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	b, #16
4$:
	ld	a, (de)
	ld	(hl+), a
	inc	de
	dec	b
	jr	nz, 4$
	pop	hl
	inc	hl
	inc	hl
	dec	c
	jr	nz, 3$
	ret

	;; Order of the areas for the linker.
	.area	_HOME
	.area	_CODE
	.area	_INITIALIZER
	.area	_GSINIT
	.area	_GSFINAL
	.area	_DATA
	.area	_INITIALIZED
	.area	_BSEG
	.area	_BSS
	.area	_HEAP
