;; The Game Boy's own versions of the things Space Impact does every tick
;; that the compiler makes far too slow: finding what a sprite has run into
;; (see core/si.c), putting a band of a sprite into the picture (see
;; core/sprite.c), and bringing the screen's tiles up to date with the
;; picture. All of it is in the ROM's first bank.
	.module draw
	.globl	_sprite_screen
	.globl	_sprite_band, _sprite_band_dst, _sprite_band_src, _sprite_draw_bitmap
	.globl	_sprite_band_n, _sprite_band_up, _sprite_band_valid, _sprite_band_mode
	.globl	_gb_present_zoom, _gb_present_plain, _gb_present_all, _gb_clear_tiles
	.globl	_gb_zoom_band, _gb_zoom_bands, _gb_columns, _gb_tile_low, _gb_tile_high, _gb_vram_put
	.globl	_sprites, _si_player_side_types
	.globl	_strip_scan, _strip_cell, _strip_own, _strip_place, _strip_fine
	.globl	_strip_invert, _strip_picture, _strip_terrain
	.globl	_si_find_hit_from, _si_find_types
	.globl	_si_find_left, _si_find_right, _si_find_top, _si_find_bottom

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

;; What the tiles on the screen were made from: a copy of sprite_screen.
shown:
	.ds	504
_gb_present_all::
	.ds	1
;; The first of the bands of the picture gb_present_zoom shows, and how
;; many: four beside Space Impact's strip of terrain, all six for a game
;; without one.
_gb_zoom_band::
	.ds	1
_gb_zoom_bands::
	.ds	1
band:
	.ds	1
bands_left:
	.ds	1
differed:
	.ds	1
tile_top:
	.ds	2
tile_bottom:
	.ds	2
_gb_columns::
columns:
	.ds	8
rows:
	.ds	8

_si_find_types::
	.ds	2
_si_find_left::
	.ds	2
_si_find_right::
	.ds	2
_si_find_top::
	.ds	2
_si_find_bottom::
	.ds	2
find_id:
	.ds	1
bitmap_width:
	.ds	1
bitmap_rows:
	.ds	1
bitmap_shift:
	.ds	1
bitmap_band:
	.ds	1
bitmap_valid:
	.ds	1
bitmap_dst:
	.ds	2
scan_band:
	.ds	1
scan_across:
	.ds	1

	.area	_HOME

;; uint16_t si_find_hit_from(uint16_t first): Space Impact's search for what
;; a sprite has run into (find_hit in core/si.c), which the game makes for
;; the ship and for every shot on every tick. Walks the sprite list from
;; `first` and returns the first sprite that is not one of the player's own
;; and whose box touches the box in si_find_left, _right, _top and _bottom,
;; or 0. A sprite is 16 bytes with its next sprite at 0, x at 3, y at 4,
;; width at 9 and height at 10; si_find_types is the address of sprite 0's
;; object type, the types being 16 bytes apart too. The other sprite's
;; place and size are signed bytes, as in the firmware, and the sums are 16
;; bits wide.
_si_find_hit_from::
	ld	a, e
find_loop:
	or	a, a
	jr	nz, 1$
	ld	bc, #0
	ret
1$:
	ld	(#find_id), a
	ld	l, a
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	push	hl
	ld	a, (#_si_find_types)
	ld	e, a
	ld	a, (#_si_find_types + 1)
	ld	d, a
	add	hl, de
	ld	a, (hl)			; its type
	pop	hl
	ld	de, #_sprites
	add	hl, de			; hl = the sprite
	cp	a, #0x25
	jr	nc, 2$			; no type above 0x24 is the player's
	push	hl
	add	a, #<_si_player_side_types
	ld	l, a
	ld	a, #0
	adc	a, #>_si_player_side_types
	ld	h, a
	ld	a, (hl)
	pop	hl
	or	a, a
	jr	nz, find_next
2$:
	push	hl
	inc	hl
	inc	hl
	inc	hl
	ld	a, (hl+)		; x
	ld	e, a
	rla
	sbc	a, a
	ld	d, a			; de = x
	ld	a, (#_si_find_right)	; right of the box before x: no
	sub	a, e
	ld	a, (#_si_find_right + 1)
	sbc	a, d
	rla
	jp	c, find_skip
	ld	b, (hl)			; y
	ld	a, l
	add	a, #5
	ld	l, a
	jr	nc, 3$
	inc	h
3$:
	ld	a, (hl+)		; width
	ld	c, (hl)			; height
	ld	l, a
	rla
	sbc	a, a
	ld	h, a
	add	hl, de			; hl = x + width
	ld	a, (#_si_find_left)	; before the left of the box: no
	ld	e, a
	ld	a, l
	sub	a, e
	ld	a, (#_si_find_left + 1)
	ld	e, a
	ld	a, h
	sbc	a, e
	rla
	jp	c, find_skip
	ld	a, b
	ld	e, a
	rla
	sbc	a, a
	ld	d, a			; de = y
	ld	a, (#_si_find_bottom)
	sub	a, e
	ld	a, (#_si_find_bottom + 1)
	sbc	a, d
	rla
	jr	c, find_skip
	ld	a, c
	ld	l, a
	rla
	sbc	a, a
	ld	h, a
	add	hl, de			; hl = y + height
	ld	a, (#_si_find_top)
	ld	e, a
	ld	a, l
	sub	a, e
	ld	a, (#_si_find_top + 1)
	ld	e, a
	ld	a, h
	sbc	a, e
	rla
	jr	c, find_skip
	pop	hl
	ld	a, (#find_id)
	ld	c, a
	ld	b, #0
	ret
find_skip:
	pop	hl
find_next:
	ld	a, (hl)
	jp	find_loop

;; void sprite_draw_bitmap(const struct sprite *s): draw_bitmap of
;; core/sprite.c, with the mode already in sprite_band_mode. A sprite has
;; its x at 3, y at 4, the address of its bitmap at 7, its width at 9 and
;; height at 10. Each band of 8 rows of the bitmap goes onto one band of
;; the picture, or across two when the sprite's row is not a multiple of 8.
_sprite_draw_bitmap::
	ld	h, d
	ld	l, e
	inc	hl
	inc	hl
	inc	hl
	ld	a, (hl+)
	ld	b, a			; x
	ld	a, (hl+)
	ld	c, a			; y
	inc	hl
	inc	hl
	ld	a, (hl+)
	ld	(#_sprite_band_src), a
	ld	a, (hl+)
	ld	(#_sprite_band_src + 1), a
	ld	a, (hl+)
	ld	d, a			; width
	ld	e, (hl)			; height
	or	a, a
	ret	z
	ld	a, e
	or	a, a
	ret	z
	;; A sprite whose far edge has wrapped past 255 is not drawn at all,
	;; nor one that starts off the picture.
	ld	a, b
	add	a, d
	dec	a
	cp	a, b
	ret	c
	ld	a, c
	add	a, e
	dec	a
	cp	a, c
	ret	c
	ld	a, b
	cp	a, #84
	ret	nc
	ld	a, c
	cp	a, #48
	ret	nc
	ld	a, #84			; the columns that fit
	sub	a, b
	cp	a, d
	jr	c, 1$
	ld	a, d
1$:
	ld	(#_sprite_band_n), a
	ld	a, d
	ld	(#bitmap_width), a
	ld	a, e
	ld	(#bitmap_rows), a
	ld	a, c
	and	a, #7
	ld	(#bitmap_shift), a
	ld	a, c
	rrca
	rrca
	rrca
	and	a, #0x1f
	ld	(#bitmap_band), a
	add	a, a
	ld	e, a
	ld	d, #0
	ld	hl, #band_starts
	add	hl, de
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	e, b
	add	hl, de			; the band's start and x
	ld	a, l
	ld	(#bitmap_dst), a
	ld	a, h
	ld	(#bitmap_dst + 1), a
bitmap_next_band:
	ld	a, (#bitmap_band)
	cp	a, #6
	ret	nc
	ld	a, (#bitmap_rows)
	or	a, a
	ret	z
	ld	b, a
	cp	a, #8
	jr	c, 1$
	ld	b, #8
1$:
	sub	a, b
	ld	(#bitmap_rows), a
	ld	a, b			; the rows of this band that are the sprite's
	add	a, #<low_bits
	ld	l, a
	ld	a, #0
	adc	a, #>low_bits
	ld	h, a
	ld	c, (hl)
	ld	a, (#bitmap_dst)
	ld	(#_sprite_band_dst), a
	ld	a, (#bitmap_dst + 1)
	ld	(#_sprite_band_dst + 1), a
	ld	a, (#bitmap_shift)
	or	a, a
	jr	nz, 2$
	ld	(#_sprite_band_up), a
	ld	a, c
	ld	(#_sprite_band_valid), a
	call	_sprite_band
	jr	bitmap_advance
2$:
	ld	(#_sprite_band_up), a
	ld	b, a
	ld	a, c
	ld	(#bitmap_valid), a
3$:
	add	a, a
	dec	b
	jr	nz, 3$
	ld	(#_sprite_band_valid), a
	call	_sprite_band
	;; The rows that spill into the next band.
	ld	a, (#bitmap_shift)
	ld	b, a
	ld	a, #8
	sub	a, b
	ld	b, a
	ld	a, (#bitmap_valid)
4$:
	srl	a
	dec	b
	jr	nz, 4$
	or	a, a
	jr	z, bitmap_advance
	ld	(#_sprite_band_valid), a
	ld	a, (#bitmap_band)
	cp	a, #5
	jr	nc, bitmap_advance
	ld	a, (#bitmap_dst)
	add	a, #84
	ld	(#_sprite_band_dst), a
	ld	a, (#bitmap_dst + 1)
	adc	a, #0
	ld	(#_sprite_band_dst + 1), a
	ld	a, (#bitmap_shift)
	sub	a, #8
	ld	(#_sprite_band_up), a
	call	_sprite_band
bitmap_advance:
	ld	hl, #bitmap_band
	inc	(hl)
	ld	a, (#bitmap_dst)
	add	a, #84
	ld	(#bitmap_dst), a
	jr	nc, 1$
	ld	hl, #bitmap_dst + 1
	inc	(hl)
1$:
	ld	a, (#bitmap_width)
	ld	b, a
	ld	a, (#_sprite_band_src)
	add	a, b
	ld	(#_sprite_band_src), a
	jp	nc, bitmap_next_band
	ld	hl, #_sprite_band_src + 1
	inc	(hl)
	jp	bitmap_next_band

;; Where each band of the picture starts.
band_starts:
	.dw	_sprite_screen, _sprite_screen + 84, _sprite_screen + 168
	.dw	_sprite_screen + 252, _sprite_screen + 336, _sprite_screen + 420
;; A byte with its lowest n bits set, for n from 0 to 8.
low_bits:
	.db	0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff

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

;; The picture on the screen. The game's tiles hold their pixels in the
;; first bit plane only (the second is cleared when the game comes on, and
;; the palette shows both set colours dark), so a tile is eight bytes two
;; apart. Video RAM accepts writes only outside the part of each line where
;; the LCD controller is drawing; four_rows waits for that and then writes
;; four bytes, which fit before the next line's drawing starts even when
;; the wait ends at the last moment (see flush_tiles in crt0.s).

;; Writes b, c, d, e to (hl), (hl + 2), (hl + 4), (hl + 6) and leaves hl
;; eight on.
four_rows:
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

;; Where each row of the screen's tiles starts in video RAM: twelve rows
;; from 0x8000, six from 0x9000 (see platform/gb/main.c).
tile_rows:
	.dw	0x8000, 0x8140, 0x8280, 0x83c0, 0x8500, 0x8640
	.dw	0x8780, 0x88c0, 0x8a00, 0x8b40, 0x8c80, 0x8dc0
	.dw	0x9000, 0x9140, 0x9280, 0x93c0, 0x9500, 0x9640

;; Makes the copy of what is shown differ from the picture in every bit, so
;; that the next pass makes every tile.
forget_shown:
	ld	hl, #_sprite_screen
	ld	de, #shown
	ld	bc, #504
1$:
	ld	a, (hl+)
	cpl
	ld	(de), a
	inc	de
	dec	bc
	ld	a, b
	or	a, c
	jr	nz, 1$
	xor	a, a
	ld	(#_gb_present_all), a
	ret

;; hl = the tile b tiles from the end of a row of 20 that starts at the
;; address in the variable at de.
tile_address:
	ld	a, #20
	sub	a, b
	ld	l, a
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	a, (de)
	inc	de
	add	a, l
	ld	l, a
	ld	a, (de)
	adc	a, h
	ld	h, a
	ret

;; The game at 2x, as the full-screen variant shows it: a tile is four
;; columns by four rows of the picture, so each group of four bytes of a
;; band is two tiles, one for each half byte, and the picture's first 80
;; columns fill the screen's 20 tiles. This does gb_zoom_bands bands of the
;; picture from gb_zoom_band, two rows of tiles each at 0x8000; for Space
;; Impact that is four, and the other two bands hold the terrain, which
;; strip.c puts on the screen. Only the groups that
;; differ from what is shown are made again.
;; void gb_present_zoom(void)
_gb_present_zoom::
	ld	a, (#_gb_present_all)
	or	a, a
	call	nz, forget_shown
	xor	a, a
	ld	(#band), a
	ld	a, (#_gb_zoom_bands)
	ld	(#bands_left), a
	;; 84 bytes on in the picture and in the copy for each band skipped.
	ld	hl, #_sprite_screen
	ld	de, #shown
	ld	a, (#_gb_zoom_band)
	or	a, a
	jr	z, zoom_band
	ld	b, a
	push	de
	ld	de, #84
2$:
	add	hl, de
	dec	b
	jr	nz, 2$
	pop	de
	push	hl
	ld	a, (#_gb_zoom_band)
	ld	b, a
	ld	h, d
	ld	l, e
	ld	de, #84
3$:
	add	hl, de
	dec	b
	jr	nz, 3$
	ld	d, h
	ld	e, l
	pop	hl
zoom_band:
	push	hl
	push	de
	;; This band's two rows of tiles: rows 2 * band and the next.
	ld	a, (#band)
	add	a, a
	add	a, a
	ld	e, a
	ld	d, #0
	ld	hl, #tile_rows
	add	hl, de
	ld	de, #tile_top
	ld	b, #4
1$:
	ld	a, (hl+)
	ld	(de), a
	inc	de
	dec	b
	jr	nz, 1$
	pop	de
	pop	hl
	ld	b, #20			; groups left in the band
zoom_scan:
	ld	a, (de)
	cp	a, (hl)
	jr	nz, zoom_hit0
	inc	hl
	inc	de
	ld	a, (de)
	cp	a, (hl)
	jr	nz, zoom_hit1
	inc	hl
	inc	de
	ld	a, (de)
	cp	a, (hl)
	jr	nz, zoom_hit2
	inc	hl
	inc	de
	ld	a, (de)
	cp	a, (hl)
	jr	nz, zoom_hit3
	inc	hl
	inc	de
zoom_scanned:
	dec	b
	jr	nz, zoom_scan
	;; The band's last four columns are not shown.
	inc	hl
	inc	hl
	inc	hl
	inc	hl
	inc	de
	inc	de
	inc	de
	inc	de
	ld	a, (#band)
	inc	a
	ld	(#band), a
	ld	a, (#bands_left)
	dec	a
	ld	(#bands_left), a
	jr	nz, zoom_band
	ret

zoom_hit3:
	dec	hl
	dec	de
zoom_hit2:
	dec	hl
	dec	de
zoom_hit1:
	dec	hl
	dec	de
zoom_hit0:
	ld	c, #0
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 0), a
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 1), a
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 2), a
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 3), a
	push	hl
	push	de
	push	bc
	ld	a, c
	ld	(#differed), a
	and	a, #0x0f
	jr	z, 1$
	;; The upper tile, from the low half of each byte.
	ld	de, #tile_top
	call	tile_address
	push	hl
	ld	hl, #columns
	ld	a, (hl+)
	ld	b, a
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	ld	d, a
	ld	e, (hl)
	call	zoom_rows
	pop	hl
	call	put_zoom_tile
	pop	bc
	push	bc
1$:
	ld	a, (#differed)
	and	a, #0xf0
	jr	z, 2$
	;; The lower tile, from the high half.
	ld	de, #tile_bottom
	call	tile_address
	push	hl
	ld	hl, #columns
	ld	a, (hl+)
	swap	a
	ld	b, a
	ld	a, (hl+)
	swap	a
	ld	c, a
	ld	a, (hl+)
	swap	a
	ld	d, a
	ld	a, (hl)
	swap	a
	ld	e, a
	call	zoom_rows
	pop	hl
	call	put_zoom_tile
2$:
	pop	bc
	pop	de
	pop	hl
	jp	zoom_scanned

;; Four rows of four pixels, from the low halves of b, c, d and e (the
;; columns, left to right), each pixel doubled across, into `rows`. A
;; column's bit goes in at the left and is then doubled by a shift that
;; keeps the leftmost bit.
zoom_rows:
	ld	hl, #rows
	srl	e
	rra
	sra	a
	srl	d
	rra
	sra	a
	srl	c
	rra
	sra	a
	srl	b
	rra
	sra	a
	ld	(hl+), a
	srl	e
	rra
	sra	a
	srl	d
	rra
	sra	a
	srl	c
	rra
	sra	a
	srl	b
	rra
	sra	a
	ld	(hl+), a
	srl	e
	rra
	sra	a
	srl	d
	rra
	sra	a
	srl	c
	rra
	sra	a
	srl	b
	rra
	sra	a
	ld	(hl+), a
	srl	e
	rra
	sra	a
	srl	d
	rra
	sra	a
	srl	c
	rra
	sra	a
	srl	b
	rra
	sra	a
	ld	(hl+), a
	ret

;; void strip_scan(uint8_t band): goes over the 21 cells of a band of the
;; scrolled strip of terrain (see strip.c) and calls strip_cell(across,
;; band) for each one that needs looking at. One that does not is plain
;; terrain shown as such: its four bytes of the picture equal the terrain
;; bitmap's (inverted on a level drawn light on dark) and neither of its
;; two tiles is one of its own. The cells whose terrain the bitmap cannot
;; vouch for, the first of each terrain tile and one that starts left of
;; the picture, always go to strip_cell.
_strip_scan::
	ld	(#scan_band), a
	xor	a, a
	ld	(#scan_across), a
scan_cell:
	ld	b, a			; across
	ld	a, (#_strip_place)
	add	a, b
	and	a, #7
	jr	z, scan_slow
	ld	a, b
	add	a, a
	add	a, a
	ld	hl, #_strip_fine
	sub	a, (hl)
	jr	c, scan_slow		; starts left of the picture
	ld	c, a
	ld	b, #0			; bc = the cell's first column
	ld	a, (#_strip_terrain)
	ld	l, a
	ld	a, (#_strip_terrain + 1)
	ld	h, a
	add	hl, bc
	ld	d, h
	ld	e, l
	ld	a, (#_strip_picture)
	ld	l, a
	ld	a, (#_strip_picture + 1)
	ld	h, a
	add	hl, bc
	ld	a, (#_strip_invert)
	ld	c, a
	ld	a, (de)
	xor	a, c
	cp	a, (hl)
	jr	nz, scan_slow
	inc	hl
	inc	de
	ld	a, (de)
	xor	a, c
	cp	a, (hl)
	jr	nz, scan_slow
	inc	hl
	inc	de
	ld	a, (de)
	xor	a, c
	cp	a, (hl)
	jr	nz, scan_slow
	inc	hl
	inc	de
	ld	a, (de)
	xor	a, c
	cp	a, (hl)
	jr	nz, scan_slow
	;; Plain terrain. Is either tile still the cell's own?
	ld	a, (#scan_across)
	add	a, a
	add	a, a
	ld	c, a
	ld	a, (#scan_band)
	add	a, a
	add	a, c
	ld	c, a
	ld	b, #0
	ld	hl, #_strip_own
	add	hl, bc
	ld	a, (hl+)
	or	a, (hl)
	jr	z, scan_next
scan_slow:
	ld	a, (#scan_band)
	ld	e, a
	ld	a, (#scan_across)
	call	_strip_cell
scan_next:
	ld	hl, #scan_across
	inc	(hl)
	ld	a, (hl)
	cp	a, #21
	jr	nz, scan_cell
	ret

;; void gb_tile_low(uint8_t *tile), gb_tile_high(uint8_t *tile): makes the
;; tile at that address in video RAM from the low or the high halves of the
;; four bytes in gb_columns, as the 2x picture's tiles are made.
_gb_tile_low::
	push	de
	ld	hl, #columns
	ld	a, (hl+)
	ld	b, a
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	ld	d, a
	ld	e, (hl)
	jr	tile_from_columns
_gb_tile_high::
	push	de
	ld	hl, #columns
	ld	a, (hl+)
	swap	a
	ld	b, a
	ld	a, (hl+)
	swap	a
	ld	c, a
	ld	a, (hl+)
	swap	a
	ld	d, a
	ld	a, (hl)
	swap	a
	ld	e, a
tile_from_columns:
	call	zoom_rows
	pop	hl
	jp	put_zoom_tile

;; void gb_vram_put(uint8_t *address, uint8_t value): one byte to video
;; RAM, between the lines being drawn.
_gb_vram_put::
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

;; Writes the tile at hl from the four rows in `rows`, each one twice.
put_zoom_tile:
	ld	a, (#rows)
	ld	b, a
	ld	c, a
	ld	a, (#rows + 1)
	ld	d, a
	ld	e, a
	call	four_rows
	ld	a, (#rows + 2)
	ld	b, a
	ld	c, a
	ld	a, (#rows + 3)
	ld	d, a
	ld	e, a
	jp	four_rows

;; The game as it is, in the phone-sized mode: the picture's 84 columns
;; across eleven tiles from the sixth, its 48 rows down six tile rows from
;; the seventh. A tile is eight bytes of a band turned on their side; the
;; eleventh has only four. The picture then starts at pixel 40, and the
;; layer scrolls the screen two pixels to put it in the middle, at 38.
;; void gb_present_plain(void)
_gb_present_plain::
	ld	a, (#_gb_present_all)
	or	a, a
	call	nz, forget_shown
	xor	a, a
	ld	(#band), a
	ld	hl, #_sprite_screen
	ld	de, #shown
plain_band_loop:
	push	hl
	push	de
	;; The band's row of tiles, row 6 + band, from nine tiles before its
	;; sixth: tile_address counts a row of 20 from its end, and the
	;; eleven tiles here end at the sixteenth.
	ld	a, (#band)
	add	a, #6
	add	a, a
	ld	e, a
	ld	d, #0
	ld	hl, #tile_rows
	add	hl, de
	ld	a, (hl+)
	sub	a, #4 * 16
	ld	(#tile_top), a
	ld	a, (hl)
	sbc	a, #0
	ld	(#tile_top + 1), a
	pop	de
	pop	hl
	ld	b, #11			; tiles left in the band
plain_scan:
	ld	a, b
	dec	a
	jr	z, plain_last
	ld	a, (de)
	cp	a, (hl)
	jp	nz, plain_hit0
	inc	hl
	inc	de
	ld	a, (de)
	cp	a, (hl)
	jp	nz, plain_hit1
	inc	hl
	inc	de
	ld	a, (de)
	cp	a, (hl)
	jp	nz, plain_hit2
	inc	hl
	inc	de
	ld	a, (de)
	cp	a, (hl)
	jp	nz, plain_hit3
	inc	hl
	inc	de
	ld	a, (de)
	cp	a, (hl)
	jp	nz, plain_hit4
	inc	hl
	inc	de
	ld	a, (de)
	cp	a, (hl)
	jp	nz, plain_hit5
	inc	hl
	inc	de
	ld	a, (de)
	cp	a, (hl)
	jp	nz, plain_hit6
	inc	hl
	inc	de
	ld	a, (de)
	cp	a, (hl)
	jp	nz, plain_hit7
	inc	hl
	inc	de
plain_scanned:
	dec	b
	jr	nz, plain_scan
	ld	a, (#band)
	inc	a
	ld	(#band), a
	cp	a, #6
	jp	nz, plain_band_loop
	ret

;; The eleventh tile: four columns, the rest of it empty.
plain_last:
	ld	a, (de)
	cp	a, (hl)
	jr	nz, last_hit0
	inc	hl
	inc	de
	ld	a, (de)
	cp	a, (hl)
	jr	nz, last_hit1
	inc	hl
	inc	de
	ld	a, (de)
	cp	a, (hl)
	jr	nz, last_hit2
	inc	hl
	inc	de
	ld	a, (de)
	cp	a, (hl)
	jr	nz, last_hit3
	inc	hl
	inc	de
	jr	plain_scanned
last_hit3:
	dec	hl
	dec	de
last_hit2:
	dec	hl
	dec	de
last_hit1:
	dec	hl
	dec	de
last_hit0:
	xor	a, a
	ld	(#columns + 4), a
	ld	(#columns + 5), a
	ld	(#columns + 6), a
	ld	(#columns + 7), a
	ld	c, #0
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 0), a
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 1), a
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 2), a
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 3), a
	jr	plain_tile

plain_hit7:
	dec	hl
	dec	de
plain_hit6:
	dec	hl
	dec	de
plain_hit5:
	dec	hl
	dec	de
plain_hit4:
	dec	hl
	dec	de
plain_hit3:
	dec	hl
	dec	de
plain_hit2:
	dec	hl
	dec	de
plain_hit1:
	dec	hl
	dec	de
plain_hit0:
	ld	c, #0
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 0), a
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 1), a
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 2), a
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 3), a
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 4), a
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 5), a
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 6), a
	ld	a, (de)
	xor	a, (hl)
	or	a, c
	ld	c, a
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	(#columns + 7), a
plain_tile:
	push	hl
	push	de
	push	bc
	;; Turn the eight columns into eight rows.
	ld	de, #rows
	ld	c, #8
1$:
	ld	hl, #columns
	xor	a, a
	srl	(hl)
	rla
	inc	hl
	srl	(hl)
	rla
	inc	hl
	srl	(hl)
	rla
	inc	hl
	srl	(hl)
	rla
	inc	hl
	srl	(hl)
	rla
	inc	hl
	srl	(hl)
	rla
	inc	hl
	srl	(hl)
	rla
	inc	hl
	srl	(hl)
	rla
	inc	hl
	ld	(de), a
	inc	de
	dec	c
	jr	nz, 1$
	ld	de, #tile_top
	call	tile_address
	push	hl
	ld	hl, #rows
	ld	a, (hl+)
	ld	b, a
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	ld	d, a
	ld	e, (hl)
	pop	hl
	call	four_rows
	push	hl
	ld	hl, #rows + 4
	ld	a, (hl+)
	ld	b, a
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	ld	d, a
	ld	e, (hl)
	pop	hl
	call	four_rows
	pop	bc
	pop	de
	pop	hl
	jp	plain_scanned

;; void gb_clear_tiles(void): clears both bit planes of every tile of the
;; screen, with the LCD on, four bytes at a time between the lines being
;; drawn. The game's tiles are then written in the first plane only.
_gb_clear_tiles::
	ld	hl, #0x8000
	ld	d, #0x8f		; 240 tiles
	call	clear_to
	ld	hl, #0x9000
	ld	d, #0x97
	ld	e, #0x80		; 120 tiles
	jr	clear_until
clear_to:
	ld	e, #0x00
clear_until:
	di
1$:
	ld	a, (#0xff41)
	and	a, #0x02
	jr	nz, 1$
	xor	a, a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ei
	ld	a, h
	cp	a, d
	jr	nz, clear_until
	ld	a, l
	cp	a, e
	jr	nz, clear_until
	ret
