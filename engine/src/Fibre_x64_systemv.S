.global FibreSwitch
.type FibreSwitch, @function

.global FibreSwitchNewJob
.type FibreSwitchNewJob, @function


FibreSwitch:
    # Save old fiber context. 
    # System V ABI: %rdi holds pointer to `old_context`, %rsi holds pointer to `new_context`
    movq %rsp, (%rdi)      # offset of fsp field = 0
    movq %rbx, 8(%rdi)     # offset of rbx field = 8
    movq %rbp, 16(%rdi)    # offset of rbp field = 16
    movq %r12, 24(%rdi)    # offset of r12 field = 24
    movq %r13, 32(%rdi)    # offset of r13 field = 32
    movq %r14, 40(%rdi)    # offset of r14 field = 40
    movq %r15, 48(%rdi)    # offset of r15 field = 48
    
    # Save return address so we can resume here later
    movq (%rsp), %rax
    movq %rax, 56(%rdi)    # offset of rip = 56

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
    pushq %rax # stack has to be 16 byte aligned at the point of a call instruction
               # so here we push 2 qwords instead of one

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
    

    # different from all subsequent switches we want to load 3 arguments
    # to pass to the job wrapper function.
    # on subsequent occasions we ignore the rdi, rdx and rsi , etc, fields of the ctx
    # struct because these are caller saved.
    movq 64(%rsi), %rdi
    movq 80(%rsi), %rdx
    movq 88(%rsi), %rcx
    

    # Push the new return address to the new stack so `ret` jumps to new %rip
    movq 56(%rsi), %rax
    pushq %rax
    pushq %rax # so here we push 2 qwords instead of one
    
    movq 72(%rsi), %rsi
    ret
