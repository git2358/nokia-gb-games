;; Game Boy startup: interrupt vectors, RAM set-up, and a vertical-blank
;; handler that counts frames. makebin fills in the cartridge header.
	.module crt0
	.globl	_main
	.globl	_flush_tiles
	.globl	_frame_count, _pad_last, _pad_latch
	.globl	_sound_frame
	.globl	_staged, _staged_at, _staged_count
	.globl	s__INITIALIZER, s__INITIALIZED, l__INITIALIZER

	.area	_HEADER (ABS)
	.org	0x40		; vertical blank
	jp	vblank
	.org	0x48		; LCD status
	jp	lcd_split
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

	;; The handlers and helpers go with the rest of the code: the header
	;; area above is at fixed addresses and ends where the code begins.
	.area	_HOME
vblank:
	push	af
	push	hl
	ld	hl, #_frame_count
	inc	(hl)
	;; The top of the screen takes its tiles from 0x8000.
	ld	hl, #0xff40
	set	4, (hl)

	;; Read the pad every frame and latch new presses, so none is lost
	;; or delayed while the main loop is drawing. Buttons go in the low
	;; half of the byte and directions in the high half.
	ld	a, #0x10		; select the buttons
	ld	(#0xff00), a
	ld	a, (#0xff00)
	ld	a, (#0xff00)
	cpl
	and	a, #0x0f
	ld	l, a
	ld	a, #0x20		; select the directions
	ld	(#0xff00), a
	ld	a, (#0xff00)
	ld	a, (#0xff00)
	ld	a, (#0xff00)
	ld	a, (#0xff00)
	cpl
	and	a, #0x0f
	swap	a
	or	a, l
	ld	l, a			; keys held now
	ld	a, #0x30
	ld	(#0xff00), a
	ld	a, (#_pad_last)
	cpl
	and	a, l			; keys newly pressed
	ld	h, a
	ld	a, (#_pad_latch)
	or	a, h
	ld	(#_pad_latch), a
	ld	a, l
	ld	(#_pad_last), a

	;; Advance the sound in progress.
	push	bc
	push	de
	call	_sound_frame
	pop	de
	pop	bc
	pop	hl
	pop	af
	reti

;; Raised on the line before the one where the screen's tiles continue at
;; 0x9000. Waits for that line's horizontal blank, so the switch never
;; lands in the middle of a drawn line.
lcd_split:
	push	af
	push	hl
	ld	hl, #0xff41
1$:
	ld	a, (hl)
	and	a, #0x03
	jr	nz, 1$
	ld	hl, #0xff40
	res	4, (hl)
	pop	hl
	pop	af
	reti

;; void flush_tiles(void): copies staged_count 16-byte tiles from staged to
;; the video RAM addresses in staged_at, with the LCD on. Video RAM accepts
;; writes only outside the part of each line where the LCD controller is
;; drawing, so each pair of bytes waits for a horizontal or vertical blank;
;; the writes then land within the first 10 cycles after the check, inside
;; the 20 that the following line's OAM scan still leaves. Interrupts are
;; held off between the check and the writes.
_flush_tiles::
	ld	a, (#_staged_count)
	or	a, a
	ret	z
	ld	de, #_staged
	ld	hl, #_staged_at
1$:
	push	af			; tiles left
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	push	hl
	ld	h, a
	ld	l, c			; hl = where this tile goes
2$:
	ld	a, (de)
	inc	de
	ld	b, a
	ld	a, (de)
	inc	de
	ld	c, a
	di
3$:
	ld	a, (#0xff41)
	and	a, #0x02
	jr	nz, 3$			; drawing, or about to: wait
	ld	(hl), b
	inc	hl
	ld	(hl), c
	inc	hl
	ei
	ld	a, l
	and	a, #0x0f
	jr	nz, 2$			; tiles start on 16-byte boundaries
	pop	hl
	pop	af
	dec	a
	jr	nz, 1$
	ret
