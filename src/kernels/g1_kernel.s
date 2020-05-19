	.arch armv8-a
	.file	"g1_kernel.c"
// GNU C17 (GCC) version 9.2.0 (aarch64-unknown-linux-gnu)
//	compiled by GNU C version 9.2.0, GMP version 6.0.0, MPFR version 3.1.1, MPC version 1.0.1, isl version none
// GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
// options passed:  -I .. -I /usr/local/apps/openmpi-4.0.1_gcc920/include
// -D_REENTRANT
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c
// -march=armv8-a+simd -mlittle-endian -mabi=lp64 -mtune=thunderx2t99 -O3
// -fno-builtin -fverbose-asm
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
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	3
.LC0:
	.string	"%llu\n"
	.text
	.align	2
	.p2align 4,,15
	.global	run_init
	.type	run_init, %function
run_init:
.LFB21:
	.cfi_startproc
	stp	x29, x30, [sp, -80]!	//,,,
	.cfi_def_cfa_offset 80
	.cfi_offset 29, -80
	.cfi_offset 30, -72
	mov	x29, sp	//,
	str	d8, [sp, 40]	//,
	.cfi_offset 72, -40
	fmov	d8, d0	// s, tmp143
	stp	x19, x20, [sp, 16]	//,,
	.cfi_offset 19, -64
	.cfi_offset 20, -56
	mov	x19, x0	// tmp142, a
	mov	w20, w1	// narr, tmp144
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:84:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	mov	w0, 1	//,
	add	x1, sp, 48	// tmp147,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:81: run_init(TYPE *a, TYPE s, int narr, uint64_t *ns, uint64_t *cy){
	str	x21, [sp, 32]	//,
	.cfi_offset 21, -48
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:81: run_init(TYPE *a, TYPE s, int narr, uint64_t *ns, uint64_t *cy){
	mov	x21, x2	// ns, tmp145
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:84:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:85:     clock_gettime(CLOCK_MONOTONIC, &ts2);
	add	x1, sp, 64	// tmp148,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:86:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	add	x1, sp, 48	// tmp149,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:87:     CYCLE_ST;
#APP
// 87 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSH
	mrs x29, pmccntr_el0
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:88:     for(int i = 0; i < narr; i ++){
#NO_APP
	cmp	w20, 0	// narr,
	ble	.L2		//,
	sub	w1, w20, #1	// tmp118, narr,
	add	x2, x19, 8	// tmp119, ivtmp.24,
	mov	x0, x19	// ivtmp.24, a
	add	x1, x2, x1, uxtw 3	// _51, tmp119, tmp118,
	.p2align 4,,15
.L3:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:89:         a[i] = s;
	str	d8, [x0], 8	// s, MEM[base: _20, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:88:     for(int i = 0; i < narr; i ++){
	cmp	x0, x1	// ivtmp.24, _51
	bne	.L3		//,
.L2:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:91:     CYCLE_EN;
#APP
// 91 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSHST
	mrs x30, pmccntr_el0
	sub x30, x30, x29
	str x30, [x0]	// cy
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:93:     printf("%llu\n", *cy);
#NO_APP
	ldr	x1, [x0]	//, *cy_28
	adrp	x0, .LC0	// tmp124,
	add	x0, x0, :lo12:.LC0	//, tmp124,
	bl	printf		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:94:     fflush(stdout);
	adrp	x0, stdout	// tmp126,
	ldr	x0, [x0, #:lo12:stdout]	//, stdout
	bl	fflush		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:95:     clock_gettime(CLOCK_MONOTONIC, &ts2);
	add	x1, sp, 64	// tmp150,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:97: }
	ldp	x19, x20, [sp, 16]	//,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:96:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	ldp	d0, d1, [sp, 64]	// ts2.tv_sec, ts2.tv_nsec, ts2.tv_sec
	mov	x0, 225833675390976	// tmp146,
	movk	x0, 0x41cd, lsl 48	// tmp146,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:96:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	ldp	d4, d3, [sp, 48]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:97: }
	ldr	d8, [sp, 40]	//,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:96:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmov	d2, x0	// tmp133, tmp146
	scvtf	d1, d1	// tmp131, ts2.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:96:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d0, d0	// tmp129, ts2.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:96:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d4, d4	// tmp134, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:96:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d3, d3	// tmp137, ts1.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:96:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmadd	d0, d0, d2, d1	// _11, tmp129, tmp133, tmp131
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:96:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmsub	d0, d4, d2, d0	// _15, tmp134, tmp133, _11
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:96:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fsub	d0, d0, d3	// tmp139, _15, tmp137
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:96:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fcvtzu	d0, d0	// tmp140, tmp139
	str	d0, [x21]	// tmp140, *ns_32(D)
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:97: }
	ldr	x21, [sp, 32]	//,
	ldp	x29, x30, [sp], 80	//,,,
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 72
	.cfi_restore 21
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret	
	.cfi_endproc
.LFE21:
	.size	run_init, .-run_init
	.align	2
	.p2align 4,,15
	.global	run_sum
	.type	run_sum, %function
run_sum:
.LFB22:
	.cfi_startproc
	stp	x29, x30, [sp, -80]!	//,,,
	.cfi_def_cfa_offset 80
	.cfi_offset 29, -80
	.cfi_offset 30, -72
	mov	x29, sp	//,
	stp	x19, x20, [sp, 16]	//,,
	.cfi_offset 19, -64
	.cfi_offset 20, -56
	mov	x19, x0	// tmp137, a
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:107:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	mov	w0, 1	//,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:104: run_sum(TYPE *a, TYPE *s, int narr, uint64_t *ns, uint64_t *cy){
	mov	w20, w2	// narr, tmp139
	stp	x21, x22, [sp, 32]	//,,
	.cfi_offset 21, -48
	.cfi_offset 22, -40
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:104: run_sum(TYPE *a, TYPE *s, int narr, uint64_t *ns, uint64_t *cy){
	mov	x21, x1	// s, tmp138
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:107:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	add	x1, sp, 48	// tmp142,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:104: run_sum(TYPE *a, TYPE *s, int narr, uint64_t *ns, uint64_t *cy){
	mov	x22, x3	// ns, tmp140
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:107:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:108:     clock_gettime(CLOCK_MONOTONIC, &ts2);
	add	x1, sp, 64	// tmp143,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:109:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	add	x1, sp, 48	// tmp144,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:110:     CYCLE_ST;
#APP
// 110 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSH
	mrs x29, pmccntr_el0
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:111:     for(int i = 0; i < narr; i ++){
#NO_APP
	cmp	w20, 0	// narr,
	ble	.L10		//,
	ldr	d0, [x21]	// _6, *s_35(D)
	sub	w2, w20, #1	// tmp132, narr,
	add	x1, x19, 8	// tmp133, ivtmp.33,
	mov	x0, x19	// ivtmp.33, a
	add	x2, x1, x2, uxtw 3	// _49, tmp133, tmp132,
	.p2align 4,,15
.L9:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:112:         *s += a[i];
	ldr	d1, [x0], 8	// MEM[base: _44, offset: 0B], MEM[base: _44, offset: 0B]
	fadd	d0, d0, d1	// _6, _6, MEM[base: _44, offset: 0B]
	str	d0, [x21]	// _6, *s_35(D)
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:111:     for(int i = 0; i < narr; i ++){
	cmp	x0, x2	// ivtmp.33, _49
	bne	.L9		//,
.L10:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:114:     CYCLE_EN;
#APP
// 114 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSHST
	mrs x30, pmccntr_el0
	sub x30, x30, x29
	str x30, [x0]	// cy
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:115:     clock_gettime(CLOCK_MONOTONIC, &ts2);
#NO_APP
	mov	w0, 1	//,
	add	x1, sp, 64	// tmp145,,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:118: }
	ldp	x19, x20, [sp, 16]	//,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:117:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	ldp	d0, d1, [sp, 64]	// ts2.tv_sec, ts2.tv_nsec, ts2.tv_sec
	mov	x0, 225833675390976	// tmp141,
	movk	x0, 0x41cd, lsl 48	// tmp141,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:117:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	ldp	d4, d3, [sp, 48]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:117:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmov	d2, x0	// tmp124, tmp141
	scvtf	d1, d1	// tmp122, ts2.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:117:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d0, d0	// tmp120, ts2.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:117:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d4, d4	// tmp125, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:117:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d3, d3	// tmp128, ts1.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:117:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmadd	d0, d0, d2, d1	// _12, tmp120, tmp124, tmp122
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:117:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmsub	d0, d4, d2, d0	// _16, tmp125, tmp124, _12
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:117:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fsub	d0, d0, d3	// tmp130, _16, tmp128
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:117:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fcvtzu	d0, d0	// tmp131, tmp130
	str	d0, [x22]	// tmp131, *ns_31(D)
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:118: }
	ldp	x21, x22, [sp, 32]	//,,
	ldp	x29, x30, [sp], 80	//,,,
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 21
	.cfi_restore 22
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret	
	.cfi_endproc
.LFE22:
	.size	run_sum, .-run_sum
	.section	.rodata.str1.8
	.align	3
.LC1:
	.string	"xxx\n"
	.text
	.align	2
	.p2align 4,,15
	.global	run_copy
	.type	run_copy, %function
run_copy:
.LFB23:
	.cfi_startproc
	stp	x29, x30, [sp, -80]!	//,,,
	.cfi_def_cfa_offset 80
	.cfi_offset 29, -80
	.cfi_offset 30, -72
	mov	x29, sp	//,
	stp	x19, x20, [sp, 16]	//,,
	.cfi_offset 19, -64
	.cfi_offset 20, -56
	mov	x20, x0	// tmp137, a
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:128:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	mov	w0, 1	//,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:125: run_copy(TYPE *a, TYPE *b, int narr, uint64_t *ns, uint64_t *cy){
	mov	w19, w2	// narr, tmp139
	stp	x21, x22, [sp, 32]	//,,
	.cfi_offset 21, -48
	.cfi_offset 22, -40
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:125: run_copy(TYPE *a, TYPE *b, int narr, uint64_t *ns, uint64_t *cy){
	mov	x21, x1	// b, tmp138
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:128:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	add	x1, sp, 48	// tmp142,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:125: run_copy(TYPE *a, TYPE *b, int narr, uint64_t *ns, uint64_t *cy){
	mov	x22, x3	// ns, tmp140
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:128:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:129:     clock_gettime(CLOCK_MONOTONIC, &ts2);
	add	x1, sp, 64	// tmp143,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:130:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	add	x1, sp, 48	// tmp144,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:131:     CYCLE_ST;
#APP
// 131 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSH
	mrs x29, pmccntr_el0
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:132:     for(int i = 0; i < narr; i ++){
#NO_APP
	cmp	w19, 0	// narr,
	ble	.L15		//,
	sub	w4, w19, #1	// tmp118, narr,
	mov	x2, 0	// ivtmp.44,
	add	x4, x4, 1	// tmp119, tmp118,
	lsl	x4, x4, 3	// _39, tmp119,
	.p2align 4,,15
.L16:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:133:         a[i] = b[i];
	ldr	d0, [x21, x2]	// _5, MEM[base: b_35(D), index: ivtmp.44_48, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:133:         a[i] = b[i];
	str	d0, [x20, x2]	// _5, MEM[base: a_36(D), index: ivtmp.44_48, offset: 0B]
	add	x2, x2, 8	// ivtmp.44, ivtmp.44,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:132:     for(int i = 0; i < narr; i ++){
	cmp	x4, x2	// _39, ivtmp.44
	bne	.L16		//,
.L15:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:135:     CYCLE_EN;
#APP
// 135 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSHST
	mrs x30, pmccntr_el0
	sub x30, x30, x29
	str x30, [x0]	// cy
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:136:         printf("xxx\n");
#NO_APP
	adrp	x0, .LC1	// tmp122,
	add	x0, x0, :lo12:.LC1	//, tmp122,
	bl	printf		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:137:     clock_gettime(CLOCK_MONOTONIC, &ts2);
	add	x1, sp, 64	// tmp145,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:139: }
	ldp	x19, x20, [sp, 16]	//,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:138:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	ldp	d0, d1, [sp, 64]	// ts2.tv_sec, ts2.tv_nsec, ts2.tv_sec
	mov	x0, 225833675390976	// tmp141,
	movk	x0, 0x41cd, lsl 48	// tmp141,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:138:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	ldp	d4, d3, [sp, 48]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:138:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmov	d2, x0	// tmp128, tmp141
	scvtf	d1, d1	// tmp126, ts2.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:138:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d0, d0	// tmp124, ts2.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:138:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d4, d4	// tmp129, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:138:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d3, d3	// tmp132, ts1.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:138:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmadd	d0, d0, d2, d1	// _11, tmp124, tmp128, tmp126
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:138:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmsub	d0, d4, d2, d0	// _15, tmp129, tmp128, _11
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:138:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fsub	d0, d0, d3	// tmp134, _15, tmp132
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:138:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fcvtzu	d0, d0	// tmp135, tmp134
	str	d0, [x22]	// tmp135, *ns_31(D)
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:139: }
	ldp	x21, x22, [sp, 32]	//,,
	ldp	x29, x30, [sp], 80	//,,,
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 21
	.cfi_restore 22
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret	
	.cfi_endproc
.LFE23:
	.size	run_copy, .-run_copy
	.align	2
	.p2align 4,,15
	.global	run_update
	.type	run_update, %function
run_update:
.LFB24:
	.cfi_startproc
	stp	x29, x30, [sp, -80]!	//,,,
	.cfi_def_cfa_offset 80
	.cfi_offset 29, -80
	.cfi_offset 30, -72
	mov	x29, sp	//,
	str	d8, [sp, 40]	//,
	.cfi_offset 72, -40
	fmov	d8, d0	// s, tmp170
	stp	x19, x20, [sp, 16]	//,,
	.cfi_offset 19, -64
	.cfi_offset 20, -56
	mov	x20, x0	// tmp169, a
	mov	w19, w1	// narr, tmp171
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:149:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	mov	w0, 1	//,
	add	x1, sp, 48	// tmp177,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:146: run_update(TYPE *a, TYPE s, int narr, uint64_t *ns, uint64_t *cy){
	str	x21, [sp, 32]	//,
	.cfi_offset 21, -48
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:146: run_update(TYPE *a, TYPE s, int narr, uint64_t *ns, uint64_t *cy){
	mov	x21, x2	// ns, tmp172
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:149:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:150:     clock_gettime(CLOCK_MONOTONIC, &ts2);
	add	x1, sp, 64	// tmp178,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:151:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	add	x1, sp, 48	// tmp179,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:152:     CYCLE_ST;
#APP
// 152 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSH
	mrs x29, pmccntr_el0
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:153:     for(int i = 0; i < narr; i ++){
#NO_APP
	cmp	w19, 0	// narr,
	ble	.L20		//,
	sub	w0, w19, #1	// tmp136, narr,
	cmp	w0, 2	// tmp136,
	bls	.L24		//,
	dup	v2.2d, v8.d[0]	// vect_cst__64, s
	lsr	w4, w19, 1	// bnd.49, narr,
	mov	x3, x20	// ivtmp.60, a
	add	x4, x20, x4, uxtw 4	// _83, a, bnd.49,
	.p2align 4,,15
.L22:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	ldr	q1, [x3]	// vect__4.54, MEM[base: _78, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	fmul	v1.2d, v1.2d, v2.2d	// vect__5.55, vect__4.54, vect_cst__64
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	str	q1, [x3], 16	// vect__5.55, MEM[base: _78, offset: 0B]
	cmp	x3, x4	// ivtmp.60, _83
	bne	.L22		//,
	and	w1, w19, -2	// tmp.51, narr,
	tbz	x19, 0, .L20	// narr,,
.L21:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	sbfiz	x2, x1, 3, 32	// _3, tmp.51,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:153:     for(int i = 0; i < narr; i ++){
	add	w0, w1, 1	// i, tmp.51,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	add	x3, x20, x2	// _4, a, _3
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	ldr	d0, [x3]	// *_4, *_4
	fmul	d0, d0, d8	// tmp144, *_4, s
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	str	d0, [x3]	// tmp144, *_4
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:153:     for(int i = 0; i < narr; i ++){
	cmp	w19, w0	// narr, i
	ble	.L20		//,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	add	x0, x2, 8	// tmp147, _3,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:153:     for(int i = 0; i < narr; i ++){
	add	w1, w1, 2	// i, tmp.51,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	add	x0, x20, x0	// _72, a, tmp147
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	ldr	d0, [x0]	// *_72, *_72
	fmul	d0, d0, d8	// tmp148, *_72, s
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	str	d0, [x0]	// tmp148, *_72
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:153:     for(int i = 0; i < narr; i ++){
	cmp	w19, w1	// narr, i
	ble	.L20		//,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	add	x2, x2, 16	// tmp151, _3,
	add	x20, x20, x2	// _44, a, tmp151
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	ldr	d0, [x20]	// *_44, *_44
	fmul	d8, d0, d8	// tmp152, *_44, s
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:154:         a[i] = s * a[i];
	str	d8, [x20]	// tmp152, *_44
.L20:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:156:     CYCLE_EN;
#APP
// 156 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSHST
	mrs x30, pmccntr_el0
	sub x30, x30, x29
	str x30, [x0]	// cy
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:157:     clock_gettime(CLOCK_MONOTONIC, &ts2);
#NO_APP
	mov	w0, 1	//,
	add	x1, sp, 64	// tmp180,,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:159: }
	ldp	x19, x20, [sp, 16]	//,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:158:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	ldp	d0, d1, [sp, 64]	// ts2.tv_sec, ts2.tv_nsec, ts2.tv_sec
	mov	x0, 225833675390976	// tmp173,
	movk	x0, 0x41cd, lsl 48	// tmp173,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:158:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	ldp	d4, d3, [sp, 48]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:159: }
	ldr	d8, [sp, 40]	//,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:158:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmov	d2, x0	// tmp160, tmp173
	scvtf	d1, d1	// tmp158, ts2.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:158:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d0, d0	// tmp156, ts2.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:158:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d4, d4	// tmp161, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:158:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d3, d3	// tmp164, ts1.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:158:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmadd	d0, d0, d2, d1	// _11, tmp156, tmp160, tmp158
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:158:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmsub	d0, d4, d2, d0	// _15, tmp161, tmp160, _11
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:158:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fsub	d0, d0, d3	// tmp166, _15, tmp164
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:158:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fcvtzu	d0, d0	// tmp167, tmp166
	str	d0, [x21]	// tmp167, *ns_30(D)
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:159: }
	ldr	x21, [sp, 32]	//,
	ldp	x29, x30, [sp], 80	//,,,
	.cfi_remember_state
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 72
	.cfi_restore 21
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret	
.L24:
	.cfi_restore_state
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:153:     for(int i = 0; i < narr; i ++){
	mov	w1, 0	// tmp.51,
	b	.L21		//
	.cfi_endproc
.LFE24:
	.size	run_update, .-run_update
	.align	2
	.p2align 4,,15
	.global	run_triad
	.type	run_triad, %function
run_triad:
.LFB25:
	.cfi_startproc
	stp	x29, x30, [sp, -96]!	//,,,
	.cfi_def_cfa_offset 96
	.cfi_offset 29, -96
	.cfi_offset 30, -88
	mov	x29, sp	//,
	str	d8, [sp, 56]	//,
	.cfi_offset 72, -40
	fmov	d8, d0	// s, tmp189
	stp	x19, x20, [sp, 16]	//,,
	.cfi_offset 19, -80
	.cfi_offset 20, -72
	mov	x19, x0	// tmp186, a
	mov	x20, x1	// b, tmp187
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:169:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	mov	w0, 1	//,
	add	x1, sp, 64	// tmp196,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:166: run_triad(TYPE *a, TYPE *b, TYPE *c, TYPE s, int narr, uint64_t *ns, uint64_t *cy){
	stp	x21, x22, [sp, 32]	//,,
	.cfi_offset 21, -64
	.cfi_offset 22, -56
	mov	x21, x2	// c, tmp188
	mov	x22, x4	// ns, tmp191
	str	x23, [sp, 48]	//,
	.cfi_offset 23, -48
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:166: run_triad(TYPE *a, TYPE *b, TYPE *c, TYPE s, int narr, uint64_t *ns, uint64_t *cy){
	mov	w23, w3	// narr, tmp190
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:169:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:170:     clock_gettime(CLOCK_MONOTONIC, &ts2);
	add	x1, sp, 80	// tmp197,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:171:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	add	x1, sp, 64	// tmp198,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:172:     CYCLE_ST;
#APP
// 172 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSH
	mrs x29, pmccntr_el0
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:173:     for(int i = 0; i < narr; i ++){
#NO_APP
	cmp	w23, 0	// narr,
	ble	.L28		//,
	add	x1, x20, 15	// tmp147, b,
	add	x0, x21, 15	// tmp149, c,
	sub	w2, w23, #1	// _52, narr,
	sub	x1, x1, x19	// tmp148, tmp147, a
	sub	x0, x0, x19	// tmp150, tmp149, a
	cmp	x1, 30	// tmp148,
	ccmp	x0, 30, 0, hi	// tmp150,,,
	ccmp	w2, 4, 0, hi	// _52,,,
	bls	.L29		//,
	dup	v2.2d, v8.d[0]	// vect_cst__100, s
	lsr	w0, w23, 1	// bnd.68, narr,
	mov	x5, 0	// ivtmp.92,
	lsl	x0, x0, 4	// _43, bnd.68,
	.p2align 4,,15
.L30:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:175:         a[i] = b[i] + s * c[i];
	ldr	q0, [x21, x5]	// vect__6.76, MEM[base: c_39(D), index: ivtmp.92_7, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:175:         a[i] = b[i] + s * c[i];
	ldr	q1, [x20, x5]	// vect__4.73, MEM[base: b_38(D), index: ivtmp.92_7, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:175:         a[i] = b[i] + s * c[i];
	fmla	v1.2d, v0.2d, v2.2d	// vect__9.78, vect__6.76, vect_cst__100
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:175:         a[i] = b[i] + s * c[i];
	str	q1, [x19, x5]	// vect__9.78, MEM[base: a_41(D), index: ivtmp.92_7, offset: 0B]
	add	x5, x5, 16	// ivtmp.92, ivtmp.92,
	cmp	x5, x0	// ivtmp.92, _43
	bne	.L30		//,
	and	w3, w23, -2	//, narr,
	tbz	x23, 0, .L28	// narr,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:175:         a[i] = b[i] + s * c[i];
	lsl	x3, x3, 3	// _79, niters_vector_mult_vf.69,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:175:         a[i] = b[i] + s * c[i];
	ldr	d0, [x21, x3]	// *_82, *_82
	ldr	d1, [x20, x3]	// *_80, *_80
	fmadd	d0, d8, d0, d1	// _86, s, *_82, *_80
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:175:         a[i] = b[i] + s * c[i];
	str	d0, [x19, x3]	// _86, *_85
.L28:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:177:     CYCLE_EN;
#APP
// 177 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSHST
	mrs x30, pmccntr_el0
	sub x30, x30, x29
	str x30, [x0]	// cy
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:178:     clock_gettime(CLOCK_MONOTONIC, &ts2);
#NO_APP
	mov	w0, 1	//,
	add	x1, sp, 80	// tmp199,,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:180: }
	ldp	x19, x20, [sp, 16]	//,,
	ldr	x23, [sp, 48]	//,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:179:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	mov	x0, 225833675390976	// tmp192,
	ldp	d0, d1, [sp, 80]	// ts2.tv_sec, ts2.tv_nsec, ts2.tv_sec
	movk	x0, 0x41cd, lsl 48	// tmp192,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:179:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	ldp	d4, d3, [sp, 64]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:179:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmov	d2, x0	// tmp177, tmp192
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:180: }
	ldr	d8, [sp, 56]	//,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:179:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d1, d1	// tmp175, ts2.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:179:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d0, d0	// tmp173, ts2.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:179:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d4, d4	// tmp178, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:179:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d3, d3	// tmp181, ts1.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:179:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmadd	d0, d0, d2, d1	// _15, tmp173, tmp177, tmp175
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:179:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmsub	d0, d4, d2, d0	// _19, tmp178, tmp177, _15
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:179:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fsub	d0, d0, d3	// tmp183, _19, tmp181
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:179:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fcvtzu	d0, d0	// tmp184, tmp183
	str	d0, [x22]	// tmp184, *ns_34(D)
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:180: }
	ldp	x21, x22, [sp, 32]	//,,
	ldp	x29, x30, [sp], 96	//,,,
	.cfi_remember_state
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 72
	.cfi_restore 23
	.cfi_restore 21
	.cfi_restore 22
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret	
	.p2align 3,,7
.L29:
	.cfi_restore_state
	ubfiz	x1, x2, 3, 32	// tmp168, _52,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:173:     for(int i = 0; i < narr; i ++){
	mov	x0, 0	// ivtmp.88,
	add	x1, x1, 8	// _6, tmp168,
	.p2align 4,,15
.L32:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:175:         a[i] = b[i] + s * c[i];
	ldr	d1, [x21, x0]	// MEM[base: c_39(D), index: ivtmp.88_46, offset: 0B], MEM[base: c_39(D), index: ivtmp.88_46, offset: 0B]
	ldr	d0, [x20, x0]	// MEM[base: b_38(D), index: ivtmp.88_46, offset: 0B], MEM[base: b_38(D), index: ivtmp.88_46, offset: 0B]
	fmadd	d0, d8, d1, d0	// _71, s, MEM[base: c_39(D), index: ivtmp.88_46, offset: 0B], MEM[base: b_38(D), index: ivtmp.88_46, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:175:         a[i] = b[i] + s * c[i];
	str	d0, [x19, x0]	// _71, MEM[base: a_41(D), index: ivtmp.88_46, offset: 0B]
	add	x0, x0, 8	// ivtmp.88, ivtmp.88,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:173:     for(int i = 0; i < narr; i ++){
	cmp	x0, x1	// ivtmp.88, _6
	bne	.L32		//,
	b	.L28		//
	.cfi_endproc
.LFE25:
	.size	run_triad, .-run_triad
	.align	2
	.p2align 4,,15
	.global	run_daxpy
	.type	run_daxpy, %function
run_daxpy:
.LFB26:
	.cfi_startproc
	stp	x29, x30, [sp, -96]!	//,,,
	.cfi_def_cfa_offset 96
	.cfi_offset 29, -96
	.cfi_offset 30, -88
	mov	x29, sp	//,
	str	d8, [sp, 48]	//,
	.cfi_offset 72, -48
	fmov	d8, d0	// s, tmp183
	stp	x19, x20, [sp, 16]	//,,
	.cfi_offset 19, -80
	.cfi_offset 20, -72
	mov	x19, x0	// tmp181, a
	mov	x20, x1	// b, tmp182
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:190:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	mov	w0, 1	//,
	add	x1, sp, 64	// tmp189,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:187: run_daxpy(TYPE *a, TYPE *b, TYPE s, int narr, uint64_t *ns, uint64_t *cy){
	stp	x21, x22, [sp, 32]	//,,
	.cfi_offset 21, -64
	.cfi_offset 22, -56
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:187: run_daxpy(TYPE *a, TYPE *b, TYPE s, int narr, uint64_t *ns, uint64_t *cy){
	mov	w22, w2	// narr, tmp184
	mov	x21, x3	// ns, tmp185
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:190:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:191:     clock_gettime(CLOCK_MONOTONIC, &ts2);
	add	x1, sp, 80	// tmp190,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:192:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	add	x1, sp, 64	// tmp191,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:193:     CYCLE_ST;
#APP
// 193 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSH
	mrs x29, pmccntr_el0
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:194:     for(int i = 0; i < narr; i ++){
#NO_APP
	cmp	w22, 0	// narr,
	ble	.L38		//,
	add	x0, x19, 15	// tmp146, a,
	sub	w1, w22, #1	// _50, narr,
	sub	x0, x0, x20	// tmp147, tmp146, b
	cmp	x0, 30	// tmp147,
	ccmp	w1, 3, 0, hi	// _50,,,
	bls	.L39		//,
	dup	v2.2d, v8.d[0]	// vect_cst__91, s
	lsr	w3, w22, 1	// bnd.103, narr,
	mov	x0, x19	// ivtmp.125, a
	mov	x1, x20	// ivtmp.128, b
	add	x3, x19, x3, uxtw 4	// _112, a, bnd.103,
	.p2align 4,,15
.L40:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:195:         a[i] = a[i] + b[i] * s;
	ldr	q0, [x1], 16	// vect__6.111, MEM[base: _108, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:195:         a[i] = a[i] + b[i] * s;
	ldr	q1, [x0]	// vect__4.108, MEM[base: _106, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:195:         a[i] = a[i] + b[i] * s;
	fmla	v1.2d, v0.2d, v2.2d	// vect__8.113, vect__6.111, vect_cst__91
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:195:         a[i] = a[i] + b[i] * s;
	str	q1, [x0], 16	// vect__8.113, MEM[base: _106, offset: 0B]
	cmp	x0, x3	// ivtmp.125, _112
	bne	.L40		//,
	and	w2, w22, -2	//, narr,
	tbz	x22, 0, .L38	// narr,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:195:         a[i] = a[i] + b[i] * s;
	lsl	x2, x2, 3	// _71, niters_vector_mult_vf.104,
	add	x0, x19, x2	// _72, a, _71
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:195:         a[i] = a[i] + b[i] * s;
	ldr	d0, [x20, x2]	// *_74, *_74
	ldr	d1, [x0]	// *_72, *_72
	fmadd	d0, d8, d0, d1	// _77, s, *_74, *_72
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:195:         a[i] = a[i] + b[i] * s;
	str	d0, [x0]	// _77, *_72
.L38:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:197:     CYCLE_EN;
#APP
// 197 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSHST
	mrs x30, pmccntr_el0
	sub x30, x30, x29
	str x30, [x0]	// cy
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:198:     clock_gettime(CLOCK_MONOTONIC, &ts2);
#NO_APP
	mov	w0, 1	//,
	add	x1, sp, 80	// tmp192,,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:200: }
	ldp	x19, x20, [sp, 16]	//,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:199:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	ldp	d0, d1, [sp, 80]	// ts2.tv_sec, ts2.tv_nsec, ts2.tv_sec
	mov	x0, 225833675390976	// tmp186,
	movk	x0, 0x41cd, lsl 48	// tmp186,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:199:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	ldp	d4, d3, [sp, 64]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:200: }
	ldr	d8, [sp, 48]	//,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:199:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmov	d2, x0	// tmp172, tmp186
	scvtf	d1, d1	// tmp170, ts2.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:199:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d0, d0	// tmp168, ts2.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:199:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d4, d4	// tmp173, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:199:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d3, d3	// tmp176, ts1.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:199:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmadd	d0, d0, d2, d1	// _14, tmp168, tmp172, tmp170
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:199:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmsub	d0, d4, d2, d0	// _18, tmp173, tmp172, _14
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:199:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fsub	d0, d0, d3	// tmp178, _18, tmp176
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:199:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fcvtzu	d0, d0	// tmp179, tmp178
	str	d0, [x21]	// tmp179, *ns_33(D)
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:200: }
	ldp	x21, x22, [sp, 32]	//,,
	ldp	x29, x30, [sp], 96	//,,,
	.cfi_remember_state
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 72
	.cfi_restore 21
	.cfi_restore 22
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret	
	.p2align 3,,7
.L39:
	.cfi_restore_state
	add	x2, x19, 8	// tmp162, ivtmp.119,
	mov	x0, x19	// ivtmp.119, a
	add	x1, x2, x1, uxtw 3	// _99, tmp162, _50,
	.p2align 4,,15
.L42:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:195:         a[i] = a[i] + b[i] * s;
	ldr	d1, [x20], 8	// MEM[base: _8, offset: 0B], MEM[base: _8, offset: 0B]
	ldr	d0, [x0]	// MEM[base: _6, offset: 0B], MEM[base: _6, offset: 0B]
	fmadd	d0, d8, d1, d0	// _63, s, MEM[base: _8, offset: 0B], MEM[base: _6, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:195:         a[i] = a[i] + b[i] * s;
	str	d0, [x0], 8	// _63, MEM[base: _6, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:194:     for(int i = 0; i < narr; i ++){
	cmp	x0, x1	// ivtmp.119, _99
	bne	.L42		//,
	b	.L38		//
	.cfi_endproc
.LFE26:
	.size	run_daxpy, .-run_daxpy
	.align	2
	.p2align 4,,15
	.global	run_striad
	.type	run_striad, %function
run_striad:
.LFB27:
	.cfi_startproc
	stp	x29, x30, [sp, -96]!	//,,,
	.cfi_def_cfa_offset 96
	.cfi_offset 29, -96
	.cfi_offset 30, -88
	mov	x29, sp	//,
	stp	x19, x20, [sp, 16]	//,,
	.cfi_offset 19, -80
	.cfi_offset 20, -72
	mov	x19, x0	// tmp203, a
	mov	x20, x1	// b, tmp204
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:210:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	mov	w0, 1	//,
	add	x1, sp, 64	// tmp214,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:207: run_striad(TYPE *a, TYPE *b, TYPE *c, TYPE *d, int narr, uint64_t *ns, uint64_t *cy){
	stp	x21, x22, [sp, 32]	//,,
	.cfi_offset 21, -64
	.cfi_offset 22, -56
	mov	x21, x2	// c, tmp205
	mov	x22, x3	// d, tmp206
	stp	x23, x24, [sp, 48]	//,,
	.cfi_offset 23, -48
	.cfi_offset 24, -40
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:207: run_striad(TYPE *a, TYPE *b, TYPE *c, TYPE *d, int narr, uint64_t *ns, uint64_t *cy){
	mov	w24, w4	// narr, tmp207
	mov	x23, x5	// ns, tmp208
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:210:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:211:     clock_gettime(CLOCK_MONOTONIC, &ts2);
	add	x1, sp, 80	// tmp215,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:212:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	add	x1, sp, 64	// tmp216,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:213:     CYCLE_ST;
#APP
// 213 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSH
	mrs x29, pmccntr_el0
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:214:     for(int i = 0; i < narr; i ++){
#NO_APP
	cmp	w24, 0	// narr,
	ble	.L48		//,
	add	x2, x21, 15	// tmp155, c,
	add	x1, x22, 15	// tmp157, d,
	add	x0, x20, 15	// tmp165, b,
	sub	x2, x2, x19	// tmp156, tmp155, a
	sub	x1, x1, x19	// tmp158, tmp157, a
	sub	x0, x0, x19	// tmp166, tmp165, a
	cmp	x2, 30	// tmp156,
	sub	w2, w24, #1	// _57, narr,
	ccmp	x1, 30, 0, hi	// tmp158,,,
	cset	w1, hi	// tmp164,
	cmp	x0, 30	// tmp166,
	ccmp	w2, 4, 0, hi	// _57,,,
	cset	w0, hi	// tmp170,
	tst	w1, w0	// tmp164, tmp170
	beq	.L49		//,
	lsr	w0, w24, 1	// bnd.135, narr,
	mov	x6, 0	// ivtmp.163,
	lsl	x0, x0, 4	// _10, bnd.135,
	.p2align 4,,15
.L50:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:215:        a[i] = b[i] + c[i] * d[i];
	ldr	q2, [x21, x6]	// vect__6.143, MEM[base: c_41(D), index: ivtmp.163_7, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:215:        a[i] = b[i] + c[i] * d[i];
	ldr	q1, [x22, x6]	// vect__8.146, MEM[base: d_42(D), index: ivtmp.163_7, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:215:        a[i] = b[i] + c[i] * d[i];
	ldr	q0, [x20, x6]	// vect__4.140, MEM[base: b_40(D), index: ivtmp.163_7, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:215:        a[i] = b[i] + c[i] * d[i];
	fmla	v0.2d, v2.2d, v1.2d	// vect__11.148, vect__6.143, vect__8.146
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:215:        a[i] = b[i] + c[i] * d[i];
	str	q0, [x19, x6]	// vect__11.148, MEM[base: a_43(D), index: ivtmp.163_7, offset: 0B]
	add	x6, x6, 16	// ivtmp.163, ivtmp.163,
	cmp	x6, x0	// ivtmp.163, _10
	bne	.L50		//,
	and	w4, w24, -2	//, narr,
	tbz	x24, 0, .L48	// narr,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:215:        a[i] = b[i] + c[i] * d[i];
	lsl	x4, x4, 3	// _88, niters_vector_mult_vf.136,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:215:        a[i] = b[i] + c[i] * d[i];
	ldr	d2, [x21, x4]	// *_91, *_91
	ldr	d1, [x22, x4]	// *_93, *_93
	ldr	d0, [x20, x4]	// *_89, *_89
	fmadd	d0, d2, d1, d0	// _97, *_91, *_93, *_89
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:215:        a[i] = b[i] + c[i] * d[i];
	str	d0, [x19, x4]	// _97, *_96
.L48:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:217:     CYCLE_EN;
#APP
// 217 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSHST
	mrs x30, pmccntr_el0
	sub x30, x30, x29
	str x30, [x0]	// cy
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:218:     clock_gettime(CLOCK_MONOTONIC, &ts2);
#NO_APP
	mov	w0, 1	//,
	add	x1, sp, 80	// tmp217,,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:220: }
	ldp	x19, x20, [sp, 16]	//,,
	ldp	x21, x22, [sp, 32]	//,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:219:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	mov	x0, 225833675390976	// tmp209,
	ldp	d0, d1, [sp, 80]	// ts2.tv_sec, ts2.tv_nsec, ts2.tv_sec
	movk	x0, 0x41cd, lsl 48	// tmp209,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:219:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	ldp	d4, d3, [sp, 64]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:219:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmov	d2, x0	// tmp194, tmp209
	scvtf	d1, d1	// tmp192, ts2.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:219:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d0, d0	// tmp190, ts2.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:219:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d4, d4	// tmp195, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:219:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d3, d3	// tmp198, ts1.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:219:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmadd	d0, d0, d2, d1	// _17, tmp190, tmp194, tmp192
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:219:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmsub	d0, d4, d2, d0	// _21, tmp195, tmp194, _17
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:219:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fsub	d0, d0, d3	// tmp200, _21, tmp198
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:219:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fcvtzu	d0, d0	// tmp201, tmp200
	str	d0, [x23]	// tmp201, *ns_36(D)
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:220: }
	ldp	x23, x24, [sp, 48]	//,,
	ldp	x29, x30, [sp], 96	//,,,
	.cfi_remember_state
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 23
	.cfi_restore 24
	.cfi_restore 21
	.cfi_restore 22
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret	
	.p2align 3,,7
.L49:
	.cfi_restore_state
	ubfiz	x1, x2, 3, 32	// tmp184, _57,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:214:     for(int i = 0; i < narr; i ++){
	mov	x0, 0	// ivtmp.159,
	add	x1, x1, 8	// _6, tmp184,
	.p2align 4,,15
.L52:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:215:        a[i] = b[i] + c[i] * d[i];
	ldr	d2, [x21, x0]	// MEM[base: c_41(D), index: ivtmp.159_48, offset: 0B], MEM[base: c_41(D), index: ivtmp.159_48, offset: 0B]
	ldr	d1, [x22, x0]	// MEM[base: d_42(D), index: ivtmp.159_48, offset: 0B], MEM[base: d_42(D), index: ivtmp.159_48, offset: 0B]
	ldr	d0, [x20, x0]	// MEM[base: b_40(D), index: ivtmp.159_48, offset: 0B], MEM[base: b_40(D), index: ivtmp.159_48, offset: 0B]
	fmadd	d0, d2, d1, d0	// _80, MEM[base: c_41(D), index: ivtmp.159_48, offset: 0B], MEM[base: d_42(D), index: ivtmp.159_48, offset: 0B], MEM[base: b_40(D), index: ivtmp.159_48, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:215:        a[i] = b[i] + c[i] * d[i];
	str	d0, [x19, x0]	// _80, MEM[base: a_43(D), index: ivtmp.159_48, offset: 0B]
	add	x0, x0, 8	// ivtmp.159, ivtmp.159,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:214:     for(int i = 0; i < narr; i ++){
	cmp	x0, x1	// ivtmp.159, _6
	bne	.L52		//,
	b	.L48		//
	.cfi_endproc
.LFE27:
	.size	run_striad, .-run_striad
	.align	2
	.p2align 4,,15
	.global	run_sdaxpy
	.type	run_sdaxpy, %function
run_sdaxpy:
.LFB28:
	.cfi_startproc
	stp	x29, x30, [sp, -96]!	//,,,
	.cfi_def_cfa_offset 96
	.cfi_offset 29, -96
	.cfi_offset 30, -88
	mov	x29, sp	//,
	stp	x19, x20, [sp, 16]	//,,
	.cfi_offset 19, -80
	.cfi_offset 20, -72
	mov	x19, x0	// tmp183, a
	mov	x20, x1	// b, tmp184
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:230:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	mov	w0, 1	//,
	add	x1, sp, 64	// tmp191,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:227: run_sdaxpy(TYPE *a, TYPE *b, TYPE *c, int narr, uint64_t *ns, uint64_t *cy){
	stp	x21, x22, [sp, 32]	//,,
	.cfi_offset 21, -64
	.cfi_offset 22, -56
	mov	x21, x2	// c, tmp185
	mov	x22, x4	// ns, tmp187
	str	x23, [sp, 48]	//,
	.cfi_offset 23, -48
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:227: run_sdaxpy(TYPE *a, TYPE *b, TYPE *c, int narr, uint64_t *ns, uint64_t *cy){
	mov	w23, w3	// narr, tmp186
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:230:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:231:     clock_gettime(CLOCK_MONOTONIC, &ts2);
	add	x1, sp, 80	// tmp192,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:232:     clock_gettime(CLOCK_MONOTONIC, &ts1);
	add	x1, sp, 64	// tmp193,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:233:     CYCLE_ST;
#APP
// 233 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSH
	mrs x29, pmccntr_el0
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:234:     for(int i = 0; i < narr; i ++){
#NO_APP
	cmp	w23, 0	// narr,
	ble	.L58		//,
	add	x0, x19, 15	// _53, a,
	sub	w1, w23, #1	// _55, narr,
	sub	x2, x0, x21	// tmp147, _53, c
	sub	x0, x0, x20	// tmp148, _53, b
	cmp	x2, 30	// tmp147,
	ccmp	x0, 30, 0, hi	// tmp148,,,
	ccmp	w1, 3, 0, hi	// _55,,,
	bls	.L59		//,
	lsr	w1, w23, 1	// bnd.176, narr,
	mov	x0, 0	// ivtmp.203,
	lsl	x1, x1, 4	// _10, bnd.176,
	.p2align 4,,15
.L60:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:235:         a[i] = a[i] + b[i] * c[i];
	ldr	q2, [x20, x0]	// vect__6.184, MEM[base: b_40(D), index: ivtmp.203_7, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:235:         a[i] = a[i] + b[i] * c[i];
	ldr	q1, [x21, x0]	// vect__8.187, MEM[base: c_41(D), index: ivtmp.203_7, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:235:         a[i] = a[i] + b[i] * c[i];
	ldr	q0, [x19, x0]	// vect__4.181, MEM[base: a_39(D), index: ivtmp.203_7, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:235:         a[i] = a[i] + b[i] * c[i];
	fmla	v0.2d, v2.2d, v1.2d	// vect__10.189, vect__6.184, vect__8.187
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:235:         a[i] = a[i] + b[i] * c[i];
	str	q0, [x19, x0]	// vect__10.189, MEM[base: a_39(D), index: ivtmp.203_7, offset: 0B]
	add	x0, x0, 16	// ivtmp.203, ivtmp.203,
	cmp	x0, x1	// ivtmp.203, _10
	bne	.L60		//,
	and	w0, w23, -2	//, narr,
	tbz	x23, 0, .L58	// narr,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:235:         a[i] = a[i] + b[i] * c[i];
	lsl	x0, x0, 3	// _80, niters_vector_mult_vf.177,
	add	x19, x19, x0	// _81, a, _80
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:235:         a[i] = a[i] + b[i] * c[i];
	ldr	d2, [x20, x0]	// *_83, *_83
	ldr	d1, [x21, x0]	// *_85, *_85
	ldr	d0, [x19]	// *_81, *_81
	fmadd	d0, d2, d1, d0	// _88, *_83, *_85, *_81
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:235:         a[i] = a[i] + b[i] * c[i];
	str	d0, [x19]	// _88, *_81
.L58:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:237:     CYCLE_EN;
#APP
// 237 "/chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c" 1
	DMB NSHST
	mrs x30, pmccntr_el0
	sub x30, x30, x29
	str x30, [x0]	// cy
// 0 "" 2
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:238:     clock_gettime(CLOCK_MONOTONIC, &ts2);
#NO_APP
	mov	w0, 1	//,
	add	x1, sp, 80	// tmp194,,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:240: }
	ldp	x19, x20, [sp, 16]	//,,
	ldr	x23, [sp, 48]	//,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:239:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	mov	x0, 225833675390976	// tmp188,
	ldp	d0, d1, [sp, 80]	// ts2.tv_sec, ts2.tv_nsec, ts2.tv_sec
	movk	x0, 0x41cd, lsl 48	// tmp188,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:239:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	ldp	d4, d3, [sp, 64]	// ts1.tv_sec, ts1.tv_nsec, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:239:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmov	d2, x0	// tmp174, tmp188
	scvtf	d1, d1	// tmp172, ts2.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:239:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d0, d0	// tmp170, ts2.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:239:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d4, d4	// tmp175, ts1.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:239:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	scvtf	d3, d3	// tmp178, ts1.tv_nsec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:239:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmadd	d0, d0, d2, d1	// _16, tmp170, tmp174, tmp172
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:239:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fmsub	d0, d4, d2, d0	// _20, tmp175, tmp174, _16
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:239:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fsub	d0, d0, d3	// tmp180, _20, tmp178
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:239:     *ns = ts2.tv_sec * 1e9 + ts2.tv_nsec - ts1.tv_sec * 1e9 - ts1.tv_nsec;
	fcvtzu	d0, d0	// tmp181, tmp180
	str	d0, [x22]	// tmp181, *ns_35(D)
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:240: }
	ldp	x21, x22, [sp, 32]	//,,
	ldp	x29, x30, [sp], 96	//,,,
	.cfi_remember_state
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 23
	.cfi_restore 21
	.cfi_restore 22
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret	
	.p2align 3,,7
.L59:
	.cfi_restore_state
	ubfiz	x1, x1, 3, 32	// tmp164, _55,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:234:     for(int i = 0; i < narr; i ++){
	mov	x0, 0	// ivtmp.199,
	add	x1, x1, 8	// _6, tmp164,
	.p2align 4,,15
.L62:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:235:         a[i] = a[i] + b[i] * c[i];
	ldr	d2, [x20, x0]	// MEM[base: b_40(D), index: ivtmp.199_46, offset: 0B], MEM[base: b_40(D), index: ivtmp.199_46, offset: 0B]
	ldr	d1, [x21, x0]	// MEM[base: c_41(D), index: ivtmp.199_46, offset: 0B], MEM[base: c_41(D), index: ivtmp.199_46, offset: 0B]
	ldr	d0, [x19, x0]	// MEM[base: a_39(D), index: ivtmp.199_46, offset: 0B], MEM[base: a_39(D), index: ivtmp.199_46, offset: 0B]
	fmadd	d0, d2, d1, d0	// _72, MEM[base: b_40(D), index: ivtmp.199_46, offset: 0B], MEM[base: c_41(D), index: ivtmp.199_46, offset: 0B], MEM[base: a_39(D), index: ivtmp.199_46, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:235:         a[i] = a[i] + b[i] * c[i];
	str	d0, [x19, x0]	// _72, MEM[base: a_39(D), index: ivtmp.199_46, offset: 0B]
	add	x0, x0, 8	// ivtmp.199, ivtmp.199,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:234:     for(int i = 0; i < narr; i ++){
	cmp	x0, x1	// ivtmp.199, _6
	bne	.L62		//,
	b	.L58		//
	.cfi_endproc
.LFE28:
	.size	run_sdaxpy, .-run_sdaxpy
	.section	.rodata.str1.8
	.align	3
.LC2:
	.string	"aaa\n"
	.text
	.align	2
	.p2align 4,,15
	.global	run_g1_kernel
	.type	run_g1_kernel, %function
run_g1_kernel:
.LFB29:
	.cfi_startproc
	stp	x29, x30, [sp, -160]!	//,,,
	.cfi_def_cfa_offset 160
	.cfi_offset 29, -160
	.cfi_offset 30, -152
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:253:     double s = 0.5;
	fmov	d0, 5.0e-1	// tmp197,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:249: run_g1_kernel(int ntest, uint64_t kib, uint64_t **res_ns, uint64_t **res_cy) {
	mov	x29, sp	//,
	stp	x25, x26, [sp, 64]	//,,
	.cfi_offset 25, -96
	.cfi_offset 26, -88
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:256:     nbyte = kib * 1024;
	lsl	w26, w1, 10	// nbyte, tmp309,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:257:     nsize = nbyte / 8; // double
	cmp	w26, 0	// nbyte,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:249: run_g1_kernel(int ntest, uint64_t kib, uint64_t **res_ns, uint64_t **res_cy) {
	stp	x19, x20, [sp, 16]	//,,
	.cfi_offset 19, -144
	.cfi_offset 20, -136
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:257:     nsize = nbyte / 8; // double
	add	w20, w26, 7	// tmp199, nbyte,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:249: run_g1_kernel(int ntest, uint64_t kib, uint64_t **res_ns, uint64_t **res_cy) {
	mov	x19, x2	// res_ns, tmp310
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:257:     nsize = nbyte / 8; // double
	csel	w20, w20, w26, lt	// nbyte, tmp199, nbyte,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:249: run_g1_kernel(int ntest, uint64_t kib, uint64_t **res_ns, uint64_t **res_cy) {
	stp	x21, x22, [sp, 32]	//,,
	.cfi_offset 21, -128
	.cfi_offset 22, -120
	mov	x21, x3	// res_cy, tmp311
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:257:     nsize = nbyte / 8; // double
	asr	w20, w20, 3	// nsize, nbyte,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:249: run_g1_kernel(int ntest, uint64_t kib, uint64_t **res_ns, uint64_t **res_cy) {
	str	w0, [sp, 124]	// tmp308, %sfp
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:259:     a = (TYPE *)malloc(sizeof(TYPE) * nsize);
	sbfiz	x25, x20, 3, 32	// _4, nsize,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:253:     double s = 0.5;
	str	d0, [sp, 136]	// tmp197, s
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:259:     a = (TYPE *)malloc(sizeof(TYPE) * nsize);
	mov	x0, x25	//, _4
	bl	malloc		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:260:     if(a == NULL) {
	cbz	x0, .L70	// a,
	stp	x23, x24, [sp, 48]	//,,
	.cfi_offset 24, -104
	.cfi_offset 23, -112
	mov	x23, x0	// a, tmp312
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:263:     b = (TYPE *)malloc(sizeof(TYPE) * nsize);
	mov	x0, x25	//, _4
	bl	malloc		//
	mov	x22, x0	// b, tmp313
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:264:     if(b == NULL) {
	cbz	x0, .L92	// b,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:267:     c = (TYPE *)malloc(sizeof(TYPE) * nsize);
	mov	x0, x25	//, _4
	bl	malloc		//
	mov	x24, x0	// c, tmp314
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:268:     if(c == NULL) {
	cbz	x0, .L92	// c,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:271:     d = (TYPE *)malloc(sizeof(TYPE) * nsize);
	mov	x0, x25	//, _4
	bl	malloc		//
	mov	x25, x0	// d, tmp315
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:272:     if(d == NULL) {
	cbz	x0, .L92	// d,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:275:     clock_gettime(CLOCK_MONOTONIC, &ts);
	add	x1, sp, 144	// tmp318,,
	mov	w0, 1	//,
	stp	x27, x28, [sp, 80]	//,,
	.cfi_offset 28, -72
	.cfi_offset 27, -80
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:276:     t0 = ts.tv_sec * 1e9 + ts.tv_nsec;
	ldp	d2, d0, [sp, 144]	// ts.tv_sec, ts.tv_nsec, ts.tv_sec
	mov	x0, 225833675390976	// tmp317,
	movk	x0, 0x41cd, lsl 48	// tmp317,,
	fmov	d1, x0	// tmp211, tmp317
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:276:     t0 = ts.tv_sec * 1e9 + ts.tv_nsec;
	scvtf	d2, d2	// tmp207, ts.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:276:     t0 = ts.tv_sec * 1e9 + ts.tv_nsec;
	scvtf	d0, d0	// tmp209, ts.tv_nsec
	fmadd	d0, d2, d1, d0	// _10, tmp207, tmp211, tmp209
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:276:     t0 = ts.tv_sec * 1e9 + ts.tv_nsec;
	fcvtzu	x1, d0	// t0, _10
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:277:     t1 = t0 + 1e9;
	ucvtf	d0, x1	// tmp212, t0
	fadd	d1, d0, d1	// tmp213, tmp212, tmp211
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:277:     t1 = t0 + 1e9;
	fcvtzu	x27, d1	// t1, tmp213
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:278:     for(i = 0; i < nsize; i ++) {
	cmp	w26, 7	// nbyte,
	ble	.L71		//,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:279:         a[i] = 1.0;
	fmov	d3, 1.0e+0	// tmp215,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:280:         b[i] = 2.0;
	fmov	d2, 2.0e+0	// tmp216,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:278:     for(i = 0; i < nsize; i ++) {
	mov	x0, 0	// ivtmp.223,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:281:         c[i] = 3.0;
	fmov	d1, 3.0e+0	// tmp217,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:282:         d[i] = 4.0;
	fmov	d0, 4.0e+0	// tmp218,
	.p2align 4,,15
.L72:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:279:         a[i] = 1.0;
	str	d3, [x23, x0, lsl 3]	// tmp215, MEM[base: a_111, index: ivtmp.223_187, step: 8, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:280:         b[i] = 2.0;
	str	d2, [x22, x0, lsl 3]	// tmp216, MEM[base: b_113, index: ivtmp.223_187, step: 8, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:281:         c[i] = 3.0;
	str	d1, [x24, x0, lsl 3]	// tmp217, MEM[base: c_115, index: ivtmp.223_187, step: 8, offset: 0B]
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:282:         d[i] = 4.0;
	str	d0, [x25, x0, lsl 3]	// tmp218, MEM[base: d_117, index: ivtmp.223_187, step: 8, offset: 0B]
	add	x0, x0, 1	// ivtmp.223, ivtmp.223,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:278:     for(i = 0; i < nsize; i ++) {
	cmp	w20, w0	// nsize, ivtmp.223
	bgt	.L72		//,
.L71:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:284:     while(t0 < t1) {
	cmp	x1, x27	// t0, t1
	bcs	.L73		//,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:295:         t0 = ts.tv_sec * 1e9 + ts.tv_nsec; 
	mov	x0, 225833675390976	// tmp316,
	str	d8, [sp, 96]	//,
	.cfi_offset 72, -64
	adrp	x28, .LC2	// tmp305,
	add	x26, sp, 136	// tmp303,,
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:295:         t0 = ts.tv_sec * 1e9 + ts.tv_nsec; 
	movk	x0, 0x41cd, lsl 48	// tmp316,,
	add	x28, x28, :lo12:.LC2	// tmp302, tmp305,
	fmov	d8, x0	// tmp261, tmp316
	.p2align 4,,15
.L74:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:285:         run_init(a, s,  nsize,  &res_ns[0][0],  &res_cy[0][0]);
	ldr	x2, [x19]	//, *res_ns_126(D)
	ldr	x3, [x21]	//, *res_cy_127(D)
	ldr	d0, [sp, 136]	//, s
	mov	w1, w20	//, nsize
	mov	x0, x23	//, a
	bl	run_init		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:286:         run_sum(a,  &s, nsize,  &res_ns[0][1],  &res_cy[0][1]);
	ldr	x3, [x19]	// *res_ns_126(D), *res_ns_126(D)
	ldr	x4, [x21]	// *res_cy_127(D), *res_cy_127(D)
	mov	w2, w20	//, nsize
	mov	x1, x26	//, tmp303
	mov	x0, x23	//, a
	add	x3, x3, 8	//, *res_ns_126(D),
	add	x4, x4, 8	//, *res_cy_127(D),
	bl	run_sum		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:287:         run_copy(a, b,  nsize,  &res_ns[0][2],  &res_cy[0][2]);
	ldr	x3, [x19]	// *res_ns_126(D), *res_ns_126(D)
	ldr	x4, [x21]	// *res_cy_127(D), *res_cy_127(D)
	mov	w2, w20	//, nsize
	mov	x1, x22	//, b
	mov	x0, x23	//, a
	add	x4, x4, 16	//, *res_cy_127(D),
	add	x3, x3, 16	//, *res_ns_126(D),
	bl	run_copy		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:288:         printf("aaa\n");
	mov	x0, x28	//, tmp302
	bl	printf		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:289:         run_update(b,   s,  nsize,  &res_ns[0][3],  &res_cy[0][3]);
	ldr	x2, [x19]	// *res_ns_126(D), *res_ns_126(D)
	ldr	x3, [x21]	// *res_cy_127(D), *res_cy_127(D)
	ldr	d0, [sp, 136]	//, s
	mov	w1, w20	//, nsize
	mov	x0, x22	//, b
	add	x2, x2, 24	//, *res_ns_126(D),
	add	x3, x3, 24	//, *res_cy_127(D),
	bl	run_update		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:290:         run_triad(a, b, c, s, nsize, &res_ns[0][4], &res_cy[0][4]);
	ldr	x4, [x19]	// *res_ns_126(D), *res_ns_126(D)
	ldr	x5, [x21]	// *res_cy_127(D), *res_cy_127(D)
	ldr	d0, [sp, 136]	//, s
	mov	w3, w20	//, nsize
	mov	x2, x24	//, c
	mov	x1, x22	//, b
	mov	x0, x23	//, a
	add	x5, x5, 32	//, *res_cy_127(D),
	add	x4, x4, 32	//, *res_ns_126(D),
	bl	run_triad		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:291:         run_daxpy(a, b, s, nsize, &res_ns[0][5], &res_cy[0][5]);
	ldr	x3, [x19]	// *res_ns_126(D), *res_ns_126(D)
	ldr	x4, [x21]	// *res_cy_127(D), *res_cy_127(D)
	ldr	d0, [sp, 136]	//, s
	mov	w2, w20	//, nsize
	mov	x1, x22	//, b
	mov	x0, x23	//, a
	add	x3, x3, 40	//, *res_ns_126(D),
	add	x4, x4, 40	//, *res_cy_127(D),
	bl	run_daxpy		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:292:         run_striad(a, b, c, d, nsize, &res_ns[0][6], &res_cy[0][6]);
	ldr	x5, [x19]	// *res_ns_126(D), *res_ns_126(D)
	ldr	x6, [x21]	// *res_cy_127(D), *res_cy_127(D)
	mov	w4, w20	//, nsize
	mov	x3, x25	//, d
	mov	x2, x24	//, c
	mov	x1, x22	//, b
	mov	x0, x23	//, a
	add	x6, x6, 48	//, *res_cy_127(D),
	add	x5, x5, 48	//, *res_ns_126(D),
	bl	run_striad		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:293:         run_sdaxpy(d, b, c, nsize, &res_ns[0][7], &res_cy[0][7]);
	ldr	x4, [x19]	// *res_ns_126(D), *res_ns_126(D)
	ldr	x5, [x21]	// *res_cy_127(D), *res_cy_127(D)
	mov	w3, w20	//, nsize
	mov	x2, x24	//, c
	mov	x1, x22	//, b
	mov	x0, x25	//, d
	add	x4, x4, 56	//, *res_ns_126(D),
	add	x5, x5, 56	//, *res_cy_127(D),
	bl	run_sdaxpy		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:294:         clock_gettime(CLOCK_MONOTONIC, &ts);
	add	x1, sp, 144	// tmp319,,
	mov	w0, 1	//,
	bl	clock_gettime		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:295:         t0 = ts.tv_sec * 1e9 + ts.tv_nsec; 
	ldp	d1, d0, [sp, 144]	// ts.tv_sec, ts.tv_nsec, ts.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:295:         t0 = ts.tv_sec * 1e9 + ts.tv_nsec; 
	scvtf	d1, d1	// tmp257, ts.tv_sec
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:295:         t0 = ts.tv_sec * 1e9 + ts.tv_nsec; 
	scvtf	d0, d0	// tmp259, ts.tv_nsec
	fmadd	d0, d1, d8, d0	// _58, tmp257, tmp261, tmp259
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:295:         t0 = ts.tv_sec * 1e9 + ts.tv_nsec; 
	fcvtzu	x0, d0	// t0, _58
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:284:     while(t0 < t1) {
	cmp	x27, x0	// t1, t0
	bhi	.L74		//,
	ldr	d8, [sp, 96]	//,
	.cfi_restore 72
.L73:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:298:     for(i = 0; i < ntest; i ++) {
	ldr	w0, [sp, 124]	//, %sfp
	cmp	w0, 0	// ntest,
	ble	.L75		//,
	sub	w27, w0, #1	// tmp263, ntest,
	add	x0, x19, 8	// tmp264, ivtmp.218,
	add	x26, sp, 136	// tmp303,,
	add	x27, x0, x27, uxtw 3	// _186, tmp264, tmp263,
	.p2align 4,,15
.L76:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:299:         run_init(a, s, nsize, &res_ns[i][0], &res_cy[i][0]);
	ldr	x2, [x19]	//, MEM[base: _158, offset: 0B]
	ldr	x3, [x21]	//, MEM[base: _174, offset: 0B]
	ldr	d0, [sp, 136]	//, s
	mov	w1, w20	//, nsize
	mov	x0, x23	//, a
	bl	run_init		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:300:         run_sum(a, &s, nsize, &res_ns[i][1], &res_cy[i][1]);
	ldr	x3, [x19]	// MEM[base: _158, offset: 0B], MEM[base: _158, offset: 0B]
	ldr	x4, [x21]	// MEM[base: _174, offset: 0B], MEM[base: _174, offset: 0B]
	mov	w2, w20	//, nsize
	mov	x1, x26	//, tmp303
	mov	x0, x23	//, a
	add	x3, x3, 8	//, MEM[base: _158, offset: 0B],
	add	x4, x4, 8	//, MEM[base: _174, offset: 0B],
	bl	run_sum		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:301:         run_copy(a, b, nsize, &res_ns[i][2], &res_cy[i][2]);
	ldr	x3, [x19]	// MEM[base: _158, offset: 0B], MEM[base: _158, offset: 0B]
	ldr	x4, [x21]	// MEM[base: _174, offset: 0B], MEM[base: _174, offset: 0B]
	mov	w2, w20	//, nsize
	mov	x1, x22	//, b
	mov	x0, x23	//, a
	add	x4, x4, 16	//, MEM[base: _174, offset: 0B],
	add	x3, x3, 16	//, MEM[base: _158, offset: 0B],
	bl	run_copy		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:302:         run_update(b, s, nsize, &res_ns[i][3], &res_cy[i][3]);
	ldr	x2, [x19]	// MEM[base: _158, offset: 0B], MEM[base: _158, offset: 0B]
	ldr	x3, [x21]	// MEM[base: _174, offset: 0B], MEM[base: _174, offset: 0B]
	ldr	d0, [sp, 136]	//, s
	mov	w1, w20	//, nsize
	mov	x0, x22	//, b
	add	x2, x2, 24	//, MEM[base: _158, offset: 0B],
	add	x3, x3, 24	//, MEM[base: _174, offset: 0B],
	bl	run_update		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:303:         run_triad(a, b, c, s, nsize, &res_ns[i][4], &res_cy[i][4]);
	ldr	x4, [x19]	// MEM[base: _158, offset: 0B], MEM[base: _158, offset: 0B]
	ldr	x5, [x21]	// MEM[base: _174, offset: 0B], MEM[base: _174, offset: 0B]
	ldr	d0, [sp, 136]	//, s
	mov	w3, w20	//, nsize
	mov	x2, x24	//, c
	mov	x1, x22	//, b
	mov	x0, x23	//, a
	add	x5, x5, 32	//, MEM[base: _174, offset: 0B],
	add	x4, x4, 32	//, MEM[base: _158, offset: 0B],
	bl	run_triad		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:304:         run_daxpy(a, b, s, nsize, &res_ns[i][5], &res_cy[i][5]);
	ldr	x3, [x19]	// MEM[base: _158, offset: 0B], MEM[base: _158, offset: 0B]
	ldr	x4, [x21]	// MEM[base: _174, offset: 0B], MEM[base: _174, offset: 0B]
	ldr	d0, [sp, 136]	//, s
	mov	w2, w20	//, nsize
	mov	x1, x22	//, b
	mov	x0, x23	//, a
	add	x3, x3, 40	//, MEM[base: _158, offset: 0B],
	add	x4, x4, 40	//, MEM[base: _174, offset: 0B],
	bl	run_daxpy		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:305:         run_striad(a, b, c, d, nsize, &res_ns[i][6], &res_cy[i][6]);
	ldr	x5, [x19]	// MEM[base: _158, offset: 0B], MEM[base: _158, offset: 0B]
	ldr	x6, [x21]	// MEM[base: _174, offset: 0B], MEM[base: _174, offset: 0B]
	mov	w4, w20	//, nsize
	mov	x3, x25	//, d
	mov	x2, x24	//, c
	mov	x1, x22	//, b
	mov	x0, x23	//, a
	add	x5, x5, 48	//, MEM[base: _158, offset: 0B],
	add	x6, x6, 48	//, MEM[base: _174, offset: 0B],
	bl	run_striad		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:306:         run_sdaxpy(d, b, c, nsize, &res_ns[i][7], &res_cy[i][7]);
	ldr	x4, [x19], 8	// MEM[base: _158, offset: 0B], MEM[base: _158, offset: 0B]
	ldr	x5, [x21], 8	// MEM[base: _174, offset: 0B], MEM[base: _174, offset: 0B]
	mov	w3, w20	//, nsize
	mov	x2, x24	//, c
	mov	x1, x22	//, b
	mov	x0, x25	//, d
	add	x5, x5, 56	//, MEM[base: _174, offset: 0B],
	add	x4, x4, 56	//, MEM[base: _158, offset: 0B],
	bl	run_sdaxpy		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:298:     for(i = 0; i < ntest; i ++) {
	cmp	x19, x27	// ivtmp.218, _186
	bne	.L76		//,
.L75:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:309:     free(a);
	mov	x0, x23	//, a
	bl	free		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:310:     free(b);
	mov	x0, x22	//, b
	bl	free		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:311:     free(c);
	mov	x0, x24	//, c
	bl	free		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:312:     free(d);
	mov	x0, x25	//, d
	bl	free		//
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:313:     return 0;
	ldp	x23, x24, [sp, 48]	//,,
	.cfi_restore 24
	.cfi_restore 23
	ldp	x27, x28, [sp, 80]	//,,
	.cfi_restore 28
	.cfi_restore 27
	mov	w0, 0	// <retval>,
.L67:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:314: }
	ldp	x19, x20, [sp, 16]	//,,
	ldp	x21, x22, [sp, 32]	//,,
	ldp	x25, x26, [sp, 64]	//,,
	ldp	x29, x30, [sp], 160	//,,,
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 25
	.cfi_restore 26
	.cfi_restore 21
	.cfi_restore 22
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret	
.L92:
	.cfi_def_cfa_offset 160
	.cfi_offset 19, -144
	.cfi_offset 20, -136
	.cfi_offset 21, -128
	.cfi_offset 22, -120
	.cfi_offset 23, -112
	.cfi_offset 24, -104
	.cfi_offset 25, -96
	.cfi_offset 26, -88
	.cfi_offset 29, -160
	.cfi_offset 30, -152
	ldp	x23, x24, [sp, 48]	//,,
	.cfi_restore 24
	.cfi_restore 23
.L70:
// /chpc/home/stf-keyliao-a/proj/TPBench/src/kernels/g1_kernel.c:261:         return MEM_FAIL;
	mov	w0, 4	// <retval>,
	b	.L67		//
	.cfi_endproc
.LFE29:
	.size	run_g1_kernel, .-run_g1_kernel
	.ident	"GCC: (GNU) 9.2.0"
	.section	.note.GNU-stack,"",@progbits
