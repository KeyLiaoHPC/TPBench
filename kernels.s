# mark_description "Intel(R) C Intel(R) 64 Compiler for applications running on Intel(R) 64, Version 19.0.5.281 Build 20190815";
# mark_description "-std=gnu11 -O3 -xHost -DNTIMES=1000 -DKIB_SIZE=32 -S -fasm-blocks -o kernels.s";
	.file "kernels.c"
	.text
..TXTST0:
.L_2__routine_start_sum_0:
# -- Begin  sum
	.text
# mark_begin;
       .align    16,0x90
	.globl sum
# --- sum(double *, double *, int, uint64_t *)
sum:
# parameter 1: %rdi
# parameter 2: %rsi
# parameter 3: %edx
# parameter 4: %rcx
..B1.1:                         # Preds ..B1.0
                                # Execution count [1.00e+00]
	.cfi_startproc
..___tag_value_sum.1:
..L2:
                                                          #93.46
        pushq     %rbx                                          #93.46
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
        movl      %edx, %r9d                                    #93.46
        movq      %rcx, %r8                                     #93.46
        movq      %rdi, %r10                                    #93.46
        rdtscp                                                  #99.0
        rdtsc                                                   #99.0
        cpuid                                                   #99.0
        rdtscp                                                  #99.0
        movq      %rdx, %r11                                    #99.0
        movq      %rax, %rdi                                    #99.0
        testl     %r9d, %r9d                                    #100.24
        jle       ..B1.18       # Prob 50%                      #100.24
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d
..B1.2:                         # Preds ..B1.1
                                # Execution count [0.00e+00]
        vmovsd    (%rsi), %xmm1                                 #101.10
        cmpl      $6, %r9d                                      #100.5
        jle       ..B1.12       # Prob 50%                      #100.5
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d xmm1
..B1.3:                         # Preds ..B1.2
                                # Execution count [0.00e+00]
        movslq    %r9d, %rax                                    #93.1
        movq      %rsi, %rdx                                    #101.15
        shlq      $3, %rax                                      #100.5
        subq      %r10, %rdx                                    #101.15
        cmpq      %rax, %rdx                                    #100.5
        jge       ..B1.5        # Prob 50%                      #100.5
                                # LOE rdx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d xmm1
..B1.4:                         # Preds ..B1.3
                                # Execution count [0.00e+00]
        negq      %rdx                                          #101.10
        cmpq      $8, %rdx                                      #100.5
        jl        ..B1.12       # Prob 50%                      #100.5
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d xmm1
..B1.5:                         # Preds ..B1.3 ..B1.4
                                # Execution count [5.00e-01]
        movl      %r9d, %ebx                                    #100.5
        xorl      %ecx, %ecx                                    #100.5
        movl      $1, %eax                                      #100.5
        xorl      %edx, %edx                                    #100.5
        shrl      $1, %ebx                                      #100.5
        je        ..B1.9        # Prob 9%                       #100.5
                                # LOE rdx rcx rbx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax r9d xmm1
..B1.6:                         # Preds ..B1.5
                                # Execution count [4.50e-01]
        vxorpd    %xmm0, %xmm0, %xmm0                           #101.10
                                # LOE rdx rcx rbx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d xmm0 xmm1
..B1.7:                         # Preds ..B1.7 ..B1.6
                                # Execution count [1.25e+00]
        incq      %rcx                                          #100.5
        vaddsd    (%rdx,%r10), %xmm1, %xmm1                     #101.10
        vaddsd    8(%rdx,%r10), %xmm0, %xmm0                    #101.10
        addq      $16, %rdx                                     #100.5
        cmpq      %rbx, %rcx                                    #100.5
        jb        ..B1.7        # Prob 63%                      #100.5
                                # LOE rdx rcx rbx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d xmm0 xmm1
..B1.8:                         # Preds ..B1.7
                                # Execution count [4.50e-01]
        vaddsd    %xmm0, %xmm1, %xmm1                           #101.10
        lea       1(%rcx,%rcx), %eax                            #101.10
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax r9d xmm1
..B1.9:                         # Preds ..B1.8 ..B1.5
                                # Execution count [5.00e-01]
        lea       -1(%rax), %edx                                #100.5
        cmpl      %r9d, %edx                                    #100.5
        jae       ..B1.11       # Prob 9%                       #100.5
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax xmm1
..B1.10:                        # Preds ..B1.9
                                # Execution count [4.50e-01]
        movslq    %eax, %rax                                    #100.5
        vaddsd    -8(%r10,%rax,8), %xmm1, %xmm1                 #101.10
                                # LOE rbp rsi rdi r8 r11 r12 r13 r14 r15 xmm1
..B1.11:                        # Preds ..B1.10 ..B1.9
                                # Execution count [4.50e-01]
        vmovsd    %xmm1, (%rsi)                                 #101.10
        jmp       ..B1.18       # Prob 100%                     #101.10
                                # LOE rbp rdi r8 r11 r12 r13 r14 r15
..B1.12:                        # Preds ..B1.2 ..B1.4
                                # Execution count [5.00e-01]
        movl      %r9d, %ebx                                    #100.5
        xorl      %ecx, %ecx                                    #100.5
        movl      $1, %eax                                      #100.5
        xorl      %edx, %edx                                    #100.5
        shrl      $1, %ebx                                      #100.5
        je        ..B1.16       # Prob 9%                       #100.5
                                # LOE rdx rcx rbx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax r9d xmm1
..B1.14:                        # Preds ..B1.12 ..B1.14
                                # Execution count [1.25e+00]
        incq      %rcx                                          #100.5
        vaddsd    (%rdx,%r10), %xmm1, %xmm0                     #101.10
        vmovsd    %xmm0, (%rsi)                                 #101.10
        vaddsd    8(%rdx,%r10), %xmm0, %xmm1                    #101.10
        addq      $16, %rdx                                     #100.5
        vmovsd    %xmm1, (%rsi)                                 #101.10
        cmpq      %rbx, %rcx                                    #100.5
        jb        ..B1.14       # Prob 63%                      #100.5
                                # LOE rdx rcx rbx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d xmm1
..B1.15:                        # Preds ..B1.14
                                # Execution count [4.50e-01]
        lea       1(%rcx,%rcx), %eax                            #101.10
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax r9d xmm1
..B1.16:                        # Preds ..B1.15 ..B1.12
                                # Execution count [5.00e-01]
        lea       -1(%rax), %edx                                #100.5
        cmpl      %r9d, %edx                                    #100.5
        jae       ..B1.18       # Prob 9%                       #100.5
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax xmm1
..B1.17:                        # Preds ..B1.16
                                # Execution count [4.50e-01]
        movslq    %eax, %rax                                    #100.5
        vaddsd    -8(%r10,%rax,8), %xmm1, %xmm0                 #101.10
        vmovsd    %xmm0, (%rsi)                                 #101.10
                                # LOE rbp rdi r8 r11 r12 r13 r14 r15
..B1.18:                        # Preds ..B1.1 ..B1.16 ..B1.17 ..B1.11
                                # Execution count [1.00e+00]
        rdtsc                                                   #103.0
        movq      %rdx, %r9                                     #103.0
        movq      %rax, %rsi                                    #103.0
        cpuid                                                   #103.0
        shlq      $32, %r9                                      #103.5
        shlq      $32, %r11                                     #103.5
        orq       %rsi, %r9                                     #103.5
        orq       %rdi, %r11                                    #103.5
        subq      %r11, %r9                                     #103.5
        movq      %r9, (%r8)                                    #103.5
	.cfi_restore 3
        popq      %rbx                                          #110.1
	.cfi_def_cfa_offset 8
        ret                                                     #110.1
        .align    16,0x90
                                # LOE
	.cfi_endproc
# mark_end;
	.type	sum,@function
	.size	sum,.-sum
..LNsum.0:
	.data
# -- End  sum
	.text
.L_2__routine_start_copy_1:
# -- Begin  copy
	.text
# mark_begin;
       .align    16,0x90
	.globl copy
# --- copy(double *, double *, int, uint64_t *)
copy:
# parameter 1: %rdi
# parameter 2: %rsi
# parameter 3: %edx
# parameter 4: %rcx
..B2.1:                         # Preds ..B2.0
                                # Execution count [1.00e+00]
	.cfi_startproc
..___tag_value_copy.8:
..L9:
                                                          #117.47
        pushq     %rbx                                          #117.47
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
        pushq     %r13                                          #117.47
	.cfi_def_cfa_offset 24
	.cfi_offset 13, -24
        pushq     %r15                                          #117.47
	.cfi_def_cfa_offset 32
	.cfi_offset 15, -32
        pushq     %rbp                                          #117.47
	.cfi_def_cfa_offset 40
	.cfi_offset 6, -40
        pushq     %rsi                                          #117.47
	.cfi_def_cfa_offset 48
        movl      %edx, %r8d                                    #117.47
        movq      %rcx, %r15                                    #117.47
        rdtscp                                                  #123.0
        rdtsc                                                   #123.0
        cpuid                                                   #123.0
        rdtscp                                                  #123.0
        movq      %rdx, %r13                                    #123.0
        movq      %rax, %rbp                                    #123.0
        testl     %r8d, %r8d                                    #124.24
        jle       ..B2.5        # Prob 50%                      #124.24
                                # LOE rbp rsi rdi r12 r13 r14 r15 r8d
..B2.2:                         # Preds ..B2.1
                                # Execution count [5.00e-03]
        cmpl      $12, %r8d                                     #124.5
        jle       ..B2.6        # Prob 10%                      #124.5
                                # LOE rbp rsi rdi r12 r13 r14 r15 r8d
..B2.3:                         # Preds ..B2.2
                                # Execution count [1.00e+00]
        movslq    %r8d, %rdx                                    #124.5
        movq      %rdi, %rax                                    #124.5
        shlq      $3, %rdx                                      #124.5
        subq      %rsi, %rax                                    #124.5
        xorl      %ebx, %ebx                                    #124.5
        cmpq      %rdx, %rax                                    #124.5
        setg      %bl                                           #124.5
        xorl      %ecx, %ecx                                    #124.5
        negq      %rax                                          #124.5
        cmpq      %rdx, %rax                                    #124.5
        setg      %cl                                           #124.5
        orl       %ecx, %ebx                                    #124.5
        je        ..B2.6        # Prob 10%                      #124.5
                                # LOE rdx rbp rsi rdi r12 r13 r14 r15 r8d
..B2.4:                         # Preds ..B2.3
                                # Execution count [1.00e+00]
        call      __intel_avx_rep_memcpy                        #124.5
                                # LOE rbp r12 r13 r14 r15
..B2.5:                         # Preds ..B2.1 ..B2.4 ..B2.10 ..B2.11
                                # Execution count [1.00e+00]
        rdtsc                                                   #127.0
        movq      %rdx, %rdi                                    #127.0
        movq      %rax, %rsi                                    #127.0
        cpuid                                                   #127.0
        shlq      $32, %rdi                                     #127.5
        shlq      $32, %r13                                     #127.5
        orq       %rsi, %rdi                                    #127.5
        orq       %rbp, %r13                                    #127.5
        subq      %r13, %rdi                                    #127.5
        movq      %rdi, (%r15)                                  #127.5
        popq      %rcx                                          #134.1
	.cfi_def_cfa_offset 40
	.cfi_restore 6
        popq      %rbp                                          #134.1
	.cfi_def_cfa_offset 32
	.cfi_restore 15
        popq      %r15                                          #134.1
	.cfi_def_cfa_offset 24
	.cfi_restore 13
        popq      %r13                                          #134.1
	.cfi_def_cfa_offset 16
	.cfi_restore 3
        popq      %rbx                                          #134.1
	.cfi_def_cfa_offset 8
        ret                                                     #134.1
	.cfi_def_cfa_offset 48
	.cfi_offset 3, -16
	.cfi_offset 6, -40
	.cfi_offset 13, -24
	.cfi_offset 15, -32
                                # LOE
..B2.6:                         # Preds ..B2.3 ..B2.2
                                # Execution count [1.11e+00]
        movl      %r8d, %eax                                    #124.5
        xorl      %ecx, %ecx                                    #124.5
        movl      $1, %ebx                                      #124.5
        xorl      %edx, %edx                                    #124.5
        shrl      $1, %eax                                      #124.5
        je        ..B2.10       # Prob 10%                      #124.5
                                # LOE rax rdx rcx rbp rsi rdi r12 r13 r14 r15 ebx r8d
..B2.8:                         # Preds ..B2.6 ..B2.8
                                # Execution count [2.78e+00]
        movq      (%rdx,%rsi), %rbx                             #125.16
        incq      %rcx                                          #124.5
        movq      %rbx, (%rdx,%rdi)                             #125.9
        movq      8(%rdx,%rsi), %r9                             #125.16
        movq      %r9, 8(%rdx,%rdi)                             #125.9
        addq      $16, %rdx                                     #124.5
        cmpq      %rax, %rcx                                    #124.5
        jb        ..B2.8        # Prob 64%                      #124.5
                                # LOE rax rdx rcx rbp rsi rdi r12 r13 r14 r15 r8d
..B2.9:                         # Preds ..B2.8
                                # Execution count [1.00e+00]
        lea       1(%rcx,%rcx), %ebx                            #125.9
                                # LOE rbp rsi rdi r12 r13 r14 r15 ebx r8d
..B2.10:                        # Preds ..B2.9 ..B2.6
                                # Execution count [1.11e+00]
        lea       -1(%rbx), %eax                                #124.5
        cmpl      %r8d, %eax                                    #124.5
        jae       ..B2.5        # Prob 10%                      #124.5
                                # LOE rbp rsi rdi r12 r13 r14 r15 ebx
..B2.11:                        # Preds ..B2.10
                                # Execution count [1.00e+00]
        movslq    %ebx, %rbx                                    #124.5
        movq      -8(%rsi,%rbx,8), %rsi                         #125.16
        movq      %rsi, -8(%rdi,%rbx,8)                         #125.9
        jmp       ..B2.5        # Prob 100%                     #125.9
        .align    16,0x90
                                # LOE rbp r12 r13 r14 r15
	.cfi_endproc
# mark_end;
	.type	copy,@function
	.size	copy,.-copy
..LNcopy.1:
	.data
# -- End  copy
	.text
.L_2__routine_start_update_2:
# -- Begin  update
	.text
# mark_begin;
       .align    16,0x90
	.globl update
# --- update(double *, double, int, uint64_t *)
update:
# parameter 1: %rdi
# parameter 2: %xmm0
# parameter 3: %esi
# parameter 4: %rdx
..B3.1:                         # Preds ..B3.0
                                # Execution count [1.00e+00]
	.cfi_startproc
..___tag_value_update.34:
..L35:
                                                         #141.48
        pushq     %rbx                                          #141.48
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
        movq      %rdx, %r8                                     #141.48
        movq      %rdi, %r10                                    #141.48
        rdtscp                                                  #147.0
        rdtsc                                                   #147.0
        cpuid                                                   #147.0
        rdtscp                                                  #147.0
        movq      %rdx, %r9                                     #147.0
        movq      %rax, %rdi                                    #147.0
        testl     %esi, %esi                                    #148.24
        jle       ..B3.22       # Prob 50%                      #148.24
                                # LOE rbp rdi r8 r9 r10 r12 r13 r14 r15 esi xmm0
..B3.2:                         # Preds ..B3.1
                                # Execution count [1.00e+00]
        movl      %esi, %edx                                    #148.5
        xorl      %ebx, %ebx                                    #148.5
        movl      $1, %eax                                      #148.5
        xorl      %ecx, %ecx                                    #148.5
        shrl      $3, %edx                                      #148.5
        je        ..B3.6        # Prob 9%                       #148.5
                                # LOE rdx rcx rbx rbp rdi r8 r9 r10 r12 r13 r14 r15 eax esi xmm0
..B3.4:                         # Preds ..B3.2 ..B3.4
                                # Execution count [6.25e-01]
        vmulsd    (%rcx,%r10), %xmm0, %xmm1                     #149.20
        incq      %rbx                                          #148.5
        vmulsd    8(%rcx,%r10), %xmm0, %xmm2                    #149.20
        vmulsd    16(%rcx,%r10), %xmm0, %xmm3                   #149.20
        vmulsd    24(%rcx,%r10), %xmm0, %xmm4                   #149.20
        vmulsd    32(%rcx,%r10), %xmm0, %xmm5                   #149.20
        vmulsd    40(%rcx,%r10), %xmm0, %xmm6                   #149.20
        vmulsd    48(%rcx,%r10), %xmm0, %xmm7                   #149.20
        vmulsd    56(%rcx,%r10), %xmm0, %xmm8                   #149.20
        vmovsd    %xmm1, (%rcx,%r10)                            #149.9
        vmovsd    %xmm2, 8(%rcx,%r10)                           #149.9
        vmovsd    %xmm3, 16(%rcx,%r10)                          #149.9
        vmovsd    %xmm4, 24(%rcx,%r10)                          #149.9
        vmovsd    %xmm5, 32(%rcx,%r10)                          #149.9
        vmovsd    %xmm6, 40(%rcx,%r10)                          #149.9
        vmovsd    %xmm7, 48(%rcx,%r10)                          #149.9
        vmovsd    %xmm8, 56(%rcx,%r10)                          #149.9
        addq      $64, %rcx                                     #148.5
        cmpq      %rdx, %rbx                                    #148.5
        jb        ..B3.4        # Prob 99%                      #148.5
                                # LOE rdx rcx rbx rbp rdi r8 r9 r10 r12 r13 r14 r15 esi xmm0
..B3.5:                         # Preds ..B3.4
                                # Execution count [9.00e-01]
        lea       1(,%rbx,8), %eax                              #149.9
                                # LOE rbp rdi r8 r9 r10 r12 r13 r14 r15 eax esi xmm0
..B3.6:                         # Preds ..B3.5 ..B3.2
                                # Execution count [1.00e+00]
        cmpl      %esi, %eax                                    #148.5
        ja        ..B3.22       # Prob 50%                      #148.5
                                # LOE rbp rdi r8 r9 r10 r12 r13 r14 r15 eax esi xmm0
..B3.7:                         # Preds ..B3.6
                                # Execution count [0.00e+00]
        lea       -1(%rax), %edx                                #141.1
        subl      %edx, %esi                                    #141.1
        decl      %esi                                          #141.1
        jmp       *.2.19_2.switchtab.4(,%rsi,8)                 #148.5
                                # LOE rbp rdi r8 r9 r10 r12 r13 r14 r15 eax xmm0
..1.19_0.TAG.6:
..B3.9:                         # Preds ..B3.7
                                # Execution count [0.00e+00]
        cltq                                                    #149.9
        vmulsd    40(%r10,%rax,8), %xmm0, %xmm1                 #149.20
        vmovsd    %xmm1, 40(%r10,%rax,8)                        #149.9
                                # LOE rbp rdi r8 r9 r10 r12 r13 r14 r15 eax xmm0
..1.19_0.TAG.5:
..B3.11:                        # Preds ..B3.7 ..B3.9
                                # Execution count [0.00e+00]
        cltq                                                    #149.9
        vmulsd    32(%r10,%rax,8), %xmm0, %xmm1                 #149.20
        vmovsd    %xmm1, 32(%r10,%rax,8)                        #149.9
                                # LOE rbp rdi r8 r9 r10 r12 r13 r14 r15 eax xmm0
..1.19_0.TAG.4:
..B3.13:                        # Preds ..B3.7 ..B3.11
                                # Execution count [0.00e+00]
        cltq                                                    #149.9
        vmulsd    24(%r10,%rax,8), %xmm0, %xmm1                 #149.20
        vmovsd    %xmm1, 24(%r10,%rax,8)                        #149.9
                                # LOE rbp rdi r8 r9 r10 r12 r13 r14 r15 eax xmm0
..1.19_0.TAG.3:
..B3.15:                        # Preds ..B3.7 ..B3.13
                                # Execution count [0.00e+00]
        cltq                                                    #149.9
        vmulsd    16(%r10,%rax,8), %xmm0, %xmm1                 #149.20
        vmovsd    %xmm1, 16(%r10,%rax,8)                        #149.9
                                # LOE rbp rdi r8 r9 r10 r12 r13 r14 r15 eax xmm0
..1.19_0.TAG.2:
..B3.17:                        # Preds ..B3.7 ..B3.15
                                # Execution count [0.00e+00]
        cltq                                                    #149.9
        vmulsd    8(%r10,%rax,8), %xmm0, %xmm1                  #149.20
        vmovsd    %xmm1, 8(%r10,%rax,8)                         #149.9
                                # LOE rbp rdi r8 r9 r10 r12 r13 r14 r15 eax xmm0
..1.19_0.TAG.1:
..B3.19:                        # Preds ..B3.7 ..B3.17
                                # Execution count [0.00e+00]
        cltq                                                    #148.5
        vmulsd    (%r10,%rax,8), %xmm0, %xmm1                   #149.20
        vmovsd    %xmm1, (%r10,%rax,8)                          #149.9
                                # LOE rbp rdi r8 r9 r10 r12 r13 r14 r15 eax xmm0
..1.19_0.TAG.0:
..B3.21:                        # Preds ..B3.7 ..B3.19
                                # Execution count [9.00e-01]
        movslq    %eax, %rax                                    #149.9
        vmulsd    -8(%r10,%rax,8), %xmm0, %xmm0                 #149.20
        vmovsd    %xmm0, -8(%r10,%rax,8)                        #149.9
                                # LOE rbp rdi r8 r9 r12 r13 r14 r15
..B3.22:                        # Preds ..B3.6 ..B3.1 ..B3.21
                                # Execution count [1.00e+00]
        rdtsc                                                   #151.0
        movq      %rdx, %r10                                    #151.0
        movq      %rax, %rsi                                    #151.0
        cpuid                                                   #151.0
        shlq      $32, %r10                                     #151.5
        shlq      $32, %r9                                      #151.5
        orq       %rsi, %r10                                    #151.5
        orq       %rdi, %r9                                     #151.5
        subq      %r9, %r10                                     #151.5
        movq      %r10, (%r8)                                   #151.5
	.cfi_restore 3
        popq      %rbx                                          #158.1
	.cfi_def_cfa_offset 8
        ret                                                     #158.1
        .align    16,0x90
                                # LOE
	.cfi_endproc
# mark_end;
	.type	update,@function
	.size	update,.-update
..LNupdate.2:
	.section .rodata, "a"
	.align 8
	.align 8
.2.19_2.switchtab.4:
	.quad	..1.19_0.TAG.0
	.quad	..1.19_0.TAG.1
	.quad	..1.19_0.TAG.2
	.quad	..1.19_0.TAG.3
	.quad	..1.19_0.TAG.4
	.quad	..1.19_0.TAG.5
	.quad	..1.19_0.TAG.6
	.data
# -- End  update
	.text
.L_2__routine_start_triad_3:
# -- Begin  triad
	.text
# mark_begin;
       .align    16,0x90
	.globl triad
# --- triad(double *, double *, double *, double, int, uint64_t *)
triad:
# parameter 1: %rdi
# parameter 2: %rsi
# parameter 3: %rdx
# parameter 4: %xmm0
# parameter 5: %ecx
# parameter 6: %r8
..B4.1:                         # Preds ..B4.0
                                # Execution count [1.00e+00]
	.cfi_startproc
..___tag_value_triad.41:
..L42:
                                                         #165.65
        pushq     %rbx                                          #165.65
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
        pushq     %rbp                                          #165.65
	.cfi_def_cfa_offset 24
	.cfi_offset 6, -24
        movl      %ecx, %ebp                                    #165.65
        movq      %rdx, %r9                                     #165.65
        movq      %rdi, %r10                                    #165.65
        rdtscp                                                  #171.0
        rdtsc                                                   #171.0
        cpuid                                                   #171.0
        rdtscp                                                  #171.0
        movq      %rdx, %r11                                    #171.0
        movq      %rax, %rdi                                    #171.0
        testl     %ebp, %ebp                                    #172.24
        jle       ..B4.33       # Prob 50%                      #172.24
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp xmm0
..B4.2:                         # Preds ..B4.1
                                # Execution count [0.00e+00]
        cmpl      $6, %ebp                                      #172.5
        jle       ..B4.27       # Prob 50%                      #172.5
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp xmm0
..B4.3:                         # Preds ..B4.2
                                # Execution count [0.00e+00]
        movslq    %ebp, %rdx                                    #165.1
        movq      %rsi, %rax                                    #174.27
        shlq      $3, %rdx                                      #172.5
        subq      %r9, %rax                                     #174.27
        cmpq      %rdx, %rax                                    #172.5
        jge       ..B4.5        # Prob 50%                      #172.5
                                # LOE rax rdx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp xmm0
..B4.4:                         # Preds ..B4.3
                                # Execution count [0.00e+00]
        negq      %rax                                          #174.9
        cmpq      %rdx, %rax                                    #172.5
        jl        ..B4.27       # Prob 50%                      #172.5
                                # LOE rdx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp xmm0
..B4.5:                         # Preds ..B4.3 ..B4.4
                                # Execution count [0.00e+00]
        movq      %rsi, %rax                                    #174.16
        subq      %r10, %rax                                    #174.16
        cmpq      %rdx, %rax                                    #172.5
        jge       ..B4.7        # Prob 50%                      #172.5
                                # LOE rax rdx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp xmm0
..B4.6:                         # Preds ..B4.5
                                # Execution count [0.00e+00]
        negq      %rax                                          #174.9
        cmpq      %rdx, %rax                                    #172.5
        jl        ..B4.27       # Prob 50%                      #172.5
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp xmm0
..B4.7:                         # Preds ..B4.5 ..B4.6
                                # Execution count [5.00e-01]
        movl      %ebp, %ebx                                    #172.5
        xorl      %ecx, %ecx                                    #172.5
        movl      $1, %eax                                      #172.5
        xorl      %edx, %edx                                    #172.5
        shrl      $3, %ebx                                      #172.5
        je        ..B4.11       # Prob 9%                       #172.5
                                # LOE rdx rcx rbx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax ebp xmm0
..B4.9:                         # Preds ..B4.7 ..B4.9
                                # Execution count [3.12e-01]
        vmovsd    (%rdx,%r9), %xmm1                             #174.27
        incq      %rcx                                          #172.5
        vmovsd    8(%rdx,%r9), %xmm2                            #174.27
        vmovsd    16(%rdx,%r9), %xmm3                           #174.27
        vmovsd    24(%rdx,%r9), %xmm4                           #174.27
        vmovsd    32(%rdx,%r9), %xmm5                           #174.27
        vmovsd    40(%rdx,%r9), %xmm6                           #174.27
        vmovsd    48(%rdx,%r9), %xmm7                           #174.27
        vmovsd    56(%rdx,%r9), %xmm8                           #174.27
        vfmadd213sd (%rdx,%r10), %xmm0, %xmm1                   #174.9
        vfmadd213sd 8(%rdx,%r10), %xmm0, %xmm2                  #174.9
        vfmadd213sd 16(%rdx,%r10), %xmm0, %xmm3                 #174.9
        vfmadd213sd 24(%rdx,%r10), %xmm0, %xmm4                 #174.9
        vfmadd213sd 32(%rdx,%r10), %xmm0, %xmm5                 #174.9
        vfmadd213sd 40(%rdx,%r10), %xmm0, %xmm6                 #174.9
        vfmadd213sd 48(%rdx,%r10), %xmm0, %xmm7                 #174.9
        vfmadd213sd 56(%rdx,%r10), %xmm0, %xmm8                 #174.9
        vmovsd    %xmm1, (%rdx,%rsi)                            #174.9
        vmovsd    %xmm2, 8(%rdx,%rsi)                           #174.9
        vmovsd    %xmm3, 16(%rdx,%rsi)                          #174.9
        vmovsd    %xmm4, 24(%rdx,%rsi)                          #174.9
        vmovsd    %xmm5, 32(%rdx,%rsi)                          #174.9
        vmovsd    %xmm6, 40(%rdx,%rsi)                          #174.9
        vmovsd    %xmm7, 48(%rdx,%rsi)                          #174.9
        vmovsd    %xmm8, 56(%rdx,%rsi)                          #174.9
        addq      $64, %rdx                                     #172.5
        cmpq      %rbx, %rcx                                    #172.5
        jb        ..B4.9        # Prob 99%                      #172.5
                                # LOE rdx rcx rbx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp xmm0
..B4.10:                        # Preds ..B4.9
                                # Execution count [4.50e-01]
        lea       1(,%rcx,8), %eax                              #174.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax ebp xmm0
..B4.11:                        # Preds ..B4.10 ..B4.7
                                # Execution count [5.00e-01]
        cmpl      %ebp, %eax                                    #172.5
        ja        ..B4.33       # Prob 50%                      #172.5
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax ebp xmm0
..B4.12:                        # Preds ..B4.11
                                # Execution count [0.00e+00]
        lea       -1(%rax), %edx                                #165.1
        subl      %edx, %ebp                                    #165.1
        decl      %ebp                                          #165.1
        jmp       *.2.20_2.switchtab.6(,%rbp,8)                 #172.5
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax xmm0
..1.20_0.TAG.6:
..B4.14:                        # Preds ..B4.12
                                # Execution count [0.00e+00]
        cltq                                                    #174.9
        vmovsd    40(%r9,%rax,8), %xmm1                         #174.27
        vfmadd213sd 40(%r10,%rax,8), %xmm0, %xmm1               #174.9
        vmovsd    %xmm1, 40(%rsi,%rax,8)                        #174.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax xmm0
..1.20_0.TAG.5:
..B4.16:                        # Preds ..B4.12 ..B4.14
                                # Execution count [0.00e+00]
        cltq                                                    #174.9
        vmovsd    32(%r9,%rax,8), %xmm1                         #174.27
        vfmadd213sd 32(%r10,%rax,8), %xmm0, %xmm1               #174.9
        vmovsd    %xmm1, 32(%rsi,%rax,8)                        #174.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax xmm0
..1.20_0.TAG.4:
..B4.18:                        # Preds ..B4.12 ..B4.16
                                # Execution count [0.00e+00]
        cltq                                                    #174.9
        vmovsd    24(%r9,%rax,8), %xmm1                         #174.27
        vfmadd213sd 24(%r10,%rax,8), %xmm0, %xmm1               #174.9
        vmovsd    %xmm1, 24(%rsi,%rax,8)                        #174.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax xmm0
..1.20_0.TAG.3:
..B4.20:                        # Preds ..B4.12 ..B4.18
                                # Execution count [0.00e+00]
        cltq                                                    #174.9
        vmovsd    16(%r9,%rax,8), %xmm1                         #174.27
        vfmadd213sd 16(%r10,%rax,8), %xmm0, %xmm1               #174.9
        vmovsd    %xmm1, 16(%rsi,%rax,8)                        #174.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax xmm0
..1.20_0.TAG.2:
..B4.22:                        # Preds ..B4.12 ..B4.20
                                # Execution count [0.00e+00]
        cltq                                                    #174.9
        vmovsd    8(%r9,%rax,8), %xmm1                          #174.27
        vfmadd213sd 8(%r10,%rax,8), %xmm0, %xmm1                #174.9
        vmovsd    %xmm1, 8(%rsi,%rax,8)                         #174.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax xmm0
..1.20_0.TAG.1:
..B4.24:                        # Preds ..B4.12 ..B4.22
                                # Execution count [0.00e+00]
        cltq                                                    #172.5
        vmovsd    (%r9,%rax,8), %xmm1                           #174.27
        vfmadd213sd (%r10,%rax,8), %xmm0, %xmm1                 #174.9
        vmovsd    %xmm1, (%rsi,%rax,8)                          #174.9
        jmp       ..B4.32       # Prob 100%                     #174.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax xmm0
..B4.27:                        # Preds ..B4.2 ..B4.4 ..B4.6
                                # Execution count [5.00e-01]
        movl      %ebp, %ebx                                    #172.5
        xorl      %ecx, %ecx                                    #172.5
        movl      $1, %eax                                      #172.5
        xorl      %edx, %edx                                    #172.5
        shrl      $1, %ebx                                      #172.5
        je        ..B4.31       # Prob 9%                       #172.5
                                # LOE rdx rcx rbx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax ebp xmm0
..B4.29:                        # Preds ..B4.27 ..B4.29
                                # Execution count [1.25e+00]
        vmovsd    (%rdx,%r9), %xmm1                             #174.27
        incq      %rcx                                          #172.5
        vfmadd213sd (%rdx,%r10), %xmm0, %xmm1                   #174.9
        vmovsd    %xmm1, (%rdx,%rsi)                            #174.9
        vmovsd    8(%rdx,%r9), %xmm2                            #174.27
        vfmadd213sd 8(%rdx,%r10), %xmm0, %xmm2                  #174.9
        vmovsd    %xmm2, 8(%rdx,%rsi)                           #174.9
        addq      $16, %rdx                                     #172.5
        cmpq      %rbx, %rcx                                    #172.5
        jb        ..B4.29       # Prob 63%                      #172.5
                                # LOE rdx rcx rbx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp xmm0
..B4.30:                        # Preds ..B4.29
                                # Execution count [4.50e-01]
        lea       1(%rcx,%rcx), %eax                            #174.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax ebp xmm0
..B4.31:                        # Preds ..B4.30 ..B4.27
                                # Execution count [5.00e-01]
        lea       -1(%rax), %edx                                #172.5
        cmpl      %ebp, %edx                                    #172.5
        jae       ..B4.33       # Prob 9%                       #172.5
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax xmm0
..1.20_0.TAG.0:
..B4.32:                        # Preds ..B4.24 ..B4.12 ..B4.31
                                # Execution count [4.50e-01]
        movslq    %eax, %rax                                    #172.5
        vmovsd    -8(%r9,%rax,8), %xmm1                         #174.27
        vfmadd213sd -8(%r10,%rax,8), %xmm1, %xmm0               #174.9
        vmovsd    %xmm0, -8(%rsi,%rax,8)                        #174.9
                                # LOE rdi r8 r11 r12 r13 r14 r15
..B4.33:                        # Preds ..B4.1 ..B4.31 ..B4.11 ..B4.32
                                # Execution count [1.00e+00]
        rdtsc                                                   #176.0
        movq      %rdx, %rsi                                    #176.0
        movq      %rax, %rbp                                    #176.0
        cpuid                                                   #176.0
        shlq      $32, %rsi                                     #176.5
        shlq      $32, %r11                                     #176.5
        orq       %rbp, %rsi                                    #176.5
        orq       %rdi, %r11                                    #176.5
        subq      %r11, %rsi                                    #176.5
        movq      %rsi, (%r8)                                   #176.5
	.cfi_restore 6
        popq      %rbp                                          #183.1
	.cfi_def_cfa_offset 16
	.cfi_restore 3
        popq      %rbx                                          #183.1
	.cfi_def_cfa_offset 8
        ret                                                     #183.1
        .align    16,0x90
                                # LOE
	.cfi_endproc
# mark_end;
	.type	triad,@function
	.size	triad,.-triad
..LNtriad.3:
	.section .rodata, "a"
	.align 8
.2.20_2.switchtab.6:
	.quad	..1.20_0.TAG.0
	.quad	..1.20_0.TAG.1
	.quad	..1.20_0.TAG.2
	.quad	..1.20_0.TAG.3
	.quad	..1.20_0.TAG.4
	.quad	..1.20_0.TAG.5
	.quad	..1.20_0.TAG.6
	.data
# -- End  triad
	.text
.L_2__routine_start_daxpy_4:
# -- Begin  daxpy
	.text
# mark_begin;
       .align    16,0x90
	.globl daxpy
# --- daxpy(double *, double *, double, int, uint64_t *)
daxpy:
# parameter 1: %rdi
# parameter 2: %rsi
# parameter 3: %xmm0
# parameter 4: %edx
# parameter 5: %rcx
..B5.1:                         # Preds ..B5.0
                                # Execution count [1.00e+00]
	.cfi_startproc
..___tag_value_daxpy.52:
..L53:
                                                         #190.56
        pushq     %rbx                                          #190.56
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
        movl      %edx, %r9d                                    #190.56
        movq      %rcx, %r8                                     #190.56
        movq      %rdi, %r10                                    #190.56
        rdtscp                                                  #196.0
        rdtsc                                                   #196.0
        cpuid                                                   #196.0
        rdtscp                                                  #196.0
        movq      %rdx, %r11                                    #196.0
        movq      %rax, %rdi                                    #196.0
        testl     %r9d, %r9d                                    #197.24
        jle       ..B5.31       # Prob 50%                      #197.24
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d xmm0
..B5.2:                         # Preds ..B5.1
                                # Execution count [0.00e+00]
        cmpl      $6, %r9d                                      #197.5
        jle       ..B5.25       # Prob 50%                      #197.5
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d xmm0
..B5.3:                         # Preds ..B5.2
                                # Execution count [0.00e+00]
        movslq    %r9d, %rdx                                    #190.1
        movq      %r10, %rax                                    #198.23
        shlq      $3, %rdx                                      #197.5
        subq      %rsi, %rax                                    #198.23
        cmpq      %rdx, %rax                                    #197.5
        jge       ..B5.5        # Prob 50%                      #197.5
                                # LOE rax rdx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d xmm0
..B5.4:                         # Preds ..B5.3
                                # Execution count [0.00e+00]
        negq      %rax                                          #198.16
        cmpq      %rdx, %rax                                    #197.5
        jl        ..B5.25       # Prob 50%                      #197.5
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d xmm0
..B5.5:                         # Preds ..B5.3 ..B5.4
                                # Execution count [5.00e-01]
        movl      %r9d, %ebx                                    #197.5
        xorl      %ecx, %ecx                                    #197.5
        movl      $1, %eax                                      #197.5
        xorl      %edx, %edx                                    #197.5
        shrl      $3, %ebx                                      #197.5
        je        ..B5.9        # Prob 9%                       #197.5
                                # LOE rdx rcx rbx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax r9d xmm0
..B5.7:                         # Preds ..B5.5 ..B5.7
                                # Execution count [3.12e-01]
        vmovsd    (%rdx,%rsi), %xmm1                            #198.23
        incq      %rcx                                          #197.5
        vmovsd    8(%rdx,%rsi), %xmm2                           #198.23
        vmovsd    16(%rdx,%rsi), %xmm3                          #198.23
        vmovsd    24(%rdx,%rsi), %xmm4                          #198.23
        vmovsd    32(%rdx,%rsi), %xmm5                          #198.23
        vmovsd    40(%rdx,%rsi), %xmm6                          #198.23
        vmovsd    48(%rdx,%rsi), %xmm7                          #198.23
        vmovsd    56(%rdx,%rsi), %xmm8                          #198.23
        vfmadd213sd (%rdx,%r10), %xmm0, %xmm1                   #198.9
        vfmadd213sd 8(%rdx,%r10), %xmm0, %xmm2                  #198.9
        vfmadd213sd 16(%rdx,%r10), %xmm0, %xmm3                 #198.9
        vfmadd213sd 24(%rdx,%r10), %xmm0, %xmm4                 #198.9
        vfmadd213sd 32(%rdx,%r10), %xmm0, %xmm5                 #198.9
        vfmadd213sd 40(%rdx,%r10), %xmm0, %xmm6                 #198.9
        vfmadd213sd 48(%rdx,%r10), %xmm0, %xmm7                 #198.9
        vfmadd213sd 56(%rdx,%r10), %xmm0, %xmm8                 #198.9
        vmovsd    %xmm1, (%rdx,%r10)                            #198.9
        vmovsd    %xmm2, 8(%rdx,%r10)                           #198.9
        vmovsd    %xmm3, 16(%rdx,%r10)                          #198.9
        vmovsd    %xmm4, 24(%rdx,%r10)                          #198.9
        vmovsd    %xmm5, 32(%rdx,%r10)                          #198.9
        vmovsd    %xmm6, 40(%rdx,%r10)                          #198.9
        vmovsd    %xmm7, 48(%rdx,%r10)                          #198.9
        vmovsd    %xmm8, 56(%rdx,%r10)                          #198.9
        addq      $64, %rdx                                     #197.5
        cmpq      %rbx, %rcx                                    #197.5
        jb        ..B5.7        # Prob 99%                      #197.5
                                # LOE rdx rcx rbx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d xmm0
..B5.8:                         # Preds ..B5.7
                                # Execution count [4.50e-01]
        lea       1(,%rcx,8), %eax                              #198.9
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax r9d xmm0
..B5.9:                         # Preds ..B5.8 ..B5.5
                                # Execution count [5.00e-01]
        cmpl      %r9d, %eax                                    #197.5
        ja        ..B5.31       # Prob 50%                      #197.5
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax r9d xmm0
..B5.10:                        # Preds ..B5.9
                                # Execution count [0.00e+00]
        lea       -1(%rax), %edx                                #190.1
        subl      %edx, %r9d                                    #190.1
        decl      %r9d                                          #190.1
        jmp       *.2.21_2.switchtab.5(,%r9,8)                  #197.5
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax xmm0
..1.21_0.TAG.6:
..B5.12:                        # Preds ..B5.10
                                # Execution count [0.00e+00]
        cltq                                                    #198.9
        vmovsd    40(%rsi,%rax,8), %xmm1                        #198.23
        vfmadd213sd 40(%r10,%rax,8), %xmm0, %xmm1               #198.9
        vmovsd    %xmm1, 40(%r10,%rax,8)                        #198.9
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax xmm0
..1.21_0.TAG.5:
..B5.14:                        # Preds ..B5.10 ..B5.12
                                # Execution count [0.00e+00]
        cltq                                                    #198.9
        vmovsd    32(%rsi,%rax,8), %xmm1                        #198.23
        vfmadd213sd 32(%r10,%rax,8), %xmm0, %xmm1               #198.9
        vmovsd    %xmm1, 32(%r10,%rax,8)                        #198.9
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax xmm0
..1.21_0.TAG.4:
..B5.16:                        # Preds ..B5.10 ..B5.14
                                # Execution count [0.00e+00]
        cltq                                                    #198.9
        vmovsd    24(%rsi,%rax,8), %xmm1                        #198.23
        vfmadd213sd 24(%r10,%rax,8), %xmm0, %xmm1               #198.9
        vmovsd    %xmm1, 24(%r10,%rax,8)                        #198.9
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax xmm0
..1.21_0.TAG.3:
..B5.18:                        # Preds ..B5.10 ..B5.16
                                # Execution count [0.00e+00]
        cltq                                                    #198.9
        vmovsd    16(%rsi,%rax,8), %xmm1                        #198.23
        vfmadd213sd 16(%r10,%rax,8), %xmm0, %xmm1               #198.9
        vmovsd    %xmm1, 16(%r10,%rax,8)                        #198.9
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax xmm0
..1.21_0.TAG.2:
..B5.20:                        # Preds ..B5.10 ..B5.18
                                # Execution count [0.00e+00]
        cltq                                                    #198.9
        vmovsd    8(%rsi,%rax,8), %xmm1                         #198.23
        vfmadd213sd 8(%r10,%rax,8), %xmm0, %xmm1                #198.9
        vmovsd    %xmm1, 8(%r10,%rax,8)                         #198.9
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax xmm0
..1.21_0.TAG.1:
..B5.22:                        # Preds ..B5.10 ..B5.20
                                # Execution count [0.00e+00]
        cltq                                                    #197.5
        vmovsd    (%rsi,%rax,8), %xmm1                          #198.23
        vfmadd213sd (%r10,%rax,8), %xmm0, %xmm1                 #198.9
        vmovsd    %xmm1, (%r10,%rax,8)                          #198.9
        jmp       ..B5.30       # Prob 100%                     #198.9
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax xmm0
..B5.25:                        # Preds ..B5.2 ..B5.4
                                # Execution count [5.00e-01]
        movl      %r9d, %ebx                                    #197.5
        xorl      %ecx, %ecx                                    #197.5
        movl      $1, %eax                                      #197.5
        xorl      %edx, %edx                                    #197.5
        shrl      $1, %ebx                                      #197.5
        je        ..B5.29       # Prob 9%                       #197.5
                                # LOE rdx rcx rbx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax r9d xmm0
..B5.27:                        # Preds ..B5.25 ..B5.27
                                # Execution count [1.25e+00]
        vmovsd    (%rdx,%rsi), %xmm1                            #198.23
        incq      %rcx                                          #197.5
        vfmadd213sd (%rdx,%r10), %xmm0, %xmm1                   #198.9
        vmovsd    %xmm1, (%rdx,%r10)                            #198.9
        vmovsd    8(%rdx,%rsi), %xmm2                           #198.23
        vfmadd213sd 8(%rdx,%r10), %xmm0, %xmm2                  #198.9
        vmovsd    %xmm2, 8(%rdx,%r10)                           #198.9
        addq      $16, %rdx                                     #197.5
        cmpq      %rbx, %rcx                                    #197.5
        jb        ..B5.27       # Prob 63%                      #197.5
                                # LOE rdx rcx rbx rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 r9d xmm0
..B5.28:                        # Preds ..B5.27
                                # Execution count [4.50e-01]
        lea       1(%rcx,%rcx), %eax                            #198.9
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax r9d xmm0
..B5.29:                        # Preds ..B5.28 ..B5.25
                                # Execution count [5.00e-01]
        lea       -1(%rax), %edx                                #197.5
        cmpl      %r9d, %edx                                    #197.5
        jae       ..B5.31       # Prob 9%                       #197.5
                                # LOE rbp rsi rdi r8 r10 r11 r12 r13 r14 r15 eax xmm0
..1.21_0.TAG.0:
..B5.30:                        # Preds ..B5.22 ..B5.10 ..B5.29
                                # Execution count [4.50e-01]
        movslq    %eax, %rax                                    #197.5
        vmovsd    -8(%rsi,%rax,8), %xmm1                        #198.23
        vfmadd213sd -8(%r10,%rax,8), %xmm1, %xmm0               #198.9
        vmovsd    %xmm0, -8(%r10,%rax,8)                        #198.9
                                # LOE rbp rdi r8 r11 r12 r13 r14 r15
..B5.31:                        # Preds ..B5.1 ..B5.29 ..B5.9 ..B5.30
                                # Execution count [1.00e+00]
        rdtsc                                                   #200.0
        movq      %rdx, %r9                                     #200.0
        movq      %rax, %rsi                                    #200.0
        cpuid                                                   #200.0
        shlq      $32, %r9                                      #200.5
        shlq      $32, %r11                                     #200.5
        orq       %rsi, %r9                                     #200.5
        orq       %rdi, %r11                                    #200.5
        subq      %r11, %r9                                     #200.5
        movq      %r9, (%r8)                                    #200.5
	.cfi_restore 3
        popq      %rbx                                          #207.1
	.cfi_def_cfa_offset 8
        ret                                                     #207.1
        .align    16,0x90
                                # LOE
	.cfi_endproc
# mark_end;
	.type	daxpy,@function
	.size	daxpy,.-daxpy
..LNdaxpy.4:
	.section .rodata, "a"
	.align 8
.2.21_2.switchtab.5:
	.quad	..1.21_0.TAG.0
	.quad	..1.21_0.TAG.1
	.quad	..1.21_0.TAG.2
	.quad	..1.21_0.TAG.3
	.quad	..1.21_0.TAG.4
	.quad	..1.21_0.TAG.5
	.quad	..1.21_0.TAG.6
	.data
# -- End  daxpy
	.text
.L_2__routine_start_striad_5:
# -- Begin  striad
	.text
# mark_begin;
       .align    16,0x90
	.globl striad
# --- striad(double *, double *, double *, double *, int, uint64_t *)
striad:
# parameter 1: %rdi
# parameter 2: %rsi
# parameter 3: %rdx
# parameter 4: %rcx
# parameter 5: %r8d
# parameter 6: %r9
..B6.1:                         # Preds ..B6.0
                                # Execution count [1.00e+00]
	.cfi_startproc
..___tag_value_striad.59:
..L60:
                                                         #214.67
        pushq     %rbx                                          #214.67
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
        pushq     %r15                                          #214.67
	.cfi_def_cfa_offset 24
	.cfi_offset 15, -24
        pushq     %rbp                                          #214.67
	.cfi_def_cfa_offset 32
	.cfi_offset 6, -32
        movq      %r9, %r10                                     #214.67
        movq      %rcx, %r9                                     #214.67
        movq      %rdx, %rbp                                    #214.67
        movq      %rdi, %r11                                    #214.67
        rdtscp                                                  #219.0
        rdtsc                                                   #219.0
        cpuid                                                   #219.0
        rdtscp                                                  #219.0
        movq      %rdx, %r15                                    #219.0
        movq      %rax, %rdi                                    #219.0
        testl     %r8d, %r8d                                    #220.24
        jle       ..B6.34       # Prob 50%                      #220.24
                                # LOE rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 r8d
..B6.2:                         # Preds ..B6.1
                                # Execution count [0.00e+00]
        movslq    %r8d, %rdx                                    #214.1
        movq      %rsi, %rax                                    #222.15
        shlq      $3, %rdx                                      #220.5
        subq      %r11, %rax                                    #222.15
        cmpq      %rdx, %rax                                    #220.5
        jge       ..B6.4        # Prob 50%                      #220.5
                                # LOE rax rdx rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 r8d
..B6.3:                         # Preds ..B6.2
                                # Execution count [0.00e+00]
        negq      %rax                                          #222.8
        cmpq      %rdx, %rax                                    #220.5
        jl        ..B6.28       # Prob 50%                      #220.5
                                # LOE rdx rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 r8d
..B6.4:                         # Preds ..B6.2 ..B6.3
                                # Execution count [0.00e+00]
        movq      %rsi, %rax                                    #222.22
        subq      %rbp, %rax                                    #222.22
        cmpq      %rdx, %rax                                    #220.5
        jge       ..B6.6        # Prob 50%                      #220.5
                                # LOE rax rdx rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 r8d
..B6.5:                         # Preds ..B6.4
                                # Execution count [0.00e+00]
        negq      %rax                                          #222.8
        cmpq      %rdx, %rax                                    #220.5
        jl        ..B6.28       # Prob 50%                      #220.5
                                # LOE rdx rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 r8d
..B6.6:                         # Preds ..B6.4 ..B6.5
                                # Execution count [0.00e+00]
        movq      %rsi, %rax                                    #222.29
        subq      %r9, %rax                                     #222.29
        cmpq      %rdx, %rax                                    #220.5
        jge       ..B6.8        # Prob 50%                      #220.5
                                # LOE rax rdx rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 r8d
..B6.7:                         # Preds ..B6.6
                                # Execution count [0.00e+00]
        negq      %rax                                          #222.8
        cmpq      %rdx, %rax                                    #220.5
        jl        ..B6.28       # Prob 50%                      #220.5
                                # LOE rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 r8d
..B6.8:                         # Preds ..B6.6 ..B6.7
                                # Execution count [5.00e-01]
        movl      %r8d, %ebx                                    #220.5
        xorl      %ecx, %ecx                                    #220.5
        movl      $1, %eax                                      #220.5
        xorl      %edx, %edx                                    #220.5
        shrl      $3, %ebx                                      #220.5
        je        ..B6.12       # Prob 9%                       #220.5
                                # LOE rdx rcx rbx rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 eax r8d
..B6.10:                        # Preds ..B6.8 ..B6.10
                                # Execution count [3.12e-01]
        vmovsd    (%rdx,%rbp), %xmm1                            #222.22
        incq      %rcx                                          #220.5
        vmovsd    8(%rdx,%rbp), %xmm3                           #222.22
        vmovsd    16(%rdx,%rbp), %xmm5                          #222.22
        vmovsd    24(%rdx,%rbp), %xmm7                          #222.22
        vmovsd    32(%rdx,%rbp), %xmm9                          #222.22
        vmovsd    40(%rdx,%rbp), %xmm11                         #222.22
        vmovsd    48(%rdx,%rbp), %xmm13                         #222.22
        vmovsd    56(%rdx,%rbp), %xmm15                         #222.22
        vmovsd    (%rdx,%r9), %xmm0                             #222.29
        vmovsd    8(%rdx,%r9), %xmm2                            #222.29
        vmovsd    16(%rdx,%r9), %xmm4                           #222.29
        vmovsd    24(%rdx,%r9), %xmm6                           #222.29
        vmovsd    32(%rdx,%r9), %xmm8                           #222.29
        vmovsd    40(%rdx,%r9), %xmm10                          #222.29
        vmovsd    48(%rdx,%r9), %xmm12                          #222.29
        vmovsd    56(%rdx,%r9), %xmm14                          #222.29
        vfmadd213sd (%rdx,%r11), %xmm0, %xmm1                   #222.8
        vfmadd213sd 8(%rdx,%r11), %xmm2, %xmm3                  #222.8
        vfmadd213sd 16(%rdx,%r11), %xmm4, %xmm5                 #222.8
        vfmadd213sd 24(%rdx,%r11), %xmm6, %xmm7                 #222.8
        vfmadd213sd 32(%rdx,%r11), %xmm8, %xmm9                 #222.8
        vfmadd213sd 40(%rdx,%r11), %xmm10, %xmm11               #222.8
        vfmadd213sd 48(%rdx,%r11), %xmm12, %xmm13               #222.8
        vfmadd213sd 56(%rdx,%r11), %xmm14, %xmm15               #222.8
        vmovsd    %xmm1, (%rdx,%rsi)                            #222.8
        vmovsd    %xmm3, 8(%rdx,%rsi)                           #222.8
        vmovsd    %xmm5, 16(%rdx,%rsi)                          #222.8
        vmovsd    %xmm7, 24(%rdx,%rsi)                          #222.8
        vmovsd    %xmm9, 32(%rdx,%rsi)                          #222.8
        vmovsd    %xmm11, 40(%rdx,%rsi)                         #222.8
        vmovsd    %xmm13, 48(%rdx,%rsi)                         #222.8
        vmovsd    %xmm15, 56(%rdx,%rsi)                         #222.8
        addq      $64, %rdx                                     #220.5
        cmpq      %rbx, %rcx                                    #220.5
        jb        ..B6.10       # Prob 99%                      #220.5
                                # LOE rdx rcx rbx rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 r8d
..B6.11:                        # Preds ..B6.10
                                # Execution count [4.50e-01]
        lea       1(,%rcx,8), %eax                              #222.8
                                # LOE rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 eax r8d
..B6.12:                        # Preds ..B6.11 ..B6.8
                                # Execution count [5.00e-01]
        cmpl      %r8d, %eax                                    #220.5
        ja        ..B6.34       # Prob 50%                      #220.5
                                # LOE rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 eax r8d
..B6.13:                        # Preds ..B6.12
                                # Execution count [0.00e+00]
        lea       -1(%rax), %edx                                #214.1
        subl      %edx, %r8d                                    #214.1
        decl      %r8d                                          #214.1
        jmp       *.2.22_2.switchtab.6(,%r8,8)                  #220.5
                                # LOE rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 eax
..1.22_0.TAG.6:
..B6.15:                        # Preds ..B6.13
                                # Execution count [0.00e+00]
        cltq                                                    #222.8
        vmovsd    40(%rbp,%rax,8), %xmm1                        #222.22
        vmovsd    40(%r9,%rax,8), %xmm0                         #222.29
        vfmadd213sd 40(%r11,%rax,8), %xmm0, %xmm1               #222.8
        vmovsd    %xmm1, 40(%rsi,%rax,8)                        #222.8
                                # LOE rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 eax
..1.22_0.TAG.5:
..B6.17:                        # Preds ..B6.13 ..B6.15
                                # Execution count [0.00e+00]
        cltq                                                    #222.8
        vmovsd    32(%rbp,%rax,8), %xmm1                        #222.22
        vmovsd    32(%r9,%rax,8), %xmm0                         #222.29
        vfmadd213sd 32(%r11,%rax,8), %xmm0, %xmm1               #222.8
        vmovsd    %xmm1, 32(%rsi,%rax,8)                        #222.8
                                # LOE rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 eax
..1.22_0.TAG.4:
..B6.19:                        # Preds ..B6.13 ..B6.17
                                # Execution count [0.00e+00]
        cltq                                                    #222.8
        vmovsd    24(%rbp,%rax,8), %xmm1                        #222.22
        vmovsd    24(%r9,%rax,8), %xmm0                         #222.29
        vfmadd213sd 24(%r11,%rax,8), %xmm0, %xmm1               #222.8
        vmovsd    %xmm1, 24(%rsi,%rax,8)                        #222.8
                                # LOE rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 eax
..1.22_0.TAG.3:
..B6.21:                        # Preds ..B6.13 ..B6.19
                                # Execution count [0.00e+00]
        cltq                                                    #222.8
        vmovsd    16(%rbp,%rax,8), %xmm1                        #222.22
        vmovsd    16(%r9,%rax,8), %xmm0                         #222.29
        vfmadd213sd 16(%r11,%rax,8), %xmm0, %xmm1               #222.8
        vmovsd    %xmm1, 16(%rsi,%rax,8)                        #222.8
                                # LOE rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 eax
..1.22_0.TAG.2:
..B6.23:                        # Preds ..B6.13 ..B6.21
                                # Execution count [0.00e+00]
        cltq                                                    #222.8
        vmovsd    8(%rbp,%rax,8), %xmm1                         #222.22
        vmovsd    8(%r9,%rax,8), %xmm0                          #222.29
        vfmadd213sd 8(%r11,%rax,8), %xmm0, %xmm1                #222.8
        vmovsd    %xmm1, 8(%rsi,%rax,8)                         #222.8
                                # LOE rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 eax
..1.22_0.TAG.1:
..B6.25:                        # Preds ..B6.13 ..B6.23
                                # Execution count [0.00e+00]
        cltq                                                    #220.5
        vmovsd    (%rbp,%rax,8), %xmm1                          #222.22
        vmovsd    (%r9,%rax,8), %xmm0                           #222.29
        vfmadd213sd (%r11,%rax,8), %xmm0, %xmm1                 #222.8
        vmovsd    %xmm1, (%rsi,%rax,8)                          #222.8
        jmp       ..B6.33       # Prob 100%                     #222.8
                                # LOE rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 eax
..B6.28:                        # Preds ..B6.3 ..B6.5 ..B6.7
                                # Execution count [5.00e-01]
        movl      %r8d, %ebx                                    #220.5
        xorl      %ecx, %ecx                                    #220.5
        movl      $1, %eax                                      #220.5
        xorl      %edx, %edx                                    #220.5
        shrl      $1, %ebx                                      #220.5
        je        ..B6.32       # Prob 9%                       #220.5
                                # LOE rdx rcx rbx rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 eax r8d
..B6.30:                        # Preds ..B6.28 ..B6.30
                                # Execution count [1.25e+00]
        vmovsd    (%rdx,%rbp), %xmm1                            #222.22
        incq      %rcx                                          #220.5
        vmovsd    (%rdx,%r9), %xmm0                             #222.29
        vfmadd213sd (%rdx,%r11), %xmm0, %xmm1                   #222.8
        vmovsd    %xmm1, (%rdx,%rsi)                            #222.8
        vmovsd    8(%rdx,%rbp), %xmm3                           #222.22
        vmovsd    8(%rdx,%r9), %xmm2                            #222.29
        vfmadd213sd 8(%rdx,%r11), %xmm2, %xmm3                  #222.8
        vmovsd    %xmm3, 8(%rdx,%rsi)                           #222.8
        addq      $16, %rdx                                     #220.5
        cmpq      %rbx, %rcx                                    #220.5
        jb        ..B6.30       # Prob 63%                      #220.5
                                # LOE rdx rcx rbx rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 r8d
..B6.31:                        # Preds ..B6.30
                                # Execution count [4.50e-01]
        lea       1(%rcx,%rcx), %eax                            #222.8
                                # LOE rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 eax r8d
..B6.32:                        # Preds ..B6.31 ..B6.28
                                # Execution count [5.00e-01]
        lea       -1(%rax), %edx                                #220.5
        cmpl      %r8d, %edx                                    #220.5
        jae       ..B6.34       # Prob 9%                       #220.5
                                # LOE rbp rsi rdi r9 r10 r11 r12 r13 r14 r15 eax
..1.22_0.TAG.0:
..B6.33:                        # Preds ..B6.25 ..B6.13 ..B6.32
                                # Execution count [4.50e-01]
        movslq    %eax, %rax                                    #220.5
        vmovsd    -8(%rbp,%rax,8), %xmm1                        #222.22
        vmovsd    -8(%r9,%rax,8), %xmm0                         #222.29
        vfmadd213sd -8(%r11,%rax,8), %xmm0, %xmm1               #222.8
        vmovsd    %xmm1, -8(%rsi,%rax,8)                        #222.8
                                # LOE rdi r10 r12 r13 r14 r15
..B6.34:                        # Preds ..B6.1 ..B6.32 ..B6.12 ..B6.33
                                # Execution count [1.00e+00]
        rdtsc                                                   #224.0
        movq      %rdx, %rsi                                    #224.0
        movq      %rax, %rbp                                    #224.0
        cpuid                                                   #224.0
        shlq      $32, %rsi                                     #224.5
        shlq      $32, %r15                                     #224.5
        orq       %rbp, %rsi                                    #224.5
        orq       %rdi, %r15                                    #224.5
        subq      %r15, %rsi                                    #224.5
        movq      %rsi, (%r10)                                  #224.5
	.cfi_restore 6
        popq      %rbp                                          #231.1
	.cfi_def_cfa_offset 24
	.cfi_restore 15
        popq      %r15                                          #231.1
	.cfi_def_cfa_offset 16
	.cfi_restore 3
        popq      %rbx                                          #231.1
	.cfi_def_cfa_offset 8
        ret                                                     #231.1
        .align    16,0x90
                                # LOE
	.cfi_endproc
# mark_end;
	.type	striad,@function
	.size	striad,.-striad
..LNstriad.5:
	.section .rodata, "a"
	.align 8
.2.22_2.switchtab.6:
	.quad	..1.22_0.TAG.0
	.quad	..1.22_0.TAG.1
	.quad	..1.22_0.TAG.2
	.quad	..1.22_0.TAG.3
	.quad	..1.22_0.TAG.4
	.quad	..1.22_0.TAG.5
	.quad	..1.22_0.TAG.6
	.data
# -- End  striad
	.text
.L_2__routine_start_sdaxpy_6:
# -- Begin  sdaxpy
	.text
# mark_begin;
       .align    16,0x90
	.globl sdaxpy
# --- sdaxpy(double *, double *, double *, int, uint64_t *)
sdaxpy:
# parameter 1: %rdi
# parameter 2: %rsi
# parameter 3: %rdx
# parameter 4: %ecx
# parameter 5: %r8
..B7.1:                         # Preds ..B7.0
                                # Execution count [1.00e+00]
	.cfi_startproc
..___tag_value_sdaxpy.74:
..L75:
                                                         #238.58
        pushq     %rbx                                          #238.58
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
        pushq     %rbp                                          #238.58
	.cfi_def_cfa_offset 24
	.cfi_offset 6, -24
        movl      %ecx, %ebp                                    #238.58
        movq      %rdx, %r9                                     #238.58
        movq      %rdi, %r10                                    #238.58
        rdtscp                                                  #244.0
        rdtsc                                                   #244.0
        cpuid                                                   #244.0
        rdtscp                                                  #244.0
        movq      %rdx, %r11                                    #244.0
        movq      %rax, %rdi                                    #244.0
        testl     %ebp, %ebp                                    #245.24
        jle       ..B7.32       # Prob 50%                      #245.24
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp
..B7.2:                         # Preds ..B7.1
                                # Execution count [0.00e+00]
        movslq    %ebp, %rdx                                    #238.1
        movq      %r10, %rax                                    #246.23
        shlq      $3, %rdx                                      #245.5
        subq      %rsi, %rax                                    #246.23
        cmpq      %rdx, %rax                                    #245.5
        jge       ..B7.4        # Prob 50%                      #245.5
                                # LOE rax rdx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp
..B7.3:                         # Preds ..B7.2
                                # Execution count [0.00e+00]
        negq      %rax                                          #246.16
        cmpq      %rdx, %rax                                    #245.5
        jl        ..B7.26       # Prob 50%                      #245.5
                                # LOE rdx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp
..B7.4:                         # Preds ..B7.2 ..B7.3
                                # Execution count [0.00e+00]
        movq      %r10, %rax                                    #246.30
        subq      %r9, %rax                                     #246.30
        cmpq      %rdx, %rax                                    #245.5
        jge       ..B7.6        # Prob 50%                      #245.5
                                # LOE rax rdx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp
..B7.5:                         # Preds ..B7.4
                                # Execution count [0.00e+00]
        negq      %rax                                          #246.16
        cmpq      %rdx, %rax                                    #245.5
        jl        ..B7.26       # Prob 50%                      #245.5
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp
..B7.6:                         # Preds ..B7.4 ..B7.5
                                # Execution count [5.00e-01]
        movl      %ebp, %ebx                                    #245.5
        xorl      %ecx, %ecx                                    #245.5
        movl      $1, %eax                                      #245.5
        xorl      %edx, %edx                                    #245.5
        shrl      $3, %ebx                                      #245.5
        je        ..B7.10       # Prob 9%                       #245.5
                                # LOE rdx rcx rbx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax ebp
..B7.8:                         # Preds ..B7.6 ..B7.8
                                # Execution count [3.12e-01]
        vmovsd    (%rdx,%rsi), %xmm1                            #246.23
        incq      %rcx                                          #245.5
        vmovsd    8(%rdx,%rsi), %xmm3                           #246.23
        vmovsd    16(%rdx,%rsi), %xmm5                          #246.23
        vmovsd    24(%rdx,%rsi), %xmm7                          #246.23
        vmovsd    32(%rdx,%rsi), %xmm9                          #246.23
        vmovsd    40(%rdx,%rsi), %xmm11                         #246.23
        vmovsd    48(%rdx,%rsi), %xmm13                         #246.23
        vmovsd    56(%rdx,%rsi), %xmm15                         #246.23
        vmovsd    (%rdx,%r9), %xmm0                             #246.30
        vmovsd    8(%rdx,%r9), %xmm2                            #246.30
        vmovsd    16(%rdx,%r9), %xmm4                           #246.30
        vmovsd    24(%rdx,%r9), %xmm6                           #246.30
        vmovsd    32(%rdx,%r9), %xmm8                           #246.30
        vmovsd    40(%rdx,%r9), %xmm10                          #246.30
        vmovsd    48(%rdx,%r9), %xmm12                          #246.30
        vmovsd    56(%rdx,%r9), %xmm14                          #246.30
        vfmadd213sd (%rdx,%r10), %xmm0, %xmm1                   #246.9
        vfmadd213sd 8(%rdx,%r10), %xmm2, %xmm3                  #246.9
        vfmadd213sd 16(%rdx,%r10), %xmm4, %xmm5                 #246.9
        vfmadd213sd 24(%rdx,%r10), %xmm6, %xmm7                 #246.9
        vfmadd213sd 32(%rdx,%r10), %xmm8, %xmm9                 #246.9
        vfmadd213sd 40(%rdx,%r10), %xmm10, %xmm11               #246.9
        vfmadd213sd 48(%rdx,%r10), %xmm12, %xmm13               #246.9
        vfmadd213sd 56(%rdx,%r10), %xmm14, %xmm15               #246.9
        vmovsd    %xmm1, (%rdx,%r10)                            #246.9
        vmovsd    %xmm3, 8(%rdx,%r10)                           #246.9
        vmovsd    %xmm5, 16(%rdx,%r10)                          #246.9
        vmovsd    %xmm7, 24(%rdx,%r10)                          #246.9
        vmovsd    %xmm9, 32(%rdx,%r10)                          #246.9
        vmovsd    %xmm11, 40(%rdx,%r10)                         #246.9
        vmovsd    %xmm13, 48(%rdx,%r10)                         #246.9
        vmovsd    %xmm15, 56(%rdx,%r10)                         #246.9
        addq      $64, %rdx                                     #245.5
        cmpq      %rbx, %rcx                                    #245.5
        jb        ..B7.8        # Prob 99%                      #245.5
                                # LOE rdx rcx rbx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp
..B7.9:                         # Preds ..B7.8
                                # Execution count [4.50e-01]
        lea       1(,%rcx,8), %eax                              #246.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax ebp
..B7.10:                        # Preds ..B7.9 ..B7.6
                                # Execution count [5.00e-01]
        cmpl      %ebp, %eax                                    #245.5
        ja        ..B7.32       # Prob 50%                      #245.5
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax ebp
..B7.11:                        # Preds ..B7.10
                                # Execution count [0.00e+00]
        lea       -1(%rax), %edx                                #238.1
        subl      %edx, %ebp                                    #238.1
        decl      %ebp                                          #238.1
        jmp       *.2.23_2.switchtab.5(,%rbp,8)                 #245.5
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax
..1.23_0.TAG.6:
..B7.13:                        # Preds ..B7.11
                                # Execution count [0.00e+00]
        cltq                                                    #246.9
        vmovsd    40(%rsi,%rax,8), %xmm1                        #246.23
        vmovsd    40(%r9,%rax,8), %xmm0                         #246.30
        vfmadd213sd 40(%r10,%rax,8), %xmm0, %xmm1               #246.9
        vmovsd    %xmm1, 40(%r10,%rax,8)                        #246.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax
..1.23_0.TAG.5:
..B7.15:                        # Preds ..B7.11 ..B7.13
                                # Execution count [0.00e+00]
        cltq                                                    #246.9
        vmovsd    32(%rsi,%rax,8), %xmm1                        #246.23
        vmovsd    32(%r9,%rax,8), %xmm0                         #246.30
        vfmadd213sd 32(%r10,%rax,8), %xmm0, %xmm1               #246.9
        vmovsd    %xmm1, 32(%r10,%rax,8)                        #246.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax
..1.23_0.TAG.4:
..B7.17:                        # Preds ..B7.11 ..B7.15
                                # Execution count [0.00e+00]
        cltq                                                    #246.9
        vmovsd    24(%rsi,%rax,8), %xmm1                        #246.23
        vmovsd    24(%r9,%rax,8), %xmm0                         #246.30
        vfmadd213sd 24(%r10,%rax,8), %xmm0, %xmm1               #246.9
        vmovsd    %xmm1, 24(%r10,%rax,8)                        #246.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax
..1.23_0.TAG.3:
..B7.19:                        # Preds ..B7.11 ..B7.17
                                # Execution count [0.00e+00]
        cltq                                                    #246.9
        vmovsd    16(%rsi,%rax,8), %xmm1                        #246.23
        vmovsd    16(%r9,%rax,8), %xmm0                         #246.30
        vfmadd213sd 16(%r10,%rax,8), %xmm0, %xmm1               #246.9
        vmovsd    %xmm1, 16(%r10,%rax,8)                        #246.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax
..1.23_0.TAG.2:
..B7.21:                        # Preds ..B7.11 ..B7.19
                                # Execution count [0.00e+00]
        cltq                                                    #246.9
        vmovsd    8(%rsi,%rax,8), %xmm1                         #246.23
        vmovsd    8(%r9,%rax,8), %xmm0                          #246.30
        vfmadd213sd 8(%r10,%rax,8), %xmm0, %xmm1                #246.9
        vmovsd    %xmm1, 8(%r10,%rax,8)                         #246.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax
..1.23_0.TAG.1:
..B7.23:                        # Preds ..B7.11 ..B7.21
                                # Execution count [0.00e+00]
        cltq                                                    #245.5
        vmovsd    (%rsi,%rax,8), %xmm1                          #246.23
        vmovsd    (%r9,%rax,8), %xmm0                           #246.30
        vfmadd213sd (%r10,%rax,8), %xmm0, %xmm1                 #246.9
        vmovsd    %xmm1, (%r10,%rax,8)                          #246.9
        jmp       ..B7.31       # Prob 100%                     #246.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax
..B7.26:                        # Preds ..B7.3 ..B7.5
                                # Execution count [5.00e-01]
        movl      %ebp, %ebx                                    #245.5
        xorl      %ecx, %ecx                                    #245.5
        movl      $1, %eax                                      #245.5
        xorl      %edx, %edx                                    #245.5
        shrl      $1, %ebx                                      #245.5
        je        ..B7.30       # Prob 9%                       #245.5
                                # LOE rdx rcx rbx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax ebp
..B7.28:                        # Preds ..B7.26 ..B7.28
                                # Execution count [1.25e+00]
        vmovsd    (%rdx,%rsi), %xmm1                            #246.23
        incq      %rcx                                          #245.5
        vmovsd    (%rdx,%r9), %xmm0                             #246.30
        vfmadd213sd (%rdx,%r10), %xmm0, %xmm1                   #246.9
        vmovsd    %xmm1, (%rdx,%r10)                            #246.9
        vmovsd    8(%rdx,%rsi), %xmm3                           #246.23
        vmovsd    8(%rdx,%r9), %xmm2                            #246.30
        vfmadd213sd 8(%rdx,%r10), %xmm2, %xmm3                  #246.9
        vmovsd    %xmm3, 8(%rdx,%r10)                           #246.9
        addq      $16, %rdx                                     #245.5
        cmpq      %rbx, %rcx                                    #245.5
        jb        ..B7.28       # Prob 63%                      #245.5
                                # LOE rdx rcx rbx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 ebp
..B7.29:                        # Preds ..B7.28
                                # Execution count [4.50e-01]
        lea       1(%rcx,%rcx), %eax                            #246.9
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax ebp
..B7.30:                        # Preds ..B7.29 ..B7.26
                                # Execution count [5.00e-01]
        lea       -1(%rax), %edx                                #245.5
        cmpl      %ebp, %edx                                    #245.5
        jae       ..B7.32       # Prob 9%                       #245.5
                                # LOE rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 eax
..1.23_0.TAG.0:
..B7.31:                        # Preds ..B7.23 ..B7.11 ..B7.30
                                # Execution count [4.50e-01]
        movslq    %eax, %rax                                    #245.5
        vmovsd    -8(%rsi,%rax,8), %xmm1                        #246.23
        vmovsd    -8(%r9,%rax,8), %xmm0                         #246.30
        vfmadd213sd -8(%r10,%rax,8), %xmm0, %xmm1               #246.9
        vmovsd    %xmm1, -8(%r10,%rax,8)                        #246.9
                                # LOE rdi r8 r11 r12 r13 r14 r15
..B7.32:                        # Preds ..B7.1 ..B7.30 ..B7.10 ..B7.31
                                # Execution count [1.00e+00]
        rdtsc                                                   #248.0
        movq      %rdx, %rsi                                    #248.0
        movq      %rax, %rbp                                    #248.0
        cpuid                                                   #248.0
        shlq      $32, %rsi                                     #248.5
        shlq      $32, %r11                                     #248.5
        orq       %rbp, %rsi                                    #248.5
        orq       %rdi, %r11                                    #248.5
        subq      %r11, %rsi                                    #248.5
        movq      %rsi, (%r8)                                   #248.5
	.cfi_restore 6
        popq      %rbp                                          #255.1
	.cfi_def_cfa_offset 16
	.cfi_restore 3
        popq      %rbx                                          #255.1
	.cfi_def_cfa_offset 8
        ret                                                     #255.1
        .align    16,0x90
                                # LOE
	.cfi_endproc
# mark_end;
	.type	sdaxpy,@function
	.size	sdaxpy,.-sdaxpy
..LNsdaxpy.6:
	.section .rodata, "a"
	.align 8
.2.23_2.switchtab.5:
	.quad	..1.23_0.TAG.0
	.quad	..1.23_0.TAG.1
	.quad	..1.23_0.TAG.2
	.quad	..1.23_0.TAG.3
	.quad	..1.23_0.TAG.4
	.quad	..1.23_0.TAG.5
	.quad	..1.23_0.TAG.6
	.data
# -- End  sdaxpy
	.text
.L_2__routine_start_init_7:
# -- Begin  init
	.text
# mark_begin;
       .align    16,0x90
	.globl init
# --- init(double *, double, int, uint64_t *)
init:
# parameter 1: %rdi
# parameter 2: %xmm0
# parameter 3: %esi
# parameter 4: %rdx
..B8.1:                         # Preds ..B8.0
                                # Execution count [1.00e+00]
	.cfi_startproc
..___tag_value_init.85:
..L86:
                                                         #72.46
        pushq     %rbx                                          #72.46
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
        movl      %esi, %r10d                                   #72.46
        movq      %rdx, %r8                                     #72.46
        movq      %rdi, %r9                                     #72.46
        rdtscp                                                  #75.0
        rdtsc                                                   #75.0
        cpuid                                                   #75.0
        rdtscp                                                  #75.0
        movq      %rdx, %rdi                                    #75.0
        movq      %rax, %rsi                                    #75.0
        testl     %r10d, %r10d                                  #76.24
        jle       ..B8.8        # Prob 50%                      #76.24
                                # LOE rbp rsi rdi r8 r9 r12 r13 r14 r15 r10d xmm0
..B8.2:                         # Preds ..B8.1
                                # Execution count [1.00e+00]
        movl      %r10d, %eax                                   #76.5
        xorl      %ecx, %ecx                                    #76.5
        movl      $1, %ebx                                      #76.5
        xorl      %edx, %edx                                    #76.5
        shrl      $1, %eax                                      #76.5
        je        ..B8.6        # Prob 9%                       #76.5
                                # LOE rax rdx rcx rbp rsi rdi r8 r9 r12 r13 r14 r15 ebx r10d xmm0
..B8.4:                         # Preds ..B8.2 ..B8.4
                                # Execution count [2.50e+00]
        incq      %rcx                                          #76.5
        vmovsd    %xmm0, (%rdx,%r9)                             #77.9
        vmovsd    %xmm0, 8(%rdx,%r9)                            #77.9
        addq      $16, %rdx                                     #76.5
        cmpq      %rax, %rcx                                    #76.5
        jb        ..B8.4        # Prob 63%                      #76.5
                                # LOE rax rdx rcx rbp rsi rdi r8 r9 r12 r13 r14 r15 r10d xmm0
..B8.5:                         # Preds ..B8.4
                                # Execution count [9.00e-01]
        lea       1(%rcx,%rcx), %ebx                            #77.9
                                # LOE rbp rsi rdi r8 r9 r12 r13 r14 r15 ebx r10d xmm0
..B8.6:                         # Preds ..B8.5 ..B8.2
                                # Execution count [1.00e+00]
        lea       -1(%rbx), %eax                                #76.5
        cmpl      %r10d, %eax                                   #76.5
        jae       ..B8.8        # Prob 9%                       #76.5
                                # LOE rbp rsi rdi r8 r9 r12 r13 r14 r15 ebx xmm0
..B8.7:                         # Preds ..B8.6
                                # Execution count [9.00e-01]
        movslq    %ebx, %rbx                                    #76.5
        vmovsd    %xmm0, -8(%r9,%rbx,8)                         #77.9
                                # LOE rbp rsi rdi r8 r12 r13 r14 r15
..B8.8:                         # Preds ..B8.6 ..B8.1 ..B8.7
                                # Execution count [1.00e+00]
        rdtsc                                                   #79.0
        movq      %rdx, %r10                                    #79.0
        movq      %rax, %r9                                     #79.0
        cpuid                                                   #79.0
        shlq      $32, %r10                                     #79.5
        shlq      $32, %rdi                                     #79.5
        orq       %r9, %r10                                     #79.5
        orq       %rsi, %rdi                                    #79.5
        subq      %rdi, %r10                                    #79.5
        movq      %r10, (%r8)                                   #79.5
	.cfi_restore 3
        popq      %rbx                                          #86.1
	.cfi_def_cfa_offset 8
        ret                                                     #86.1
        .align    16,0x90
                                # LOE
	.cfi_endproc
# mark_end;
	.type	init,@function
	.size	init,.-init
..LNinit.7:
	.data
# -- End  init
	.data
	.section .note.GNU-stack, ""
# End
