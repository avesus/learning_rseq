/* SPDX-License-Identifier: LGPL-2.1-only OR MIT */
/*
 * rseq-x86.h
 *
 * (C) Copyright 2016-2018 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */
#include "HEADER_INFO.h"
#include "rseq_defines.h"
#include <stdint.h>


#define __rseq_str_1(x)	#x
#define __rseq_str(x)		__rseq_str_1(x)



/*
 * RSEQ_SIGNATURE is used with the following reserved undefined instructions, which
 * trap in user-space:
 *
 * x86-32:    0f b9 3d 53 30 05 53      ud1    0x53053053,%edi
 * x86-64:    0f b9 3d 53 30 05 53      ud1    0x53053053(%rip),%edi
 */


/*
 * Due to a compiler optimization bug in gcc-8 with asm goto and TLS asm input
 * operands, we cannot use "m" input operands, and rather pass the __rseq_abi
 * address through a "r" input operand.
 */

/* Offset of cpu_id and rseq_cs fields in struct rseq. */
#define RSEQ_CPU_ID_OFFSET	4
#define RSEQ_CS_OFFSET		8

#define rseq_barrier()		__asm__ __volatile__("" : : : "memory")
#define rseq_smp_mb()	\
	__asm__ __volatile__ ("lock; addl $0,-128(%%rsp)" ::: "memory", "cc")
#define rseq_smp_rmb()	rseq_barrier()
#define rseq_smp_wmb()	rseq_barrier()

#define rseq_smp_load_acquire(p)					\
__extension__ ({							\
	__typeof(*p) ____p1 = RSEQ_READ_ONCE(*p);			\
	rseq_barrier();							\
	____p1;								\
})

#define rseq_smp_acquire__after_ctrl_dep()	rseq_smp_rmb()

#define rseq_smp_store_release(p, v)					\
do {									\
	rseq_barrier();							\
	RSEQ_WRITE_ONCE(*p, v);						\
} while (0)

#define __RSEQ_ASM_DEFINE_TABLE(label, version, flags,			\
				start_ip, post_commit_offset, abort_ip)	\
		".pushsection __rseq_cs, \"aw\"\n\t"			\
		".balign 64\n\t"					\
		__rseq_str(label) ":\n\t"				\
		".long " __rseq_str(version) ", " __rseq_str(flags) "\n\t" \
		".quad " __rseq_str(start_ip) ", " __rseq_str(post_commit_offset) ", " __rseq_str(abort_ip) "\n\t" \
		".popsection\n\t"					\
		".pushsection __rseq_cs_ptr_array, \"aw\"\n\t"		\
		".quad " __rseq_str(label) "b\n\t"			\
		".popsection\n\t"


#define RSEQ_ASM_DEFINE_TABLE(label, start_ip, post_commit_ip, abort_ip) \
	__RSEQ_ASM_DEFINE_TABLE(label, 0x0, 0x0, start_ip,		\
				(post_commit_ip - start_ip), abort_ip)

/*
 * Exit points of a rseq critical section consist of all instructions outside
 * of the critical section where a critical section can either branch to or
 * reach through the normal course of its execution. The abort IP and the
 * post-commit IP are already part of the __rseq_cs section and should not be
 * explicitly defined as additional exit points. Knowing all exit points is
 * useful to assist debuggers stepping over the critical section.
 */
#define RSEQ_ASM_DEFINE_EXIT_POINT(start_ip, exit_ip)			\
		".pushsection __rseq_exit_point_array, \"aw\"\n\t"	\
		".quad " __rseq_str(start_ip) ", " __rseq_str(exit_ip) "\n\t" \
		".popsection\n\t"

#define RSEQ_ASM_STORE_RSEQ_CS(label, cs_label, rseq_cs)		\
		"leaq " __rseq_str(cs_label) "(%%rip), %%rax\n\t"	\
		"movq %%rax, " __rseq_str(rseq_cs) "\n\t"		\
		__rseq_str(label) ":\n\t"

#define RSEQ_ASM_CMP_CPU_ID(cpu_id, current_cpu_id, label)		\
		"cmpl %[" __rseq_str(cpu_id) "], " __rseq_str(current_cpu_id) "\n\t" \
		"jnz " __rseq_str(label) "\n\t"

#define RSEQ_ASM_DEFINE_ABORT(label, teardown, abort_label)		\
		".pushsection __rseq_failure, \"ax\"\n\t"		\
		/* Disassembler-friendly signature: ud1 <sig>(%rip),%edi. */ \
		".byte 0x0f, 0xb9, 0x3d\n\t"				\
		".long " __rseq_str(RSEQ_SIGNATURE) "\n\t"			\
		__rseq_str(label) ":\n\t"				\
		teardown						\
		"jmp %l[" __rseq_str(abort_label) "]\n\t"		\
		".popsection\n\t"

#define RSEQ_ASM_DEFINE_CMPFAIL(label, teardown, cmpfail_label)		\
		".pushsection __rseq_failure, \"ax\"\n\t"		\
		__rseq_str(label) ":\n\t"				\
		teardown						\
		"jmp %l[" __rseq_str(cmpfail_label) "]\n\t"		\
		".popsection\n\t"

static inline __attribute__((always_inline))
int rseq_cmpeqv_storev(intptr_t *v, intptr_t expect, intptr_t newv, int cpu)
{
	__asm__ __volatile__ goto (
		RSEQ_ASM_DEFINE_TABLE(3, 1f, 2f, 4f) /* start, commit, abort */
		RSEQ_ASM_DEFINE_EXIT_POINT(1f, %l[cmpfail])
		/* Start rseq by storing table entry pointer into rseq_cs. */
		RSEQ_ASM_STORE_RSEQ_CS(1, 3b, RSEQ_CS_OFFSET(%[rseq_abi]))
		RSEQ_ASM_CMP_CPU_ID(cpu_id, RSEQ_CPU_ID_OFFSET(%[rseq_abi]), 4f)
		"cmpq %[v], %[expect]\n\t"
		"jnz %l[cmpfail]\n\t"
		/* final store */
		"movq %[newv], %[v]\n\t"
		"2:\n\t"
		RSEQ_ASM_DEFINE_ABORT(4, "", abort)
		: /* gcc asm goto does not allow outputs */
		: [cpu_id]		"r" (cpu),
		  [rseq_abi]		"r" (&__rseq_abi),
		  [v]			"m" (*v),
		  [expect]		"r" (expect),
		  [newv]		"r" (newv)
		: "memory", "cc", "rax"
		: abort, cmpfail
	);
	return 0;
abort:
	return -1;
cmpfail:
	return 1;

}

/*
 * Compare @v against @expectnot. When it does _not_ match, load @v
 * into @load, and store the content of *@v + voffp into @v.
 */
static inline __attribute__((always_inline))
int rseq_cmpnev_storeoffp_load(intptr_t *v, intptr_t expectnot,
			       off_t voffp, intptr_t *load, int cpu)
{
	__asm__ __volatile__ goto (
		RSEQ_ASM_DEFINE_TABLE(3, 1f, 2f, 4f) /* start, commit, abort */
		RSEQ_ASM_DEFINE_EXIT_POINT(1f, %l[cmpfail])

		/* Start rseq by storing table entry pointer into rseq_cs. */
		RSEQ_ASM_STORE_RSEQ_CS(1, 3b, RSEQ_CS_OFFSET(%[rseq_abi]))
		RSEQ_ASM_CMP_CPU_ID(cpu_id, RSEQ_CPU_ID_OFFSET(%[rseq_abi]), 4f)
		"movq %[v], %%rbx\n\t"
		"cmpq %%rbx, %[expectnot]\n\t"
		"je %l[cmpfail]\n\t"
		"movq %%rbx, %[load]\n\t"
		"addq %[voffp], %%rbx\n\t"
		"movq (%%rbx), %%rbx\n\t"
		/* final store */
		"movq %%rbx, %[v]\n\t"
		"2:\n\t"
		RSEQ_ASM_DEFINE_ABORT(4, "", abort)
		: /* gcc asm goto does not allow outputs */
		: [cpu_id]		"r" (cpu),
		  [rseq_abi]		"r" (&__rseq_abi),
		  /* final store input */
		  [v]			"m" (*v),
		  [expectnot]		"r" (expectnot),
		  [voffp]		"er" (voffp),
		  [load]		"m" (*load)
		: "memory", "cc", "rax", "rbx"
		: abort, cmpfail

	);
	return 0;
abort:
	return -1;
cmpfail:
	return 1;

}

static inline __attribute__((always_inline))
int rseq_addv(intptr_t *v, intptr_t count, int cpu)
{
	__asm__ __volatile__ goto (
		RSEQ_ASM_DEFINE_TABLE(3, 1f, 2f, 4f) /* start, commit, abort */

		/* Start rseq by storing table entry pointer into rseq_cs. */
		RSEQ_ASM_STORE_RSEQ_CS(1, 3b, RSEQ_CS_OFFSET(%[rseq_abi]))
		RSEQ_ASM_CMP_CPU_ID(cpu_id, RSEQ_CPU_ID_OFFSET(%[rseq_abi]), 4f)
		/* final store */
		"addq %[count], %[v]\n\t"
		"2:\n\t"
		RSEQ_ASM_DEFINE_ABORT(4, "", abort)
		: /* gcc asm goto does not allow outputs */
		: [cpu_id]		"r" (cpu),
		  [rseq_abi]		"r" (&__rseq_abi),
		  /* final store input */
		  [v]			"m" (*v),
		  [count]		"er" (count)
		: "memory", "cc", "rax"
		: abort

	);
	return 0;
abort:
	return -1;
}

static inline __attribute__((always_inline))
int rseq_xorv(intptr_t *v, intptr_t count, int cpu)
{
	__asm__ __volatile__ goto (
		RSEQ_ASM_DEFINE_TABLE(3, 1f, 2f, 4f) /* start, commit, abort */

		/* Start rseq by storing table entry pointer into rseq_cs. */
		RSEQ_ASM_STORE_RSEQ_CS(1, 3b, RSEQ_CS_OFFSET(%[rseq_abi]))
		RSEQ_ASM_CMP_CPU_ID(cpu_id, RSEQ_CPU_ID_OFFSET(%[rseq_abi]), 4f)
		/* final store */
		"xor %[count], %[v]\n\t"
		"2:\n\t"
		RSEQ_ASM_DEFINE_ABORT(4, "", abort)
		: /* gcc asm goto does not allow outputs */
		: [cpu_id]		"r" (cpu),
		  [rseq_abi]		"r" (&__rseq_abi),
		  /* final store input */
		  [v]			"m" (*v),
		  [count]		"er" (count)
		: "memory", "cc", "rax"
		: abort

	);
	return 0;
abort:
	return -1;
}



static inline __attribute__((always_inline))
int rseq_cmpeqv_trystorev_storev(intptr_t *v, intptr_t expect,
				 intptr_t *v2, intptr_t newv2,
				 intptr_t newv, int cpu)
{
	__asm__ __volatile__ goto (
		RSEQ_ASM_DEFINE_TABLE(3, 1f, 2f, 4f) /* start, commit, abort */
		RSEQ_ASM_DEFINE_EXIT_POINT(1f, %l[cmpfail])

		/* Start rseq by storing table entry pointer into rseq_cs. */
		RSEQ_ASM_STORE_RSEQ_CS(1, 3b, RSEQ_CS_OFFSET(%[rseq_abi]))
		RSEQ_ASM_CMP_CPU_ID(cpu_id, RSEQ_CPU_ID_OFFSET(%[rseq_abi]), 4f)
		"cmpq %[v], %[expect]\n\t"
		"jnz %l[cmpfail]\n\t"
		/* try store */
		"movq %[newv2], %[v2]\n\t"
		/* final store */
		"movq %[newv], %[v]\n\t"
		"2:\n\t"
		RSEQ_ASM_DEFINE_ABORT(4, "", abort)
		: /* gcc asm goto does not allow outputs */
		: [cpu_id]		"r" (cpu),
		  [rseq_abi]		"r" (&__rseq_abi),
		  /* try store input */
		  [v2]			"m" (*v2),
		  [newv2]		"r" (newv2),
		  /* final store input */
		  [v]			"m" (*v),
		  [expect]		"r" (expect),
		  [newv]		"r" (newv)
		: "memory", "cc", "rax"
		: abort, cmpfail

	);
	return 0;
abort:
	return -1;
cmpfail:
	return 1;

}

/* x86-64 is TSO. */
static inline __attribute__((always_inline))
int rseq_cmpeqv_trystorev_storev_release(intptr_t *v, intptr_t expect,
					 intptr_t *v2, intptr_t newv2,
					 intptr_t newv, int cpu)
{
	return rseq_cmpeqv_trystorev_storev(v, expect, v2, newv2, newv, cpu);
}

static inline __attribute__((always_inline))
int rseq_cmpeqv_cmpeqv_storev(intptr_t *v, intptr_t expect,
			      intptr_t *v2, intptr_t expect2,
			      intptr_t newv, int cpu)
{
	__asm__ __volatile__ goto (
		RSEQ_ASM_DEFINE_TABLE(3, 1f, 2f, 4f) /* start, commit, abort */
		RSEQ_ASM_DEFINE_EXIT_POINT(1f, %l[cmpfail])

		/* Start rseq by storing table entry pointer into rseq_cs. */
		RSEQ_ASM_STORE_RSEQ_CS(1, 3b, RSEQ_CS_OFFSET(%[rseq_abi]))
		RSEQ_ASM_CMP_CPU_ID(cpu_id, RSEQ_CPU_ID_OFFSET(%[rseq_abi]), 4f)
		"cmpq %[v], %[expect]\n\t"
		"jnz %l[cmpfail]\n\t"
		"cmpq %[v2], %[expect2]\n\t"
		"jnz %l[cmpfail]\n\t"
		/* final store */
		"movq %[newv], %[v]\n\t"
		"2:\n\t"
		RSEQ_ASM_DEFINE_ABORT(4, "", abort)
		: /* gcc asm goto does not allow outputs */
		: [cpu_id]		"r" (cpu),
		  [rseq_abi]		"r" (&__rseq_abi),
		  /* cmp2 input */
		  [v2]			"m" (*v2),
		  [expect2]		"r" (expect2),
		  /* final store input */
		  [v]			"m" (*v),
		  [expect]		"r" (expect),
		  [newv]		"r" (newv)
		: "memory", "cc", "rax"
		: abort, cmpfail

	);
	return 0;
abort:
	return -1;
cmpfail:
	return 1;

}

static inline __attribute__((always_inline))
int rseq_cmpeqv_trymemcpy_storev(intptr_t *v, intptr_t expect,
				 void *dst, void *src, size_t len,
				 intptr_t newv, int cpu)
{
	uint64_t rseq_scratch[3];


	__asm__ __volatile__ goto (
		RSEQ_ASM_DEFINE_TABLE(3, 1f, 2f, 4f) /* start, commit, abort */
		RSEQ_ASM_DEFINE_EXIT_POINT(1f, %l[cmpfail])

		"movq %[src], %[rseq_scratch0]\n\t"
		"movq %[dst], %[rseq_scratch1]\n\t"
		"movq %[len], %[rseq_scratch2]\n\t"
		/* Start rseq by storing table entry pointer into rseq_cs. */
		RSEQ_ASM_STORE_RSEQ_CS(1, 3b, RSEQ_CS_OFFSET(%[rseq_abi]))
		RSEQ_ASM_CMP_CPU_ID(cpu_id, RSEQ_CPU_ID_OFFSET(%[rseq_abi]), 4f)
		"cmpq %[v], %[expect]\n\t"
		"jnz 5f\n\t"

		/* try memcpy */
		"test %[len], %[len]\n\t" \
		"jz 333f\n\t" \
		"222:\n\t" \
		"movb (%[src]), %%al\n\t" \
		"movb %%al, (%[dst])\n\t" \
		"inc %[src]\n\t" \
		"inc %[dst]\n\t" \
		"dec %[len]\n\t" \
		"jnz 222b\n\t" \
		"333:\n\t" \
		/* final store */
		"movq %[newv], %[v]\n\t"
		"2:\n\t"
		/* teardown */
		"movq %[rseq_scratch2], %[len]\n\t"
		"movq %[rseq_scratch1], %[dst]\n\t"
		"movq %[rseq_scratch0], %[src]\n\t"
		RSEQ_ASM_DEFINE_ABORT(4,
			"movq %[rseq_scratch2], %[len]\n\t"
			"movq %[rseq_scratch1], %[dst]\n\t"
			"movq %[rseq_scratch0], %[src]\n\t",
			abort)
		RSEQ_ASM_DEFINE_CMPFAIL(5,
			"movq %[rseq_scratch2], %[len]\n\t"
			"movq %[rseq_scratch1], %[dst]\n\t"
			"movq %[rseq_scratch0], %[src]\n\t",
			cmpfail)

		: /* gcc asm goto does not allow outputs */
		: [cpu_id]		"r" (cpu),
		  [rseq_abi]		"r" (&__rseq_abi),
		  /* final store input */
		  [v]			"m" (*v),
		  [expect]		"r" (expect),
		  [newv]		"r" (newv),
		  /* try memcpy input */
		  [dst]			"r" (dst),
		  [src]			"r" (src),
		  [len]			"r" (len),
		  [rseq_scratch0]	"m" (rseq_scratch[0]),
		  [rseq_scratch1]	"m" (rseq_scratch[1]),
		  [rseq_scratch2]	"m" (rseq_scratch[2])
		: "memory", "cc", "rax"
		: abort, cmpfail

	);
	return 0;
abort:
	return -1;
cmpfail:
	return 1;

}

/* x86-64 is TSO. */
static inline __attribute__((always_inline))
int rseq_cmpeqv_trymemcpy_storev_release(intptr_t *v, intptr_t expect,
					 void *dst, void *src, size_t len,
					 intptr_t newv, int cpu)
{
	return rseq_cmpeqv_trymemcpy_storev(v, expect, dst, src, len,
					    newv, cpu);
}

/*
 * Dereference @p. Add voffp to the dereferenced pointer, and load its content
 * into @load.
 */
static inline __attribute__((always_inline))
int rseq_deref_loadoffp(intptr_t *p, off_t voffp, intptr_t *load, int cpu)
{

	__asm__ __volatile__ goto (
		RSEQ_ASM_DEFINE_TABLE(3, 1f, 2f, 4f) /* start, commit, abort */

		/* Start rseq by storing table entry pointer into rseq_cs. */
		RSEQ_ASM_STORE_RSEQ_CS(1, 3b, RSEQ_CS_OFFSET(%[rseq_abi]))
		RSEQ_ASM_CMP_CPU_ID(cpu_id, RSEQ_CPU_ID_OFFSET(%[rseq_abi]), 4f)
		"movq %[p], %%rbx\n\t"

		"addq %[voffp], %%rbx\n\t"
		"movq (%%rbx), %%rbx\n\t"
		"movq %%rbx, %[load]\n\t"
		"2:\n\t"
		RSEQ_ASM_DEFINE_ABORT(4, "", abort)
		: /* gcc asm goto does not allow outputs */
		: [cpu_id]		"r" (cpu),
		  [rseq_abi]		"r" (&__rseq_abi),
		  /* final store input */
		  [p]			"m" (*p),
		  [voffp]		"er" (voffp),
		  [load]		"m" (*load)
		: "memory", "cc", "rax", "rbx"
		: abort
	);
	return 0;
abort:
	return -1;
}


