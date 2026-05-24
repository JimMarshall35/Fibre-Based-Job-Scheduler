.global fiber_switch
.type fiber_switch, @function


FibreSwitch:
    # Save old fiber context. 
    # System V ABI: %rdi holds pointer to `old_context`, %rsi holds pointer to `new_context`
    movq %rsp, (%rdi)
    movq %rbx, 8(%rdi)
    movq %rbp, 16(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 32(%rdi)
    movq %r14, 40(%rdi)
    movq %r15, 48(%rdi)
    
    # Save return address so we can resume here later
    movq (%rsp), %rax
    movq %rax, 56(%rdi)

    # Restore new fiber context
    movq (%rsi), %rsp
    movq 8(%rsi), %rbx
    movq 16(%rsi), %rbp
    movq 24(%rsi), %r12
    movq 32(%rsi), %r13
    movq 40(%rsi), %r14
    movq 48(%rsi), %r15
    
    # Push the new return address to the new stack so `ret` jumps to new %rip
    movq 56(%rsi), %rax
    pushq %rax

    ret


FibreSwitchNewJob:
    # Save old fiber context. 
    # System V ABI: %rdi holds pointer to `old_context`, %rsi holds pointer to `new_context`
    movq %rsp, (%rdi)
    movq %rbx, 8(%rdi)
    movq %rbp, 16(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 32(%rdi)
    movq %r14, 40(%rdi)
    movq %r15, 48(%rdi)
    
    # Save return address so we can resume here later
    movq (%rsp), %rax
    movq %rax, 56(%rdi)

    # Restore new fiber context
    movq (%rsi), %rsp
    movq 8(%rsi), %rbx
    movq 16(%rsi), %rbp
    movq 24(%rsi), %r12
    movq 32(%rsi), %r13
    movq 40(%rsi), %r14
    movq 48(%rsi), %r15
    
    # Push the new return address to the new stack so `ret` jumps to new %rip
    movq 56(%rsi), %rax
    pushq %rax

    # different from all subsequent switches we want to load 3 arguments
    # to pass to the job wrapper function.
    # on subsequent occasions we ignore the rdi, rdx and rsi , etc, fields of the ctx
    # struct because these are caller saved.
    movq 64(%rsi), %rdi
    movq 80(%rsi), %rdx
    movq 88(%rsi), %rcx
    movq 72(%rsi), %rsi

    ret
