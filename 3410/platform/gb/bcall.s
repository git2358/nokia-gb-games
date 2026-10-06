;; The compiler's banked calls (SI_FAR in core/si.h), in the first bank:
;; E the callee's bank, HL the callee. The bank mapped before is kept on
;; the stack under the call, where the compiler looks past it for the
;; arguments, and mapped again after it; the result comes back in A or BC.
;; far.c's far_mapped follows, so that its far_ calls put back the right
;; bank when one is made from a banked function.
	.module bcall
	.globl	_far_mapped
	.globl	___sdcc_bcall_ehl

	.area	_CODE

___sdcc_bcall_ehl::
	ld	a, (_far_mapped)
	push	af
	ld	a, e
	ld	(_far_mapped), a
	ld	(#0x2000), a
	call	1$
	ld	l, a
	pop	af
	ld	(_far_mapped), a
	ld	(#0x2000), a
	ld	a, l
	ret
1$:
	jp	(hl)
