/*
 * This plugin/module is the default one. It is an interactive
 * debugging engine.
 */

#include "../shiva.h"

void __attribute__((naked)) shakti_store_regs_x86_64(struct shiva_trace_regset_x86_64 *regs)
{
	__asm__ __volatile__(
		"movq %rax, (%rdi)\n\t"
		"movq %rbx, 8(%rdi)\n\t"
		"movq %rcx, 16(%rdi)\n\t"
		"movq %rdx, 24(%rdi)\n\t"
		"movq %rsi, 32(%rdi)\n\t"
		//"movq %rdi, 40(%rdi)\n\t"
		"movq %r8,  48(%rdi)\n\t"
		"movq %r9,  56(%rdi)\n\t"
		"movq %r10, 64(%rdi)\n\t"
		"movq %r11, 72(%rdi)\n\t"
		"movq %r12, 80(%rdi)\n\t"
		"movq %r13, 88(%rdi)\n\t"
		"movq %r14, 96(%rdi)\n\t"
		"movq %r15, 104(%rdi)\n\t"
		"ret\n\t"
		);
}

/*
 * This handler is being used to hook function calls within
 * the binary.
 */
void *
shakti_handler(void *arg)
{
	/*
	 * This function must always be called at the beginning of the handler
	 * to retrieve the register state. In the future users won't need to
	 * manually call this, it will be linked into the code at runtime.
	 */
	shakti_store_regs_x86_64(&ctx_global->regs.regset_x86_64);

#if 0
	printf("rdi: %p rsi %p rdx %p rcx %p r8 %p\n",
	    ctx_global->regs.regset_x86_64.rdi,
	    ctx_global->regs.regset_x86_64.rsi,
	    ctx_global->regs.regset_x86_64.rdx,
	    ctx_global->regs.regset_x86_64.rcx,
	    ctx_global->regs.regset_x86_64.r8);

	printf("arg1: %p arg2: %p arg3: %p arg4: %p\n",
		arg1, arg2, arg3, arg4);
#endif
	/*
	 * Get global pointer to the context.
	 */
	struct shiva_ctx *ctx = ctx_global;

	/*
	 * Currently we must manually use these in order to get
	 * the correct rbp and rip at the time this handler was
	 * called.
	 */
	void *retaddr = __builtin_return_address(0);
	void *frmaddr = __builtin_frame_address(1);
	struct shiva_trace_handler *handler;
	struct shiva_trace_bp *bp;
	uint64_t o_target;

	ctx->regs.regset_x86_64.rbp = (uint64_t)frmaddr;
	ctx->regs.regset_x86_64.rip = (uint64_t)retaddr - 5;
	ctx->regs.regset_x86_64.rdi = (uint64_t)arg;

	handler = shiva_trace_find_handler(ctx, &shakti_handler);
	if (handler == NULL) {
		printf("Failed to find handler struct for shakti_handler\n");
		exit(-1);
	}
	/*
	 * Get the shiva_trace_bp struct pointer that correlates
	 * to the call-hook-breakpoint. Then find the call-hook
	 * breakpoint that triggered our handler.
	 */
	TAILQ_FOREACH(bp, &handler->bp_tqlist,  _linkage) {
		if (bp->retaddr == (uint64_t)retaddr) {
			o_target = bp->o_target; /* get original call target address */
			
			/*
			 * Print the name of the function that was originally being
			 * called.
			 */
			printf("[CALL] %s\n", bp->call_target_symname);
			SHIVA_TRACE_CALL_ORIGINAL(bp);
		}
	}
	return NULL;
}

int
shakti_main(shiva_ctx_t *ctx)
{
	bool res;
	shiva_error_t error;
	shiva_callsite_iterator_t call_iter;
	struct shiva_branch_site branch;
	struct shiva_trace_handler trace_handler;
	uint64_t data = 0xdeadbeef;
	uint64_t out;

	res = shiva_trace(ctx, 0, SHIVA_TRACE_OP_ATTACH,
	    NULL, NULL, 0, &error);
	if (res == false) {
		printf("shiva_trace failed: %s\n", shiva_error_msg(&error));
		return -1;
	}
	res = shiva_trace_register_handler(ctx, (void *)&shakti_handler,
	    SHIVA_TRACE_BP_CALL, &error);
	if (res == false) {
		printf("shiva_register_handler failed: %s\n",
		    shiva_error_msg(&error));
		return -1;
	}
	shiva_callsite_iterator_init(ctx, &call_iter);
	while (shiva_callsite_iterator_next(&call_iter, &branch) == ELF_ITER_OK) {
		res = shiva_trace_set_breakpoint(ctx, (void *)&shakti_handler,
		    branch.branch_site + ctx->ulexec.base_vaddr, &error);
		if (res == false) {
			printf("shiva_trace_register_breakpoint failed: %s\n",
			    shiva_error_msg(&error));
			return -1;
		}
	}
	return 0;
}

