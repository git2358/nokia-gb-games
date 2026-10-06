;; The Game Boy's own versions of the two things screen.c does that the
;; compiler makes far too slow: finding the 8x8 blocks of the phone's
;; picture that changed, and turning one, eight column bytes (bit y of byte
;; x the pixel (x, y)), into eight row bytes of lcd_fb (bit 7 - x of byte
;; y), 20 bytes apart. In the ROM's first bank.
	.module blocks
	.globl	_gb_block_rows, _gb_block_changed, _gb_rows_put, _gb_rows, _gb_fill
	.globl	_gb_rows_present, _gb_rows_fb, _gb_rows_tiles, _si_rows_ram, _gb_any_dirty, _lcd_dirty
	.globl	_gb_rows_made, _gb_rows_count, _si_pictures, _si_rows_bank, _si_rows_at, _si_rows_a, _si_rows_b
	.globl	_far_mapped, _gb_shifted_left, _gb_terrain_shift
	.globl	_gb_palette, _gb_cgb

	.area	_DATA
;; What the boot ROM left in A (crt0.s keeps it): 0x11 on a Game Boy Color
;; or Advance, where the ROM runs in Color mode (main.c).
_gb_cgb::
	.ds	1
;; core/si_rows.c's struct si_rows_put: src, dst, stride, n, h, y, first,
;; last, op.
_gb_rows::
	.ds	11
RP_SRC = _gb_rows
RP_DST = _gb_rows + 2
RP_STRIDE = _gb_rows + 4
RP_N = _gb_rows + 5
RP_H = _gb_rows + 6
RP_Y = _gb_rows + 7
RP_FIRST = _gb_rows + 8
RP_LAST = _gb_rows + 9
RP_OP = _gb_rows + 10
;; rows_put_wide's: 8 * n, and the stride less n.
rp_back:
	.ds	1
rp_skip:
	.ds	1
;; gb_rows_put's own: the column's source and screen, columns left, its
;; mask, rows left and row in its tile.
cp_src:
	.ds	2
cp_dst:
	.ds	2
cp_left:
	.ds	1
cp_mask:
	.ds	1
cp_rows:
	.ds	1
cp_y:
	.ds	1
;; gb_rows_made's: the picture's place, size, op and so on.
rm_x:
	.ds	2
rm_y:
	.ds	2
rm_w:
	.ds	1
rm_h:
	.ds	1
rm_op:
	.ds	1
rm_s:
	.ds	1
rm_n:
	.ds	1
rm_x8:
	.ds	1
rm_skip:
	.ds	1
rm_count:
	.ds	1
rm_r0:
	.ds	1
rm_hh:
	.ds	1
rm_was:
	.ds	1
ts_h:
	.ds	1
;; gb_rows_present's place: the row of tiles, the tiles left in it, and the
;; next tile in si_rows_ram's screen and shown, in lcd_fb and video RAM.
pr_ty:
	.ds	1
pr_left:
	.ds	1
pr_now:
	.ds	2
pr_was:
	.ds	2
pr_fb:
	.ds	2
pr_tile:
	.ds	2
pr_tx:
	.ds	1

	.area	_HOME
;; void gb_rows_put(void): core/si_rows.c's put_rows_loop. h rows of n
;; bytes, each byte of a row 8 on from the last in the screen (the next
;; tile), the first byte of a row masked with `first`, the last with
;; `last`; op 1 copies under the mask, 2 ors, 3 xors. A picture one or two
;; bytes across is done a byte column at a time (wider ones a row at a
;; time, rows_put_wide): down a column, the screen's bytes are one apart within a
;; tile and 128 - 8 apart from a tile to the one below (si_rows' rows of
;; tiles are 128 bytes), and the source's a stride apart.
_gb_rows_put::
	ld	a, (#RP_N)
	cp	a, #3
	jp	nc, rows_put_wide
	ld	a, (#RP_SRC)
	ld	(#cp_src), a
	ld	a, (#RP_SRC + 1)
	ld	(#cp_src + 1), a
	ld	a, (#RP_DST)
	ld	(#cp_dst), a
	ld	a, (#RP_DST + 1)
	ld	(#cp_dst + 1), a
	ld	a, (#RP_N)
	ld	(#cp_left), a
	ld	a, (#RP_FIRST)
	ld	(#cp_mask), a
cp_column:
	ld	a, (#cp_left)
	dec	a
	jr	nz, 1$
	ld	a, (#RP_LAST)		; the last column: its mask too
	ld	b, a
	ld	a, (#cp_mask)
	and	a, b
	ld	(#cp_mask), a
1$:
	ld	a, (#cp_src)
	ld	e, a
	ld	a, (#cp_src + 1)
	ld	d, a
	ld	a, (#cp_dst)
	ld	l, a
	ld	a, (#cp_dst + 1)
	ld	h, a
	ld	a, (#RP_H)
	ld	(#cp_rows), a
	ld	a, (#RP_Y)
	ld	(#cp_y), a
	ld	a, (#RP_OP)
	cp	a, #2
	jp	z, cp_or
	jp	c, cp_copy
cp_xor:
	;; A run down one tile: 8 less its row, or the rows left.
	ld	a, (#cp_y)
	ld	b, a
	ld	a, #8
	sub	a, b
	ld	b, a
	ld	a, (#cp_rows)
	cp	a, b
	jr	nc, 1$
	ld	b, a
1$:
	sub	a, b
	ld	(#cp_rows), a
	ld	a, (#cp_mask)
	ld	c, a
2$:
	ld	a, (de)
	and	a, c
	xor	a, (hl)
	ld	(hl+), a
	ld	a, (#RP_STRIDE)
	add	a, e
	ld	e, a
	jr	nc, 3$
	inc	d
3$:
	dec	b
	jr	nz, 2$
	ld	a, (#cp_rows)
	or	a, a
	jr	z, cp_column_done
	xor	a, a
	ld	(#cp_y), a
	ld	bc, #128 - 8		; the same column in the row of tiles below
	add	hl, bc
	jr	cp_xor
cp_or:
	;; A run down one tile: 8 less its row, or the rows left.
	ld	a, (#cp_y)
	ld	b, a
	ld	a, #8
	sub	a, b
	ld	b, a
	ld	a, (#cp_rows)
	cp	a, b
	jr	nc, 1$
	ld	b, a
1$:
	sub	a, b
	ld	(#cp_rows), a
	ld	a, (#cp_mask)
	ld	c, a
2$:
	ld	a, (de)
	and	a, c
	or	a, (hl)
	ld	(hl+), a
	ld	a, (#RP_STRIDE)
	add	a, e
	ld	e, a
	jr	nc, 3$
	inc	d
3$:
	dec	b
	jr	nz, 2$
	ld	a, (#cp_rows)
	or	a, a
	jr	z, cp_column_done
	xor	a, a
	ld	(#cp_y), a
	ld	bc, #128 - 8		; the same column in the row of tiles below
	add	hl, bc
	jr	cp_or
cp_copy:
	;; A run down one tile: 8 less its row, or the rows left.
	ld	a, (#cp_y)
	ld	b, a
	ld	a, #8
	sub	a, b
	ld	b, a
	ld	a, (#cp_rows)
	cp	a, b
	jr	nc, 1$
	ld	b, a
1$:
	sub	a, b
	ld	(#cp_rows), a
	ld	a, (#cp_mask)
	ld	c, a
2$:
	ld	a, (de)
	xor	a, (hl)
	and	a, c
	xor	a, (hl)
	ld	(hl+), a
	ld	a, (#RP_STRIDE)
	add	a, e
	ld	e, a
	jr	nc, 3$
	inc	d
3$:
	dec	b
	jr	nz, 2$
	ld	a, (#cp_rows)
	or	a, a
	jr	z, cp_column_done
	xor	a, a
	ld	(#cp_y), a
	ld	bc, #128 - 8		; the same column in the row of tiles below
	add	hl, bc
	jr	cp_copy
cp_column_done:
	ld	a, (#cp_left)
	dec	a
	ret	z
	ld	(#cp_left), a
	ld	a, #0xff
	ld	(#cp_mask), a
	ld	hl, #cp_src		; the next column: one on in the source,
	inc	(hl)
	jr	nz, 1$
	inc	hl
	inc	(hl)
1$:
	ld	a, (#cp_dst)		; 8 on in the screen (within its 256)
	add	a, #8
	ld	(#cp_dst), a
	jp	cp_column

;; gb_rows_put for a picture three bytes across or more, a row at a time:
;; the source and the screen stay in de and hl from row to row, a row
;; ending 8 * n on in the screen and n on in the source.
rows_put_wide:
	ld	a, (#RP_N)
	add	a, a
	add	a, a
	add	a, a
	ld	(#rp_back), a		; 8 * n
	ld	a, (#RP_N)
	ld	b, a
	ld	a, (#RP_STRIDE)
	sub	a, b
	ld	(#rp_skip), a		; stride - n, maybe less than 0
	ld	a, (#RP_SRC)
	ld	e, a
	ld	a, (#RP_SRC + 1)
	ld	d, a
	ld	a, (#RP_DST)
	ld	l, a
	ld	a, (#RP_DST + 1)
	ld	h, a
	ld	a, (#RP_OP)
	cp	a, #2
	jp	z, put_or
	jp	c, put_copy
put_xor:
1$:
	ld	a, (#RP_N)
	ld	b, a
	ld	a, (#RP_FIRST)
	ld	c, a
	dec	b
	jr	z, 4$			; one byte: both masks
	ld	a, (de)
	and	a, c
	xor	a, (hl)
	ld	(hl), a
	inc	de
	ld	a, l
	add	a, #8
	ld	l, a
	dec	b
	jr	z, 3$
2$:					; the bytes between, whole
	ld	a, (de)
	inc	de
	xor	a, (hl)
	ld	(hl), a
	ld	a, l
	add	a, #8
	ld	l, a
	dec	b
	jr	nz, 2$
3$:
	ld	a, (#RP_LAST)
	ld	c, a
	jr	5$
4$:
	ld	a, (#RP_LAST)
	and	a, c
	ld	c, a
5$:
	ld	a, (de)
	and	a, c
	xor	a, (hl)
	ld	(hl), a
	inc	de
	ld	a, l
	add	a, #8
	ld	l, a
	call	row_next
	jr	nz, 1$
	ret
put_or:
1$:
	ld	a, (#RP_N)
	ld	b, a
	ld	a, (#RP_FIRST)
	ld	c, a
	dec	b
	jr	z, 4$			; one byte: both masks
	ld	a, (de)
	and	a, c
	or	a, (hl)
	ld	(hl), a
	inc	de
	ld	a, l
	add	a, #8
	ld	l, a
	dec	b
	jr	z, 3$
2$:					; the bytes between, whole
	ld	a, (de)
	inc	de
	or	a, (hl)
	ld	(hl), a
	ld	a, l
	add	a, #8
	ld	l, a
	dec	b
	jr	nz, 2$
3$:
	ld	a, (#RP_LAST)
	ld	c, a
	jr	5$
4$:
	ld	a, (#RP_LAST)
	and	a, c
	ld	c, a
5$:
	ld	a, (de)
	and	a, c
	or	a, (hl)
	ld	(hl), a
	inc	de
	ld	a, l
	add	a, #8
	ld	l, a
	call	row_next
	jr	nz, 1$
	ret
put_copy:
1$:
	ld	a, (#RP_N)
	ld	b, a
	ld	a, (#RP_FIRST)
	ld	c, a
	dec	b
	jr	z, 4$			; one byte: both masks
	ld	a, (de)
	xor	a, (hl)
	and	a, c
	xor	a, (hl)
	ld	(hl), a
	inc	de
	ld	a, l
	add	a, #8
	ld	l, a
	dec	b
	jr	z, 3$
2$:					; the bytes between, whole
	ld	a, (de)
	inc	de
	ld	(hl), a
	ld	a, l
	add	a, #8
	ld	l, a
	dec	b
	jr	nz, 2$
3$:
	ld	a, (#RP_LAST)
	ld	c, a
	jr	5$
4$:
	ld	a, (#RP_LAST)
	and	a, c
	ld	c, a
5$:
	ld	a, (de)
	xor	a, (hl)
	and	a, c
	xor	a, (hl)
	ld	(hl), a
	inc	de
	ld	a, l
	add	a, #8
	ld	l, a
	call	row_next
	jr	nz, 1$
	ret
;; On to the next row, de from the row's end by the stride less n, hl
;; back to the row's start and one on, or to the next row of tiles.
;; Returns with Z set when it was the last.
row_next:
	ld	a, (#rp_skip)
	ld	c, a
	rlca
	sbc	a, a			; its sign, for the high byte
	ld	b, a
	ld	a, e
	add	a, c
	ld	e, a
	ld	a, d
	adc	a, b
	ld	d, a
	ld	a, (#rp_back)
	ld	b, a
	ld	a, l
	sub	a, b
	ld	l, a
	ld	a, (#RP_Y)
	inc	a
	and	a, #7
	ld	(#RP_Y), a
	jr	z, 1$
	inc	l
	jr	2$
1$:
	ld	a, l
	add	a, #128 - 7
	ld	l, a
	jr	nc, 2$
	inc	h
2$:
	ld	a, (#RP_H)
	dec	a
	ld	(#RP_H), a
	ret

;; void gb_fill(uint8_t *dst, uint8_t value, uint8_t eights): sets
;; eights * 8 bytes (at least 8) from dst to value. dst in de, value in a,
;; eights on the stack, which this takes off.
_gb_fill::
	ld	b, a
	ldhl	sp, #2
	ld	c, (hl)
	ld	h, d
	ld	l, e
	ld	a, b
1$:
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	ld	(hl+), a
	dec	c
	jr	nz, 1$
	pop	hl
	inc	sp
	jp	(hl)

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

;; void gb_rows_present(void): each tile of si_rows_ram's screen that
;; differs from its shown: copied there, into lcd_fb's rows, and into the
;; first bit plane of its tile in video RAM (the palette shows the second
;; as nothing). Each row of tiles starts in lcd_fb at gb_rows_fb[ty] and
;; in video RAM at gb_rows_tiles[ty]. The screen and shown are rows of
;; 128 bytes from 128-byte boundaries, shown 9 * 128 after the screen: a
;; tile starts at a multiple of 8, and the scan steps through them by the
;; low bytes alone.
SHOWN_AFTER = 9 * 128
_gb_rows_present::
	xor	a, a
	ld	(#pr_ty), a
	ld	de, #_si_rows_ram
	ld	hl, #_si_rows_ram + SHOWN_AFTER
pr_row:
	push	de
	push	hl
	ld	a, (#pr_ty)
	add	a, a
	ld	e, a
	ld	d, #0
	ld	hl, #_gb_rows_fb
	add	hl, de
	ld	a, (hl+)
	ld	(#pr_fb), a
	ld	a, (hl)
	ld	(#pr_fb + 1), a
	ld	hl, #_gb_rows_tiles
	add	hl, de
	ld	a, (hl+)
	ld	(#pr_tile), a
	ld	a, (hl)
	ld	(#pr_tile + 1), a
	pop	hl
	pop	de
	ld	b, #12			; tiles left in the row
pr_tile_loop:
	ld	a, (de)
	cp	a, (hl)
	jr	nz, pr_changed
	inc	e
	inc	l
	ld	a, (de)
	cp	a, (hl)
	jr	nz, pr_changed
	inc	e
	inc	l
	ld	a, (de)
	cp	a, (hl)
	jr	nz, pr_changed
	inc	e
	inc	l
	ld	a, (de)
	cp	a, (hl)
	jr	nz, pr_changed
	inc	e
	inc	l
	ld	a, (de)
	cp	a, (hl)
	jr	nz, pr_changed
	inc	e
	inc	l
	ld	a, (de)
	cp	a, (hl)
	jr	nz, pr_changed
	inc	e
	inc	l
	ld	a, (de)
	cp	a, (hl)
	jr	nz, pr_changed
	inc	e
	inc	l
	ld	a, (de)
	cp	a, (hl)
	jr	nz, pr_changed
	inc	e
	inc	l
pr_next:
	dec	b
	jr	nz, pr_tile_loop
	;; The rest of the row's 128 bytes, unused.
	ld	a, e
	add	a, #32
	ld	e, a
	jr	nc, 1$
	inc	d
1$:
	ld	a, l
	add	a, #32
	ld	l, a
	jr	nc, 2$
	inc	h
2$:
	ld	a, (#pr_ty)
	inc	a
	ld	(#pr_ty), a
	cp	a, #9
	jp	nz, pr_row
	ret

pr_changed:
	ld	a, e
	and	a, #0xf8
	ld	e, a			; the tile's first byte
	push	bc
	push	de
	;; Shown, from now on.
	ld	hl, #SHOWN_AFTER
	add	hl, de
	ld	c, #8
1$:
	ld	a, (de)
	inc	e
	ld	(hl+), a
	dec	c
	jr	nz, 1$
	;; Its column: 12 less the tiles left.
	ld	a, #12
	sub	a, b
	ld	(#pr_tx), a
	;; Into lcd_fb, rows 20 bytes apart.
	ld	c, a
	ld	b, #0
	ld	a, (#pr_fb)
	ld	l, a
	ld	a, (#pr_fb + 1)
	ld	h, a
	add	hl, bc
	pop	de
	push	de
	ld	c, #8
2$:
	ld	a, (de)
	inc	e
	ld	(hl), a
	ld	a, l
	add	a, #20
	ld	l, a
	jr	nc, 3$
	inc	h
3$:
	dec	c
	jr	nz, 2$
	;; Into video RAM, four rows at a time.
	ld	a, (#pr_tx)
	swap	a			; 16 bytes a tile
	ld	c, a
	ld	b, #0
	ld	a, (#pr_tile)
	ld	l, a
	ld	a, (#pr_tile + 1)
	ld	h, a
	add	hl, bc
	pop	de
	push	de
	push	hl
	ld	h, d
	ld	l, e
	ld	a, (hl+)
	ld	b, a
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	ld	d, a
	ld	e, (hl)
	pop	hl
	call	plane_rows
	pop	de
	push	de
	push	hl
	ld	h, d
	ld	l, e
	inc	l
	inc	l
	inc	l
	inc	l
	ld	a, (hl+)
	ld	b, a
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	ld	d, a
	ld	e, (hl)
	pop	hl
	call	plane_rows
	;; On to the next tile.
	pop	de
	pop	bc
	ld	a, e
	add	a, #8
	ld	e, a
	ld	hl, #SHOWN_AFTER
	add	hl, de
	jp	pr_next

;; Writes b, c, d, e to (hl), (hl + 2), (hl + 4), (hl + 6), four rows of a
;; tile's first bit plane, and leaves hl eight on. As flush_tiles in
;; crt0.s, the writes wait for a horizontal or vertical blank (or an LCD
;; that is off) and land by the next line's OAM scan at the latest.
plane_rows:
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

;; uint8_t gb_any_dirty(void): whether any of lcd_dirty's 360 cells is set,
;; eight at a time.
_gb_any_dirty::
	ld	hl, #_lcd_dirty
	ld	b, #45
	xor	a, a
1$:
	or	a, (hl)
	inc	hl
	or	a, (hl)
	inc	hl
	or	a, (hl)
	inc	hl
	or	a, (hl)
	inc	hl
	or	a, (hl)
	inc	hl
	or	a, (hl)
	inc	hl
	or	a, (hl)
	inc	hl
	or	a, (hl)
	inc	hl
	jr	nz, 2$
	dec	b
	jr	nz, 1$
	ret
2$:
	ld	a, #1
	ret

;; uint8_t gb_rows_made(const struct si_pic *p, uint8_t op): core/si_rows.c's
;; draw_bitmap and draw_made for a picture of the game's own data (p in de,
;; op in a): its ready-made rows from the rows' bank, clipped, through
;; gb_rows_put. Returns 0, having done nothing, for a picture the game
;; made itself (not among si_pictures), else 1. It maps a Space Impact
;; bank for si_pictures, then the rows', then the bank that was.
SI_ROWS_FIRST_BANK = 9
SI_FIRST_BANK = 5			; any of Space Impact's, for si_pictures
_gb_rows_made::
	ld	(#rm_op), a
	ld	a, (#_far_mapped)
	ld	(#rm_was), a
	ld	a, #SI_FIRST_BANK
	ld	(#_far_mapped), a
	ld	(#0x2000), a
	ld	h, d
	ld	l, e
	ld	bc, #4
	add	hl, bc
	ld	a, (hl+)
	ld	(#rm_x), a
	ld	a, (hl+)
	ld	(#rm_x + 1), a
	ld	a, (hl+)
	ld	(#rm_y), a
	ld	a, (hl+)
	ld	(#rm_y + 1), a
	inc	hl
	inc	hl
	inc	hl
	inc	hl
	ld	a, (hl+)		; frames
	ld	c, a
	ld	a, (hl+)
	ld	b, a
	inc	hl
	ld	l, (hl)			; frame
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, bc			; im, 4 bytes each
	push	hl
	inc	hl
	inc	hl
	ld	a, (hl+)
	ld	(#rm_w), a
	ld	a, (hl)
	ld	(#rm_h), a
	pop	hl
	;; Its index among si_pictures, if it is one of them.
	ld	a, l
	sub	a, #<_si_pictures
	ld	e, a
	ld	a, h
	sbc	a, #>_si_pictures
	ld	d, a
	jp	c, made_ram
	srl	d
	rr	e
	srl	d
	rr	e
	ld	a, d
	or	a, a
	jp	nz, made_ram
	ld	a, (#_gb_rows_count)
	ld	b, a
	ld	a, e
	cp	a, b
	jr	c, made_ours
made_ram:
	call	made_back
	xor	a, a
	ret
;; The bank that was mapped, back.
made_back:
	ld	a, (#rm_was)
	ld	(#_far_mapped), a
	ld	(#0x2000), a
	ret
made_ours:
	push	de			; e: the index
	;; Not drawn from the last column or row; wholly off the left or the
	;; top, nothing to draw.
	ld	a, (#rm_x + 1)
	bit	7, a
	jr	nz, 1$
	or	a, a
	jp	nz, made_skip
	ld	a, (#rm_x)
	cp	a, #95
	jp	nc, made_skip
	jr	2$
1$:
	inc	a
	jp	nz, made_skip		; left of -256
	ld	a, (#rm_w)
	ld	b, a
	ld	a, (#rm_x)
	add	a, b
	jp	nc, made_skip		; x + w still negative
	or	a, a
	jp	z, made_skip		; x + w = 0
2$:
	ld	a, (#rm_y + 1)
	bit	7, a
	jr	nz, 3$
	or	a, a
	jp	nz, made_skip
	ld	a, (#rm_y)
	cp	a, #64
	jp	nc, made_skip
	jr	4$
3$:
	inc	a
	jp	nz, made_skip
	ld	a, (#rm_h)
	ld	b, a
	ld	a, (#rm_y)
	add	a, b
	jp	nc, made_skip
	or	a, a
	jp	z, made_skip
4$:
	;; s, n and x8: the place in the byte, the row's bytes, the first's
	;; column of bytes.
	ld	a, (#rm_x)
	and	a, #7
	ld	(#rm_s), a
	ld	b, a
	ld	a, (#rm_w)
	add	a, b
	add	a, #7
	srl	a
	srl	a
	srl	a
	ld	(#rm_n), a
	ld	c, a
	ld	a, (#rm_x)
	srl	a
	srl	a
	srl	a
	ld	b, a
	ld	a, (#rm_x + 1)
	bit	7, a
	ld	a, b
	jr	z, 5$
	or	a, #0xe0
5$:
	ld	(#rm_x8), a
	;; skip, count.
	ld	b, a			; x8
	xor	a, a
	bit	7, b
	jr	z, 6$
	sub	a, b			; -x8
6$:
	ld	(#rm_skip), a
	ld	a, b
	add	a, c			; x8 + n
	bit	7, a
	jr	nz, 7$
	cp	a, #13
	jr	c, 7$
	ld	a, #12
	sub	a, b
	jr	8$
7$:
	ld	a, c
8$:
	ld	(#rm_count), a
	ld	b, a
	ld	a, (#rm_skip)
	cp	a, b
	jp	nc, made_skip
	;; r0, hh: the first row drawn, the rows there are.
	ld	a, (#rm_h)
	ld	c, a
	ld	a, (#rm_y + 1)
	bit	7, a
	jr	z, 9$
	xor	a, a
	ld	b, a
	ld	a, (#rm_y)
	cpl
	inc	a			; -y
	jr	10$
9$:
	ld	a, (#rm_y)		; y + h past 65: 65 - y
	add	a, c
	cp	a, #66
	jr	c, 11$
	ld	a, (#rm_y)
	cpl
	add	a, #66			; 65 - y
	ld	c, a
11$:
	xor	a, a
10$:
	ld	(#rm_r0), a
	ld	b, a
	ld	a, c
	ld	(#rm_hh), a
	cp	a, b
	jp	z, made_skip
	jp	c, made_skip
	;; The rows' bank, and the picture's entry in it.
	pop	de
	ld	d, #0
	ld	hl, #_si_rows_bank
	add	hl, de
	ld	b, (hl)			; 0 or 1
	ld	hl, #_si_rows_at
	add	hl, de
	add	hl, de
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a			; its place
	ld	a, b
	add	a, #SI_ROWS_FIRST_BANK
	ld	(#_far_mapped), a
	ld	(#0x2000), a
	ld	de, #_si_rows_a
	ld	a, b
	or	a, a
	jr	z, 12$
	ld	de, #_si_rows_b
12$:
	add	hl, de			; the entry
	push	hl
	ld	a, (#rm_s)
	add	a, a
	ld	e, a
	ld	d, #0
	add	hl, de
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a			; the place's offset in the entry
	pop	de
	add	hl, de			; its first row
	;; On by r0 rows and skip bytes.
	ld	a, (#rm_n)
	ld	c, a
	ld	b, #0
	ld	a, (#rm_r0)
	or	a, a
	jr	z, 14$
13$:
	add	hl, bc
	dec	a
	jr	nz, 13$
14$:
	ld	a, (#rm_skip)
	ld	e, a
	ld	d, #0
	add	hl, de
	ld	a, l
	ld	(#RP_SRC), a
	ld	a, h
	ld	(#RP_SRC + 1), a
	;; Where it goes: row y + r0 (y below 0 starts at 0), byte column
	;; x8 + skip.
	ld	a, (#rm_y + 1)
	bit	7, a
	ld	a, #0
	jr	nz, 15$
	ld	a, (#rm_y)
15$:
	ld	b, a			; y0
	and	a, #7
	ld	(#RP_Y), a
	ld	c, a			; the row in its tile
	ld	a, b
	srl	a
	srl	a
	srl	a			; the row of tiles
	ld	l, #0
	srl	a			; halves: 128 bytes each
	rr	l
	ld	h, a
	ld	a, (#rm_x8)
	ld	b, a
	ld	a, (#rm_skip)
	add	a, b
	add	a, a
	add	a, a
	add	a, a			; (x8 + skip) * 8
	add	a, c
	ld	e, a
	ld	d, #0
	add	hl, de
	ld	de, #_si_rows_ram
	add	hl, de
	ld	a, l
	ld	(#RP_DST), a
	ld	a, h
	ld	(#RP_DST + 1), a
	ld	a, (#rm_n)
	ld	(#RP_STRIDE), a
	ld	c, a
	ld	a, (#rm_skip)
	ld	b, a
	ld	a, (#rm_count)
	sub	a, b
	ld	(#RP_N), a
	ld	a, (#rm_r0)
	ld	b, a
	ld	a, (#rm_hh)
	sub	a, b
	ld	(#RP_H), a
	;; The masks.
	ld	a, (#rm_skip)
	or	a, a
	ld	a, #0xff
	jr	nz, 16$
	ld	a, (#rm_s)
	ld	e, a
	ld	d, #0
	ld	hl, #first_masks
	add	hl, de
	ld	a, (hl)
16$:
	ld	(#RP_FIRST), a
	ld	a, (#rm_count)
	cp	a, c			; count < n: clipped on the right
	ld	a, #0xff
	jr	c, 17$
	ld	a, (#rm_s)
	ld	b, a
	ld	a, (#rm_w)
	add	a, b
	and	a, #7
	ld	e, a
	ld	d, #0
	ld	hl, #last_masks
	add	hl, de
	ld	a, (hl)
17$:
	ld	(#RP_LAST), a
	ld	a, (#rm_op)
	ld	(#RP_OP), a
	call	_gb_rows_put
	call	made_back
	ld	a, #1
	ret
made_skip:
	pop	de
	call	made_back
	ld	a, #1
	ret

first_masks:
	.db	0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01
last_masks:
	.db	0xff, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe
;; uint8_t gb_shifted_left(const uint8_t *now, const uint8_t *was): whether
;; now's first 95 bytes are was's from its second (now in de, was in bc).
_gb_shifted_left::
	ld	h, b
	ld	l, c
	inc	hl
	ld	c, #95
1$:
	ld	a, (de)
	cp	a, (hl)
	jr	nz, 2$
	inc	de
	inc	hl
	dec	c
	jr	nz, 1$
	ld	a, #1
	ret
2$:
	xor	a, a
	ret

;; void gb_terrain_shift(uint8_t *rows, const uint8_t *last_column, uint8_t h):
;; h rows of 12 bytes, 20 apart (rows in de), each moved one pixel left, the
;; pixel coming in on the right that row's bit of the bitmap's last column
;; (its byte in the first band at last_column, in bc, the next band's 96
;; on). h is on the stack, which this takes off.
_gb_terrain_shift::
	ldhl	sp, #2
	ld	a, (hl)
	ld	(#ts_h), a
	ld	h, d
	ld	l, e
	ld	de, #11
	add	hl, de			; the first row's last byte
	ld	e, #1			; the row's bit in the column
1$:
	ld	a, (bc)
	and	a, e
	add	a, #0xff		; carry: the bit
	rl	(hl)
	dec	hl
	rl	(hl)
	dec	hl
	rl	(hl)
	dec	hl
	rl	(hl)
	dec	hl
	rl	(hl)
	dec	hl
	rl	(hl)
	dec	hl
	rl	(hl)
	dec	hl
	rl	(hl)
	dec	hl
	rl	(hl)
	dec	hl
	rl	(hl)
	dec	hl
	rl	(hl)
	dec	hl
	rl	(hl)
	dec	hl
	push	de
	ld	de, #32			; from before this row to the next's last byte
	add	hl, de
	pop	de
	sla	e
	jr	nz, 2$
	ld	e, #1			; the next band
	ld	a, c
	add	a, #96
	ld	c, a
	jr	nc, 2$
	inc	b
2$:
	ld	a, (#ts_h)
	dec	a
	ld	(#ts_h), a
	jr	nz, 1$
	pop	hl
	inc	sp
	jp	(hl)

;; void gb_palette(uint8_t shades): the background palette, the shade of
;; each of the colours 0 to 3 as BGP takes it (shades in a). In Color mode
;; (gb_cgb), where BGP is not used, background palette 0 too, the shades in
;; black and white: written in the vertical blank, as the palette is not
;; there to write while a line is drawn (at once with the LCD off).
_gb_palette::
	ld	(#0xff47), a
	ld	c, a
	ld	a, (#_gb_cgb)
	cp	a, #0x11
	ret	nz
	ld	a, (#0xff40)
	bit	7, a
	jr	z, 2$
1$:
	ld	a, (#0xff44)
	cp	a, #144
	jr	c, 1$
	cp	a, #152
	jr	nc, 1$
2$:
	ld	a, #0x80		; colour 0 of palette 0, then on by itself
	ld	(#0xff68), a
	ld	b, #4
3$:
	ld	a, c
	and	a, #3
	add	a, a
	ld	e, a
	ld	d, #0
	ld	hl, #shade_colours
	add	hl, de
	ld	a, (hl+)
	ld	(#0xff69), a
	ld	a, (hl)
	ld	(#0xff69), a
	srl	c
	srl	c
	dec	b
	jr	nz, 3$
	ret

;; White, light grey, dark grey, black, as RGB555.
shade_colours:
	.dw	0x7fff, 0x56b5, 0x294a, 0x0000
