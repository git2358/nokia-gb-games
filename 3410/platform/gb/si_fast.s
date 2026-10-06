;; The Game Boy's own versions of what core/si_hit.c does most in a busy
;; game, which the compiler makes far too slow. In the ROM's first bank;
;; called from Space Impact's banks, whose copy of the data (si_pictures,
;; si_type_picture) is then mapped.
	.module si_fast
	.globl	_gb_find_hit
	.globl	_si_pictures, _si_type_picture, _si_pics

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
	pop	hl
	inc	hl
	inc	hl			; record 0's type
	ld	b, #RECORDS		; records left
	ld	de, #RECORD_SIZE
3$:
	ld	a, (hl)			; type
	cp	a, #FREE
	jr	z, 5$
	or	a, a			; TYPE_SHIP
	jr	z, 5$
	cp	a, #TYPE_EXPLOSION
	jr	z, 5$
	push	hl
	push	bc
	call	candidate
	pop	bc
	pop	hl
	jr	c, 6$
	ld	de, #RECORD_SIZE
5$:
	add	hl, de
	dec	b
	jr	nz, 3$
	xor	a, a
	ret
6$:
	ld	a, #RECORDS		; its index: the records there were less those left
	sub	a, b
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
