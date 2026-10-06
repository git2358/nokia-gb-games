;; The Game Boy's own versions of what core/si_hit.c does most in a busy
;; game, which the compiler makes far too slow. In the ROM's first bank;
;; called from Space Impact's banks, whose copy of the data (si_pictures,
;; si_type_picture) is then mapped.
	.module si_fast
	.globl	_gb_find_hit
	.globl	_si_pictures, _si_type_picture, _si_pics, _si_records_changed

RECORDS = 60
RECORD_SIZE = 13		; struct object
O_FRAME = 1
O_TYPE = 2
O_PIC = 7
O_SIDE = 11
FREE = 0x7f
TYPE_SHIP = 0
TYPE_EXPLOSION = 2
SIDE_PLAYER = 0x0a
PIC_SIZE = 16			; struct si_pic
PIC_X = 4

	.area	_DATA
fh_ax:
	.ds	1
fh_ax2:
	.ds	1
fh_ay:
	.ds	1
fh_ay2:
	.ds	1
;; The records that can be hit, as of fh_stamp (si_records_changed), if
;; fh_valid: fh_count of them, each its number and its type's address.
fh_valid:
	.ds	1
fh_stamp:
	.ds	2
fh_count:
	.ds	1
fh_list:
	.ds	3 * RECORDS

	.area	_CODE

;; uint8_t gb_find_hit(const struct object *rec, uint8_t a): core/si_hit.c's
;; find_hit (0x25bae6) with its overlap (0x25ba34): the first record from
;; 0 that is live, not the ship or an explosion, not the player's, and
;; whose box, on the low bytes of the places, meets record a's; 0 for none.
;; rec (si.rec) in de, a in a.
_gb_find_hit::
	ld	h, d
	ld	l, e
	push	hl			; rec, for the scan
	;; Record a: rec + 13 * a = rec + 8a + 4a + a.
	ld	c, a
	ld	b, #0
	ld	h, b
	ld	l, c
	add	hl, hl
	add	hl, hl			; 4a
	push	hl
	add	hl, hl			; 8a
	pop	de
	add	hl, de			; 12a
	add	hl, bc			; 13a
	pop	de
	push	de
	add	hl, de
	call	object_box
	ld	a, b
	ld	(#fh_ax), a
	add	a, d
	ld	(#fh_ax2), a		; ax + width(a)
	ld	a, c
	ld	(#fh_ay), a
	add	a, e
	ld	(#fh_ay2), a		; ay + height(a)
	pop	hl			; rec
	;; The records that can be hit: made again when one may have become
	;; one (si_records_changed), else as they were.
	ld	a, (#fh_valid)
	or	a, a
	jr	z, 1$
	ld	a, (#_si_records_changed)
	ld	b, a
	ld	a, (#fh_stamp)
	cp	a, b
	jr	nz, 1$
	ld	a, (#_si_records_changed + 1)
	ld	b, a
	ld	a, (#fh_stamp + 1)
	cp	a, b
	jr	z, 7$
1$:
	call	list_make
7$:
	;; Each of them, as it is now: still of a hitting type, not the
	;; player's, its box meeting a's. The first by record, as the phone.
	ld	a, (#fh_count)
	or	a, a
	ret	z
	ld	b, a
	ld	hl, #fh_list
3$:
	ld	a, (hl+)
	ld	c, a			; its record
	ld	a, (hl+)
	ld	e, a
	ld	a, (hl+)
	ld	d, a			; its type
	push	hl
	push	bc
	ld	h, d
	ld	l, e
	ld	a, (hl)
	cp	a, #FREE
	jr	z, 5$
	or	a, a			; TYPE_SHIP
	jr	z, 5$
	cp	a, #TYPE_EXPLOSION
	jr	z, 5$
	call	candidate
	jr	c, 6$
5$:
	pop	bc
	pop	hl
	dec	b
	jr	nz, 3$
	xor	a, a
	ret
6$:
	pop	bc
	pop	hl
	ld	a, c
	ret

;; fh_list: each record (from rec, at hl) that can be hit now, in order:
;; its number and the address of its type.
list_make:
	ld	a, (#_si_records_changed)
	ld	(#fh_stamp), a
	ld	a, (#_si_records_changed + 1)
	ld	(#fh_stamp + 1), a
	ld	a, #1
	ld	(#fh_valid), a
	xor	a, a
	ld	(#fh_count), a
	inc	hl
	inc	hl			; record 0's type
	ld	de, #fh_list
	ld	c, #0
1$:
	ld	a, (hl)
	cp	a, #FREE
	jr	z, 3$
	or	a, a
	jr	z, 3$
	cp	a, #TYPE_EXPLOSION
	jr	z, 3$
	push	hl
	ld	a, l
	add	a, #O_SIDE - O_TYPE
	ld	l, a
	jr	nc, 2$
	inc	h
2$:
	ld	a, (hl)
	pop	hl
	cp	a, #SIDE_PLAYER
	jr	z, 3$
	ld	a, c
	ld	(de), a
	inc	de
	ld	a, l
	ld	(de), a
	inc	de
	ld	a, h
	ld	(de), a
	inc	de
	ld	a, (#fh_count)
	inc	a
	ld	(#fh_count), a
3$:
	push	de
	ld	de, #RECORD_SIZE
	add	hl, de
	pop	de
	inc	c
	ld	a, c
	cp	a, #RECORDS
	jr	nz, 1$
	ret

;; Record at hl - O_TYPE, live and of a hitting type: carry set when it is
;; not the player's and its box meets a's.
candidate:
	ld	de, #O_SIDE - O_TYPE
	push	hl
	add	hl, de
	ld	a, (hl)
	pop	hl
	cp	a, #SIDE_PLAYER
	jr	z, 7$
	dec	hl
	dec	hl			; the record
	push	hl
	;; Its place first: below or right of a's box rules it out without
	;; its size.
	ld	de, #O_PIC
	add	hl, de
	ld	l, (hl)
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	de, #_si_pics + PIC_X
	add	hl, de
	ld	b, (hl)			; bx
	inc	hl
	inc	hl
	ld	c, (hl)			; by
	pop	hl
	ld	a, (#fh_ay2)
	cp	a, c
	jr	c, 7$			; ay + ha < by
	ld	a, (#fh_ax2)
	cp	a, b
	jr	c, 7$			; ax + wa < bx
	call	object_box		; b, c: its place; d, e: its size
	ld	a, c
	add	a, e			; by + hb
	ld	hl, #fh_ay
	cp	a, (hl)
	jr	c, 7$			; by + hb < ay
	ld	a, b
	add	a, d			; bx + wb
	ld	hl, #fh_ax
	cp	a, (hl)
	jr	c, 7$			; bx + wb < ax
	scf
	ret
7$:
	and	a, a			; no carry
	ret

;; The record at hl: b, c the low bytes of its picture's place, d, e the
;; width and height of its current frame (core/si_int.h's width, height).
object_box:
	push	hl
	inc	hl
	ld	a, (hl+)		; frame
	ld	c, a
	ld	e, (hl)			; type
	ld	d, #0
	ld	hl, #_si_type_picture
	add	hl, de
	add	hl, de
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a			; its first picture
	ld	b, #0
	add	hl, bc			; and the frame's
	add	hl, hl
	add	hl, hl			; 4 bytes each
	ld	de, #_si_pictures + 2
	add	hl, de
	ld	a, (hl+)
	ld	d, a			; w
	ld	e, (hl)			; h
	pop	hl
	push	de
	ld	de, #O_PIC
	add	hl, de
	ld	l, (hl)
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl			; 16 bytes each
	ld	de, #_si_pics + PIC_X
	add	hl, de
	ld	b, (hl)			; x, low byte
	inc	hl
	inc	hl
	ld	c, (hl)			; y, low byte
	pop	de
	ret
