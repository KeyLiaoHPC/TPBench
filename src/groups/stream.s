	.arch armv8-a
	.file	"stream.c"
// GNU C17 (GCC) version 9.2.0 (aarch64-unknown-linux-gnu)
//	compiled by GNU C version 9.2.0, GMP version 6.0.0, MPFR version 3.1.1, MPC version 1.0.1, isl version isl-0.16.1-GMP

// GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
// options passed:  -I ../include ./stream.c -mlittle-endian -mabi=lp64
// -auxbase-strip stream.s -O3 -fno-builtin -fverbose-asm
// options enabled:  -faggressive-loop-optimizations -falign-functions
// -falign-jumps -falign-labels -falign-loops -fassume-phsa
// -fasynchronous-unwind-tables -fauto-inc-dec -fbranch-count-reg
// -fcaller-saves -fcode-hoisting -fcombine-stack-adjustments -fcommon
// -fcompare-elim -fcprop-registers -fcrossjumping -fcse-follow-jumps
// -fdefer-pop -fdelete-null-pointer-checks -fdevirtualize
// -fdevirtualize-speculatively -fdwarf2-cfi-asm -fearly-inlining
// -feliminate-unused-debug-types -fexpensive-optimizations
// -fforward-propagate -ffp-int-builtin-inexact -ffunction-cse -fgcse
// -fgcse-after-reload -fgcse-lm -fgnu-runtime -fgnu-unique
// -fguess-branch-probability -fhoist-adjacent-loads -fident
// -fif-conversion -fif-conversion2 -findirect-inlining -finline
// -finline-atomics -finline-functions -finline-functions-called-once
// -finline-small-functions -fipa-bit-cp -fipa-cp -fipa-cp-clone -fipa-icf
// -fipa-icf-functions -fipa-icf-variables -fipa-profile -fipa-pure-const
// -fipa-ra -fipa-reference -fipa-reference-addressable -fipa-sra
// -fipa-stack-alignment -fipa-vrp -fira-hoist-pressure
// -fira-share-save-slots -fira-share-spill-slots
// -fisolate-erroneous-paths-dereference -fivopts -fkeep-static-consts
// -fleading-underscore -flifetime-dse -floop-interchange
// -floop-unroll-and-jam -flra-remat -flto-odr-type-merging -fmath-errno
// -fmerge-constants -fmerge-debug-strings -fmove-loop-invariants
// -fomit-frame-pointer -foptimize-sibling-calls -foptimize-strlen
// -fpartial-inlining -fpeel-loops -fpeephole -fpeephole2 -fplt
// -fpredictive-commoning -fprefetch-loop-arrays -free -freg-struct-return
// -freorder-blocks -freorder-functions -frerun-cse-after-loop
// -fsched-critical-path-heuristic -fsched-dep-count-heuristic
// -fsched-group-heuristic -fsched-interblock -fsched-last-insn-heuristic
// -fsched-pressure -fsched-rank-heuristic -fsched-spec
// -fsched-spec-insn-heuristic -fsched-stalled-insns-dep -fschedule-fusion
// -fschedule-insns -fschedule-insns2 -fsection-anchors
// -fsemantic-interposition -fshow-column -fshrink-wrap
// -fshrink-wrap-separate -fsigned-zeros -fsplit-ivs-in-unroller
// -fsplit-loops -fsplit-paths -fsplit-wide-types -fssa-backprop
// -fssa-phiopt -fstdarg-opt -fstore-merging -fstrict-aliasing
// -fstrict-volatile-bitfields -fsync-libcalls -fthread-jumps
// -ftoplevel-reorder -ftrapping-math -ftree-bit-ccp
// -ftree-builtin-call-dce -ftree-ccp -ftree-ch -ftree-coalesce-vars
// -ftree-copy-prop -ftree-cselim -ftree-dce -ftree-dominator-opts
// -ftree-dse -ftree-forwprop -ftree-fre -ftree-loop-distribution
// -ftree-loop-if-convert -ftree-loop-im -ftree-loop-ivcanon
// -ftree-loop-optimize -ftree-loop-vectorize -ftree-parallelize-loops=
// -ftree-partial-pre -ftree-phiprop -ftree-pre -ftree-pta -ftree-reassoc
// -ftree-scev-cprop -ftree-sink -ftree-slp-vectorize -ftree-slsr
// -ftree-sra -ftree-switch-conversion -ftree-tail-merge -ftree-ter
// -ftree-vrp -funit-at-a-time -funswitch-loops -funwind-tables
// -fverbose-asm -fversion-loops-for-strides -fzero-initialized-in-bss
// -mfix-cortex-a53-835769 -mfix-cortex-a53-843419 -mglibc -mlittle-endian
// -momit-leaf-frame-pointer -mpc-relative-literal-loads

	.text
	.align	2
	.p2align 3,,7
	.global	d_stream
	.type	d_stream, %function
d_stream:
.LFB21:
	.cfi_startproc
	stp	x29, x30, [sp, -176]!	//,,,
	.cfi_def_cfa_offset 176
	.cfi_offset 29, -176
	.cfi_offset 30, -168
	mov	x29, sp	//,
	stp	x21, x22, [sp, 32]	//,,
	.cfi_offset 21, -144
	.cfi_offset 22, -136
// ./stream.c:44:     narr = nkib * 1024 / sizeof(double);
	lsl	x21, x4, 10	// _1, tmp372,
// ./stream.c:36: d_stream(int ntest, int nepoch, uint64_t **ns, uint64_t **cy, uint64_t nkib) {
	mov	x22, x3	// cy, tmp371
	stp	x19, x20, [sp, 16]	//,,
	.cfi_offset 19, -160
	.cfi_offset 20, -152
	mov	x19, x2	// ns, tmp370
	mov	w20, w0	// ntest, tmp369
// ./stream.c:45:     a = (double *)malloc(narr * sizeof(double));
	mov	x0, x21	//, _1
// ./stream.c:36: d_stream(int ntest, int nepoch, uint64_t **ns, uint64_t **cy, uint64_t nkib) {
	stp	x23, x24, [sp, 48]	//,,
	.cfi_offset 23, -128
	.cfi_offset 24, -120
// ./stream.c:44:     narr = nkib * 1024 / sizeof(double);
	ubfiz	x24, x4, 7, 54	// narr, tmp372,,
// ./stream.c:36: d_stream(int ntest, int nepoch, uint64_t **ns, uint64_t **cy, uint64_t nkib) {
	stp	x25, x26, [sp, 64]	//,,
// ./stream.c:44:     narr = nkib * 1024 / sizeof(double);
	str	x24, [sp, 120]	// narr, %sfp
	.cfi_offset 25, -112
	.cfi_offset 26, -104
// ./stream.c:45:     a = (double *)malloc(narr * sizeof(double));
	bl	malloc		//
	mov	x23, x0	// a, tmp373
	str	x0, [sp, 136]	// a, %sfp
// ./stream.c:46:     b = (double *)malloc(narr * sizeof(double));
	mov	x0, x21	//, _1
	bl	malloc		//
	mov	x25, x0	// b, tmp374
	str	x0, [sp, 144]	// b, %sfp
// ./stream.c:47:     c = (double *)malloc(narr * sizeof(double));
	mov	x0, x21	//, _1
	bl	malloc		//
// ./stream.c:49:     if(a == NULL || b == NULL || c == NULL) {
	cmp	x23, 0	// a,
// ./stream.c:47:     c = (double *)malloc(narr * sizeof(double));
	str	x0, [sp, 128]	// c, %sfp
// ./stream.c:49:     if(a == NULL || b == NULL || c == NULL) {
	ccmp	x25, 0, 4, ne	// b,,,
	ccmp	x0, 0, 4, ne	// c,,,
	beq	.L15		//,
// ./stream.c:53:     for(int n = 0; n < narr; n ++) {
	cbz	x24, .L3	// narr,
	mov	x2, x0	// c, tmp375
	mov	x1, x23	// a, a
	mov	x3, x25	// b, b
	add	x0, x23, x24, lsl 3	// _293, a, narr,
// ./stream.c:54:         a[n] = 1.0;
	fmov	d2, 1.0e+0	// tmp257,
// ./stream.c:55:         b[n] = 2.0;
	fmov	d1, 2.0e+0	// tmp258,
// ./stream.c:56:         c[n] = 3.0;
	fmov	d0, 3.0e+0	// tmp259,
	.p2align 3,,7
.L4:
// ./stream.c:54:         a[n] = 1.0;
	str	d2, [x1], 8	// tmp257, *_6
// ./stream.c:53:     for(int n = 0; n < narr; n ++) {
	cmp	x1, x0	// ivtmp.51, _293
// ./stream.c:55:         b[n] = 2.0;
	str	d1, [x3], 8	// tmp258, *_7
// ./stream.c:56:         c[n] = 3.0;
	str	d0, [x2], 8	// tmp259, *_8
// ./stream.c:53:     for(int n = 0; n < narr; n ++) {
	bne	.L4		//,
.L3:
// ./stream.c:59:     __getns_init;
	add	x1, sp, 160	// tmp385,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// ./stream.c:60:     __getcy_init;
#APP
// 60 "./stream.c" 1
	NOP
		
// 0 "" 2
// ./stream.c:61:     __getcy_grp_init;
// 61 "./stream.c" 1
	NOP
		
// 0 "" 2
// ./stream.c:63:     for(int n = 0; n < ntest; n++) {
#NO_APP
	cmp	w20, 0	// ntest,
	ble	.L5		//,
	ldr	x1, [sp, 120]	// narr, %sfp
	sub	w0, w20, #1	// tmp262, ntest,
	add	x0, x0, 1	// tmp263, tmp262,
	stp	d8, d9, [sp, 96]	//,,
	.cfi_offset 73, -72
	.cfi_offset 72, -80
	mov	x24, 0	// ivtmp.46,
	lsl	x20, x1, 3	// _264, narr,
	lsl	x0, x0, 3	// _226, tmp263,
	ldr	x1, [sp, 136]	// a, %sfp
	str	x0, [sp, 152]	// _226, %sfp
// ./stream.c:64:         __getns_2d_st(n, 0);
	mov	x0, 225833675390976	// tmp376,
	stp	x27, x28, [sp, 80]	//,,
	.cfi_offset 28, -88
	.cfi_offset 27, -96
	add	x23, x1, x20	// _235, a, _264
	ldr	x1, [sp, 128]	// c, %sfp
// ./stream.c:64:         __getns_2d_st(n, 0);
	movk	x0, 0x41cd, lsl 48	// tmp376,,
	fmov	d9, x0	// tmp269, tmp376
// ./stream.c:82:             b[i] = s * c[i];
	adrp	x0, .LC0	// tmp391,
	add	x21, x20, x1	// _272, _264, c
	ldr	x1, [sp, 144]	// b, %sfp
	ldr	d8, [x0, #:lo12:.LC0]	// tmp368,
	add	x20, x1, x20	// _227, b, _264
	.p2align 3,,7
.L14:
// ./stream.c:64:         __getns_2d_st(n, 0);
	add	x1, sp, 160	// tmp392,,
	mov	w0, 1	//,
	bl	clock_gettime		//
	ldp	d1, d0, [sp, 160]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
	ldr	x1, [x19, x24]	// MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B], MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B]
	scvtf	d1, d1	// tmp265, ts1.tv_sec
	scvtf	d0, d0	// tmp267, ts1.tv_nsec
// ./stream.c:65:         __getcy_grp_st(n);
	ldr	x0, [x22, x24]	// MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B], MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B]
// ./stream.c:64:         __getns_2d_st(n, 0);
	fmadd	d0, d1, d9, d0	// _15, tmp265, tmp269, tmp267
	fcvtzu	d0, d0	// tmp271, _15
	str	d0, [x1]	// tmp271, *_19
// ./stream.c:65:         __getcy_grp_st(n);
#APP
// 65 "./stream.c" 1
	mov x25, x0	// MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B]
	mrs x26, pmccntr_el0
// 0 "" 2
// ./stream.c:68:         __getns_2d_st(n, 1);
#NO_APP
	mov	w0, 1	//,
	add	x1, sp, 160	// tmp393,,
	bl	clock_gettime		//
	ldp	d1, d0, [sp, 160]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
	ldr	x1, [x19, x24]	// MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B], MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B]
	scvtf	d1, d1	// tmp274, ts1.tv_sec
	scvtf	d0, d0	// tmp276, ts1.tv_nsec
// ./stream.c:69:         __getcy_2d_st(n, 1);
	ldr	x0, [x22, x24]	// MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B], MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B]
// ./stream.c:68:         __getns_2d_st(n, 1);
	fmadd	d0, d1, d9, d0	// _28, tmp274, tmp269, tmp276
// ./stream.c:69:         __getcy_2d_st(n, 1);
	add	x0, x0, 8	// tmp281, MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B],
// ./stream.c:68:         __getns_2d_st(n, 1);
	fcvtzu	d0, d0	// tmp280, _28
	str	d0, [x1, 8]	// tmp280, MEM[(uint64_t *)_29 + 8B]
// ./stream.c:69:         __getcy_2d_st(n, 1);
#APP
// 69 "./stream.c" 1
	mov x28, x0	// tmp281
	mrs x29, pmccntr_el0
// 0 "" 2
// ./stream.c:71: 		for (i = 0; i < narr; i ++) {
#NO_APP
	ldr	x0, [sp, 120]	// narr, %sfp
	cbz	x0, .L6	// narr,
	ldp	x1, x0, [sp, 128]	// ivtmp.38, ivtmp.37, %sfp
	.p2align 3,,7
.L7:
// ./stream.c:72: 	        c[i] = a[i];
	ldr	d0, [x0], 8	// _36, *_34
// ./stream.c:72: 	        c[i] = a[i];
	str	d0, [x1], 8	// _36, *_35
// ./stream.c:71: 		for (i = 0; i < narr; i ++) {
	cmp	x23, x0	// _235, ivtmp.37
	bne	.L7		//,
.L6:
// ./stream.c:74:         __getcy_2d_en(n, 1);
#APP
// 74 "./stream.c" 1
	mrs x30, pmccntr_el0
	sub x30, x30, x29
	str x30, [x28]
// 0 "" 2
// ./stream.c:75:         __getns_2d_en(n, 1);
#NO_APP
	mov	w0, 1	//,
	add	x1, sp, 160	// tmp395,,
	bl	clock_gettime		//
	ldp	d2, d0, [sp, 160]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
// ./stream.c:78:         __getns_2d_st(n, 2);
	add	x1, sp, 160	// tmp396,,
// ./stream.c:75:         __getns_2d_en(n, 1);
	ldr	x3, [x19, x24]	// _44, MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B]
// ./stream.c:78:         __getns_2d_st(n, 2);
	mov	w0, 1	//,
// ./stream.c:75:         __getns_2d_en(n, 1);
	scvtf	d2, d2	// tmp284, ts1.tv_sec
	scvtf	d0, d0	// tmp286, ts1.tv_nsec
	ldr	d1, [x3, 8]	// MEM[(uint64_t *)_44 + 8B], MEM[(uint64_t *)_44 + 8B]
	fmadd	d0, d2, d9, d0	// _43, tmp284, tmp269, tmp286
	ucvtf	d1, d1	// tmp289, MEM[(uint64_t *)_44 + 8B]
	fsub	d0, d0, d1	// tmp291, _43, tmp289
	fcvtzu	d0, d0	// tmp292, tmp291
	str	d0, [x3, 8]	// tmp292, MEM[(uint64_t *)_44 + 8B]
// ./stream.c:78:         __getns_2d_st(n, 2);
	bl	clock_gettime		//
	ldp	d1, d0, [sp, 160]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
	ldr	x1, [x19, x24]	// MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B], MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B]
	scvtf	d1, d1	// tmp294, ts1.tv_sec
	scvtf	d0, d0	// tmp296, ts1.tv_nsec
// ./stream.c:79:         __getcy_2d_st(n, 2);
	ldr	x0, [x22, x24]	// MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B], MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B]
// ./stream.c:78:         __getns_2d_st(n, 2);
	fmadd	d0, d1, d9, d0	// _54, tmp294, tmp269, tmp296
// ./stream.c:79:         __getcy_2d_st(n, 2);
	add	x0, x0, 16	// tmp301, MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B],
// ./stream.c:78:         __getns_2d_st(n, 2);
	fcvtzu	d0, d0	// tmp300, _54
	str	d0, [x1, 16]	// tmp300, MEM[(uint64_t *)_55 + 16B]
// ./stream.c:79:         __getcy_2d_st(n, 2);
#APP
// 79 "./stream.c" 1
	mov x28, x0	// tmp301
	mrs x29, pmccntr_el0
// 0 "" 2
// ./stream.c:81: 		for (i = 0; i < narr; i ++) {
#NO_APP
	ldr	x0, [sp, 120]	// narr, %sfp
	cbz	x0, .L8	// narr,
	ldr	x0, [sp, 128]	// ivtmp.30, %sfp
	ldr	x1, [sp, 144]	// ivtmp.31, %sfp
	.p2align 3,,7
.L9:
// ./stream.c:82:             b[i] = s * c[i];
	ldr	d0, [x0], 8	// _61, *_60
// ./stream.c:82:             b[i] = s * c[i];
	fmul	d0, d0, d8	// _63, _61, tmp368
// ./stream.c:81: 		for (i = 0; i < narr; i ++) {
	cmp	x21, x0	// _272, ivtmp.30
// ./stream.c:82:             b[i] = s * c[i];
	str	d0, [x1], 8	// _63, *_62
// ./stream.c:81: 		for (i = 0; i < narr; i ++) {
	bne	.L9		//,
.L8:
// ./stream.c:84:         __getcy_2d_en(n, 2);
#APP
// 84 "./stream.c" 1
	mrs x30, pmccntr_el0
	sub x30, x30, x29
	str x30, [x28]
// 0 "" 2
// ./stream.c:85:         __getns_2d_en(n, 2);
#NO_APP
	mov	w0, 1	//,
	add	x1, sp, 160	// tmp398,,
	bl	clock_gettime		//
	ldp	d2, d0, [sp, 160]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
// ./stream.c:88:         __getns_2d_st(n, 3);
	add	x1, sp, 160	// tmp399,,
// ./stream.c:85:         __getns_2d_en(n, 2);
	ldr	x3, [x19, x24]	// _71, MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B]
// ./stream.c:88:         __getns_2d_st(n, 3);
	mov	w0, 1	//,
// ./stream.c:85:         __getns_2d_en(n, 2);
	scvtf	d2, d2	// tmp305, ts1.tv_sec
	scvtf	d0, d0	// tmp307, ts1.tv_nsec
	ldr	d1, [x3, 16]	// MEM[(uint64_t *)_71 + 16B], MEM[(uint64_t *)_71 + 16B]
	fmadd	d0, d2, d9, d0	// _70, tmp305, tmp269, tmp307
	ucvtf	d1, d1	// tmp310, MEM[(uint64_t *)_71 + 16B]
	fsub	d0, d0, d1	// tmp312, _70, tmp310
	fcvtzu	d0, d0	// tmp313, tmp312
	str	d0, [x3, 16]	// tmp313, MEM[(uint64_t *)_71 + 16B]
// ./stream.c:88:         __getns_2d_st(n, 3);
	bl	clock_gettime		//
	ldp	d1, d0, [sp, 160]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
	ldr	x1, [x19, x24]	// MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B], MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B]
	scvtf	d1, d1	// tmp315, ts1.tv_sec
	scvtf	d0, d0	// tmp317, ts1.tv_nsec
// ./stream.c:89:         __getcy_2d_st(n, 3);
	ldr	x0, [x22, x24]	// MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B], MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B]
// ./stream.c:88:         __getns_2d_st(n, 3);
	fmadd	d0, d1, d9, d0	// _81, tmp315, tmp269, tmp317
// ./stream.c:89:         __getcy_2d_st(n, 3);
	add	x0, x0, 24	// tmp322, MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B],
// ./stream.c:88:         __getns_2d_st(n, 3);
	fcvtzu	d0, d0	// tmp321, _81
	str	d0, [x1, 24]	// tmp321, MEM[(uint64_t *)_82 + 24B]
// ./stream.c:89:         __getcy_2d_st(n, 3);
#APP
// 89 "./stream.c" 1
	mov x28, x0	// tmp322
	mrs x29, pmccntr_el0
// 0 "" 2
// ./stream.c:91:         for (i = 0; i < narr; i ++) {
#NO_APP
	ldr	x0, [sp, 120]	// narr, %sfp
	cbz	x0, .L10	// narr,
	ldp	x3, x0, [sp, 128]	// ivtmp.24, ivtmp.22, %sfp
	ldr	x1, [sp, 144]	// ivtmp.23, %sfp
	.p2align 3,,7
.L11:
// ./stream.c:92:             c[i] = a[i] + b[i];
	ldr	d0, [x0], 8	// _88, *_87
// ./stream.c:92:             c[i] = a[i] + b[i];
	ldr	d1, [x1], 8	// _90, *_89
// ./stream.c:91:         for (i = 0; i < narr; i ++) {
	cmp	x0, x23	// ivtmp.22, _235
// ./stream.c:92:             c[i] = a[i] + b[i];
	fadd	d0, d0, d1	// _92, _88, _90
// ./stream.c:92:             c[i] = a[i] + b[i];
	str	d0, [x3], 8	// _92, *_91
// ./stream.c:91:         for (i = 0; i < narr; i ++) {
	bne	.L11		//,
.L10:
// ./stream.c:94:         __getcy_2d_en(n, 3);
#APP
// 94 "./stream.c" 1
	mrs x30, pmccntr_el0
	sub x30, x30, x29
	str x30, [x28]
// 0 "" 2
// ./stream.c:95:         __getns_2d_en(n, 3);
#NO_APP
	mov	w0, 1	//,
	add	x1, sp, 160	// tmp401,,
	bl	clock_gettime		//
	ldp	d2, d0, [sp, 160]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
// ./stream.c:98:         __getns_2d_st(n, 4);
	add	x1, sp, 160	// tmp402,,
// ./stream.c:95:         __getns_2d_en(n, 3);
	ldr	x3, [x19, x24]	// _100, MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B]
// ./stream.c:98:         __getns_2d_st(n, 4);
	mov	w0, 1	//,
// ./stream.c:95:         __getns_2d_en(n, 3);
	scvtf	d2, d2	// tmp325, ts1.tv_sec
	scvtf	d0, d0	// tmp327, ts1.tv_nsec
	ldr	d1, [x3, 24]	// MEM[(uint64_t *)_100 + 24B], MEM[(uint64_t *)_100 + 24B]
	fmadd	d0, d2, d9, d0	// _99, tmp325, tmp269, tmp327
	ucvtf	d1, d1	// tmp330, MEM[(uint64_t *)_100 + 24B]
	fsub	d0, d0, d1	// tmp332, _99, tmp330
	fcvtzu	d0, d0	// tmp333, tmp332
	str	d0, [x3, 24]	// tmp333, MEM[(uint64_t *)_100 + 24B]
// ./stream.c:98:         __getns_2d_st(n, 4);
	bl	clock_gettime		//
	ldp	d1, d0, [sp, 160]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
	ldr	x1, [x19, x24]	// MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B], MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B]
	scvtf	d1, d1	// tmp335, ts1.tv_sec
	scvtf	d0, d0	// tmp337, ts1.tv_nsec
// ./stream.c:99:         __getcy_2d_st(n, 4);
	ldr	x0, [x22, x24]	// MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B], MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B]
// ./stream.c:98:         __getns_2d_st(n, 4);
	fmadd	d0, d1, d9, d0	// _110, tmp335, tmp269, tmp337
// ./stream.c:99:         __getcy_2d_st(n, 4);
	add	x0, x0, 32	// tmp342, MEM[base: cy_177(D), index: ivtmp.46_236, offset: 0B],
// ./stream.c:98:         __getns_2d_st(n, 4);
	fcvtzu	d0, d0	// tmp341, _110
	str	d0, [x1, 32]	// tmp341, MEM[(uint64_t *)_111 + 32B]
// ./stream.c:99:         __getcy_2d_st(n, 4);
#APP
// 99 "./stream.c" 1
	mov x28, x0	// tmp342
	mrs x29, pmccntr_el0
// 0 "" 2
// ./stream.c:101: 	    for (i = 0; i < narr; i ++) {
#NO_APP
	ldr	x0, [sp, 120]	// narr, %sfp
	cbz	x0, .L12	// narr,
	ldp	x1, x3, [sp, 128]	// ivtmp.14, ivtmp.15, %sfp
	ldr	x0, [sp, 144]	// ivtmp.13, %sfp
	.p2align 3,,7
.L13:
// ./stream.c:102:             a[i] = b[i] + s * c[i];
	ldr	d0, [x0], 8	// _117, *_116
// ./stream.c:102:             a[i] = b[i] + s * c[i];
	ldr	d1, [x1], 8	// _119, *_118
// ./stream.c:101: 	    for (i = 0; i < narr; i ++) {
	cmp	x20, x0	// _227, ivtmp.13
// ./stream.c:102:             a[i] = b[i] + s * c[i];
	fmadd	d0, d1, d8, d0	// _122, _119, tmp368, _117
// ./stream.c:102:             a[i] = b[i] + s * c[i];
	str	d0, [x3], 8	// _122, *_121
// ./stream.c:101: 	    for (i = 0; i < narr; i ++) {
	bne	.L13		//,
.L12:
// ./stream.c:104:         __getcy_2d_en(n, 4);
#APP
// 104 "./stream.c" 1
	mrs x30, pmccntr_el0
	sub x30, x30, x29
	str x30, [x28]
// 0 "" 2
// ./stream.c:105:         __getns_2d_en(n, 4);
#NO_APP
	mov	w0, 1	//,
	add	x1, sp, 160	// tmp404,,
	bl	clock_gettime		//
	ldp	d2, d0, [sp, 160]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
	ldr	x0, [x19, x24]	// _130, MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B]
	scvtf	d2, d2	// tmp346, ts1.tv_sec
	scvtf	d0, d0	// tmp348, ts1.tv_nsec
	ldr	d1, [x0, 32]	// MEM[(uint64_t *)_130 + 32B], MEM[(uint64_t *)_130 + 32B]
	fmadd	d0, d2, d9, d0	// _129, tmp346, tmp269, tmp348
	ucvtf	d1, d1	// tmp351, MEM[(uint64_t *)_130 + 32B]
	fsub	d0, d0, d1	// tmp353, _129, tmp351
	fcvtzu	d0, d0	// tmp354, tmp353
	str	d0, [x0, 32]	// tmp354, MEM[(uint64_t *)_130 + 32B]
// ./stream.c:108:         __getcy_grp_en(n);
#APP
// 108 "./stream.c" 1
	mrs x27, pmccntr_el0
	sub x27, x27, x26
	str x27, [x25]
// 0 "" 2
// ./stream.c:109:         __getns_2d_en(n, 0);
#NO_APP
	mov	w0, 1	//,
	add	x1, sp, 160	// tmp405,,
	bl	clock_gettime		//
	ldp	d2, d0, [sp, 160]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
	ldr	x0, [x19, x24]	// _141, MEM[base: ns_175(D), index: ivtmp.46_236, offset: 0B]
	add	x24, x24, 8	// ivtmp.46, ivtmp.46,
	scvtf	d2, d2	// tmp356, ts1.tv_sec
	scvtf	d0, d0	// tmp358, ts1.tv_nsec
// ./stream.c:63:     for(int n = 0; n < ntest; n++) {
	ldr	x1, [sp, 152]	// _226, %sfp
// ./stream.c:109:         __getns_2d_en(n, 0);
	ldr	d1, [x0]	// *_141, *_141
	fmadd	d0, d2, d9, d0	// _140, tmp356, tmp269, tmp358
// ./stream.c:63:     for(int n = 0; n < ntest; n++) {
	cmp	x24, x1	// ivtmp.46, _226
// ./stream.c:109:         __getns_2d_en(n, 0);
	ucvtf	d1, d1	// tmp361, *_141
	fsub	d0, d0, d1	// tmp363, _140, tmp361
	fcvtzu	d0, d0	// tmp364, tmp363
	str	d0, [x0]	// tmp364, *_141
// ./stream.c:63:     for(int n = 0; n < ntest; n++) {
	bne	.L14		//,
	ldp	x27, x28, [sp, 80]	//,,
	.cfi_restore 28
	.cfi_restore 27
	ldp	d8, d9, [sp, 96]	//,,
	.cfi_restore 73
	.cfi_restore 72
.L5:
// ./stream.c:145:     free((void *)a);
	ldr	x0, [sp, 136]	//, %sfp
	bl	free		//
// ./stream.c:146:     free((void *)b);
	ldr	x0, [sp, 144]	//, %sfp
	bl	free		//
// ./stream.c:147:     free((void *)c);
	ldr	x0, [sp, 128]	//, %sfp
	bl	free		//
// ./stream.c:148:     return 0;
	mov	w0, 0	// <retval>,
.L1:
// ./stream.c:149: }
	ldp	x19, x20, [sp, 16]	//,,
	ldp	x21, x22, [sp, 32]	//,,
	ldp	x23, x24, [sp, 48]	//,,
	ldp	x25, x26, [sp, 64]	//,,
	ldp	x29, x30, [sp], 176	//,,,
	.cfi_remember_state
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 25
	.cfi_restore 26
	.cfi_restore 23
	.cfi_restore 24
	.cfi_restore 21
	.cfi_restore 22
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret	
.L15:
	.cfi_restore_state
// ./stream.c:50:         return MALLOC_FAIL;
	mov	w0, 7	// <retval>,
	b	.L1		//
	.cfi_endproc
.LFE21:
	.size	d_stream, .-d_stream
	.section	.rodata.cst8,"aM",@progbits,8
	.align	3
.LC0:
	.word	2920577761
	.word	1071309127
	.ident	"GCC: (GNU) 9.2.0"
	.section	.note.GNU-stack,"",@progbits
