/*
            Copyright Oliver Kowalke 2009.
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENSE_1_0.txt or copy at
            http://www.boost.org/LICENSE_1_0.txt)
*/

/*
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* //////////////////////////////////////////////////////////////////////////////////////
 * implementation
 */

/**
 * translated from x64/context.asm by Alvin
 *
 * - support mingw/cygwin on windows
 */
#ifdef _WIN32

function(tb_context_make)

    // save the stack top to rax
    movq %rcx, %rax
    addq %rdx, %rax

    // reserve space for first argument(from) and retval(from) item of context-function
    subq $32, %rax

    // 16-align of the stack top address
    andq $-16, %rax

    // reserve space for context-data on context-stack
    subq $112, %rax

    // context.rbx = func
    movq %r8, 80(%rax)

    // save bottom address of context stack as 'limit'
    movq %rcx, 16(%rax)

    // save address of context stack limit as 'dealloction stack'
    movq %rcx, 8(%rax)

    // save top address of context stack as 'base'
    addq %rdx, %rcx
    movq %rcx, 24(%rax)

    // init fiber-storage to zero
    xorq %rcx, %rcx
    movq %rcx, (%rax)

    // init context.retval(saved) = a writeable space (unused)
    // it will write context (unused) and priv (unused) when jump to a new context function entry first
    leaq 128(%rax), %rcx
    movq %rcx, 96(%rax)

    // context.rip = the address of label __entry
    leaq __entry(%rip), %rcx
    movq %rcx, 104(%rax)

    // context.end = the address of label __end
    leaq __end(%rip), %rcx
    movq %rcx, 88(%rax)

    // return pointer to context-data
    ret

__entry:
    // patch return address (__end) on stack
    push %rbp

    // jump to the context function entry(rip)
    jmp *%rbx

__end:
    xorq %rcx, %rcx
    call _exit
    hlt

endfunc


function(tb_context_jump)

    // save the hidden argument: retval (from-context)
    pushq %rcx

    // save registers and construct the current context
    pushq %rbp
    pushq %rbx
    pushq %rsi
    pushq %rdi
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12

    // load TIB
    movq %gs:(0x30), %r10

    // save current stack base
    movq 0x08(%r10), %rax
    pushq %rax

    // save current stack limit
    movq 0x10(%r10), %rax
    pushq %rax

    // save current deallocation stack
    movq 0x1478(%r10), %rax
    pushq %rax

    // save fiber local storage
    movq 0x18(%r10), %rax
    pushq %rax

    // save the old context(esp) to r9
    movq %rsp, %r9

    // switch to the new context(esp) and stack
    movq %rdx, %rsp

    // load TIB
    movq %gs:(0x30), %r10

    // restore fiber local storage
    popq %rax
    movq %rax, 0x18(%r10)

    // restore deallocation stack
    popq %rax
    movq %rax, 0x1478(%r10)

    // restore stack limit
    popq %rax
    mov %rax, 0x10(%r10)

    // restore stack base
    popq %rax
    movq %rax, 0x08(%r10)

    // restore registers of the new context
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    popq %rdi
    popq %rsi
    popq %rbx
    popq %rbp

    // restore retval (saved) to rax
    popq %rax

    // restore the return or function address(r10)
    popq %r10

    // return from-context(retval: [rcx](context: r9, priv: r8)) from jump
    // it will write context (unused) and priv (unused) when jump to a new context function entry first
    movq %r9, (%rax)
    movq %r8, 8(%rax)

    movq %rax, %rcx

    // jump to the return or function address(rip)
    jmp *%r10

endfunc

#else

/* make context (refer to boost.context)
 *
 *             ------------------------------------------------------------------------------------------
 * stackdata: |                                                |         context                   |||||||
 *             ------------------------------------------------------------------------------------|-----
 *                                                                             (16-align for macosx)
 *
 *                                                       func     __end     __entry
 *             ------------------------------------------------------------------------------------------
 * context:   |   r12   |   r13   |   r14   |   r15   |   rbx   |   rbp   |   rip   | args | padding ... |
 *             ------------------------------------------------------------------------------------------
 *            0         8         16        24        32        40        48        56
 *                                                                                  |  16-align for macosx
 *                                                                                  |
 *                                                                       esp when jump to function
 *
 * @param stackdata     the stack data (rdi)
 * @param stacksize     the stack size (rsi)
 * @param func          the entry function (rdx)
 *
 * @return              the context pointer (rax)
 */
.global tb_context_make

tb_context_make:

    // save the stack top to rax
    addq %rsi, %rdi
    movq %rdi, %rax

    // 16-align for the stack top address
    movabs $-16, %r8
    andq %r8, %rax

    // reserve space for context-data on context-stack
    leaq -56(%rax), %rax

    // context.rbx = func
    movq %rdx, 32(%rax)

    // context.rip = the address of label __entry
    leaq __entry(%rip), %rcx
    movq %rcx, 48(%rax)

    // context.end = the address of label __end
    leaq __end(%rip), %rcx
    movq %rcx, 40(%rax)

    // return the context pointer
    ret

__entry:

    // pass old-context(context: rdi, priv: rsi) argument to the context function
    movq %rax, %rdi

    // patch __end
    push %rbp

    /* jump to the context function entry(rip)
     *
     *             -------------------------------
     * context: .. |   end   | args | padding ... |
     *             -------------------------------
     *             0         8
     *             | 16-align for macosx
     *            rsp
     */
    jmp *%rbx

__end:
    // exit(0)
    xorq %rdi, %rdi
// #ifdef TB_ARCH_ELF
    call _exit@PLT
// #else
//     call __exit
// #endif
    hlt

/* jump context (refer to boost.context)
 *
 * @param context       the to-context (rdi)
 * @param priv          the passed user private data (rsi)
 *
 * @return              the from-context (context: rax, priv: rdx)
 */
.global tb_context_jump

tb_context_jump:

    // save registers and construct the current context
    pushq %rbp
    pushq %rbx
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12

    // save the old context(rsp) to rax
    movq %rsp, %rax

    // switch to the new context(rsp) and stack
    movq %rdi, %rsp

    // restore registers of the new context
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    popq %rbx
    popq %rbp

    // restore the return or function address(rip)
    popq %r8

    // return from-context(context: rax, priv: rdx) from jump
    movq %rsi, %rdx

    /* jump to the return or function address(rip)
     *
     *              ---------------------
     * context: .. |  args | padding ... |
     *              ---------------------
     *             0       8
     *             | 16-align for macosx
     *            rsp
     */
    jmp *%r8

#endif
