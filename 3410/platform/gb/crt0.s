;; Game Boy startup: interrupt vectors, RAM set-up, a vertical-blank
;; handler that counts frames and a timer handler that keeps the sounds'
;; time. makebin fills in the cartridge header.
;; Everything here is in the first 16 KiB of the ROM, which is always
;; mapped; the rest of the program is in banks 1 to 3 (see far.c).
	.module crt0
	.globl	_main
	.globl	_flush_tiles
	.globl	_frame_count, _pad_last, _pad_latch
	.globl	_sound_active, _sound_tick
	.globl	_lcd_scx, _lcd_cut
	.globl	_staged, _staged_at, _staged_count, _staged_from
	.globl	_lcd_column_fill, _lcd_column_blit
	.globl	_lcd_column_rows, _lcd_column_color, _lcd_column_bits
	.globl	s__INITIALIZER, s__INITIALIZED, l__INITIALIZER
	.globl	_gb_cgb

	.area	_HEADER (ABS)
	.org	0x40		; vertical blank
	jp	vblank
	.org	0x48		; LCD status
	jp	lcd_split
	.org	0x50		; timer
	jp	timer
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
	ld	e, a			; 0x11 from a Game Boy Color's boot ROM


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
	ld	a, e
	ld	(#_gb_cgb), a

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

	.area	_DATA
;; The scroll at the top of the screen, and the two cuts: line, LCD control,
;; scroll.
_lcd_scx::
	.ds	1
_lcd_cut::
	.ds	6

	;; The handlers and helpers go with the rest of the code: the header
	;; area above is at fixed addresses and ends where the code begins.
	.area	_HOME
vblank:
	push	af
	push	hl
	ld	hl, #_frame_count
	inc	(hl)
	;; The top of the screen takes its tiles from 0x8000 and is scrolled
	;; by lcd_scx; the first cut comes the line before lcd_cut's first.
	ld	a, #0x91
	ld	(#0xff40), a
	ld	a, (#_lcd_scx)
	ld	(#0xff43), a
	ld	a, (#_lcd_cut)
	dec	a
	ld	(#0xff45), a

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

	pop	hl
	pop	af
	reti

;; The timer interrupts once per unit of the phone's timers. While a sound
;; plays, the core moves it on a unit. That is C and takes a while, so
;; other interrupts are let in first: a cut across the screen cannot wait.
timer:
	push	af
	ld	a, (#_sound_active)
	or	a, a
	jr	z, 1$
	ei
	push	bc
	push	de
	push	hl
	call	_sound_tick
	pop	hl
	pop	de
	pop	bc
1$:
	pop	af
	reti

;; The screen is cut across in up to two places, where the tile area the
;; LCD takes its tiles from and the horizontal scroll change: lcd_cut holds
;; for each cut the last line before it and the values the LCD control and
;; scroll registers take after it; a line of 0xff is no cut. The menus cut
;; once, where the screen's 360 tiles continue at 0x9000; the game at 2x
;; cuts above and below the strip of terrain it scrolls (see main.c).
;;
;; The interrupt comes a line early and waits for the cut's own line and
;; then its horizontal blank, so that the change never lands in a drawn
;; line, nor a line late when the interrupt was held off for a moment.
lcd_split:
	push	af
	push	bc
	push	hl
	ld	a, (#0xff45)
	inc	a
	ld	b, a			; the line this interrupt is for
	ld	hl, #_lcd_cut
	ld	c, #3			; the other cut is three bytes on
	cp	a, (hl)
	jr	z, 2$
	inc	hl
	inc	hl
	inc	hl
	ld	c, #0xfd		; or three back
	cp	a, (hl)
	jr	z, 2$
	;; The cuts were changed since this interrupt was asked for: it is
	;; for neither. Ask for the first.
	ld	a, (#_lcd_cut)
	jr	5$
2$:
	ld	a, (#0xff44)
	cp	a, b
	jr	c, 2$
3$:
	ld	a, (#0xff41)
	and	a, #0x03
	jr	nz, 3$
	inc	hl
	ld	a, (hl+)
	ld	(#0xff40), a
	ld	a, (hl)
	ld	(#0xff43), a
	;; The next interrupt is for the other cut.
	dec	hl
	dec	hl
	ld	a, l
	add	a, c
	ld	l, a
	ld	a, h
	adc	a, #0
	bit	7, c
	jr	z, 4$
	dec	a
4$:
	ld	h, a
	ld	a, (hl)
5$:
	dec	a
	ld	(#0xff45), a
	pop	hl
	pop	bc
	pop	af
	reti

;; void flush_tiles(void): copies staged_count 16-byte tiles from staged to
;; the video RAM addresses in staged_at, with the LCD on. Video RAM accepts
;; writes only outside the part of each line where the LCD controller is
;; drawing, so each four bytes wait for a horizontal or vertical blank. The
;; check takes 4 cycles after it reads the status and the writes 12, so even
;; when the blank ends right after the read they land inside the 20 cycles
;; of the following line's OAM scan, which still accepts them. Interrupts
;; are held off between the check and the writes.
_flush_tiles::
	ld	a, (#_staged_count)
	or	a, a
	ret	z
	ld	hl, #_staged
	ld	a, l
	ld	(#_staged_from), a
	ld	a, h
	ld	(#_staged_from + 1), a
	ld	hl, #_staged_at
	ld	a, (#_staged_count)
1$:
	push	af			; tiles left
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	push	hl			; the next address in staged_at
	ld	h, a
	ld	l, c			; hl = where this tile goes
2$:
	push	hl
	ld	a, (#_staged_from)
	ld	l, a
	ld	a, (#_staged_from + 1)
	ld	h, a			; hl = the next four bytes of data
	ld	a, (hl+)
	ld	b, a
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	ld	d, a
	ld	a, (hl+)
	ld	e, a
	ld	a, l
	ld	(#_staged_from), a
	ld	a, h
	ld	(#_staged_from + 1), a
	pop	hl
	di
3$:
	ld	a, (#0xff41)
	and	a, #0x02
	jr	nz, 3$			; drawing, or about to: wait
	ld	(hl), b
	inc	l
	ld	(hl), c
	inc	l
	ld	(hl), d
	inc	l
	ld	(hl), e
	inc	l			; tiles start on 16-byte boundaries
	ei
	ld	a, l
	and	a, #0x0f
	jr	nz, 2$
	pop	hl
	pop	af
	dec	a
	jr	nz, 1$
	ret

;; The core's two innermost drawing loops (see core/lcd.h), which the
;; compiler makes many times slower than this. A framebuffer row is 20
;; bytes. Both take the first byte's address in de and the mask in a.

;; void lcd_column_fill(uint8_t *p, uint8_t mask)
_lcd_column_fill::
	ld	c, a
	ld	h, d
	ld	l, e
	ld	de, #20
	ld	a, (#_lcd_column_rows)
	ld	b, a
	ld	a, (#_lcd_column_color)
	or	a, a
	jr	z, 3$
	dec	a
	jr	z, 2$
1$:					; invert
	ld	a, (hl)
	xor	a, c
	ld	(hl), a
	add	hl, de
	dec	b
	jr	nz, 1$
	ret
2$:					; set
	ld	a, (hl)
	or	a, c
	ld	(hl), a
	add	hl, de
	dec	b
	jr	nz, 2$
	ret
3$:					; clear
	ld	a, c
	cpl
	ld	c, a
4$:
	ld	a, (hl)
	and	a, c
	ld	(hl), a
	add	hl, de
	dec	b
	jr	nz, 4$
	ret

;; void lcd_column_blit(uint8_t *p, uint8_t mask)
_lcd_column_blit::
	ld	c, a
	ld	h, d
	ld	l, e
	ld	a, (#_lcd_column_rows)
	ld	b, a
	ld	a, (#_lcd_column_bits)
	ld	d, a
1$:
	ld	a, (hl)
	or	a, c			; set the pixel
	srl	d
	jr	c, 2$
	xor	a, c			; or clear it
2$:
	ld	(hl), a
	ld	a, l
	add	a, #20
	ld	l, a
	jr	nc, 3$
	inc	h
3$:
	dec	b
	jr	nz, 1$
	ret
