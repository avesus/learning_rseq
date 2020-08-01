#ifndef _RSEQ_ASM_DEFS_H_
#define _RSEQ_ASM_DEFS_H_

// label 1 -> begin criical section (include cpu comparison)
// label 2 -> end critical section
// label 3 -> rseq info strcut
// label 4 -> abort sequence

// defines the info struct used for control flow
#define RSEQ_INFO_DEF(alignment)                                               \
    ".pushsection __rseq_cs, \"aw\"\n\t"                                       \
    ".balign " #alignment                                                      \
    "\n\t"                                                                     \
    "3:\n\t"                                                                   \
    ".long 0x0\n"                                                              \
    ".long 0x0\n"                                                              \
    ".quad 1f\n"                                                               \
    ".quad 2f - 1f\n"                                                          \
    ".quad 4f\n"                                                               \
    ".popsection\n\t"


#define RSEQ_CS_ARR_DEF()                                                      \
    ".pushsection __rseq_cs_ptr_array, \"aw\"\n\t"                             \
    ".quad 3b\n\t"                                                             \
    ".popsection\n\t"

#define RSEQ_PREP_CS_DEF()                                                     \
    "leaq 3b (%%rip), %%rax\n\t"                                               \
    "movq %%rax, 8(%[rseq_abi])\n\t"                                           \
    "1:\n\t"

#define RSEQ_CMP_CUR_VS_START_CPUS()                                           \
    "cmpl %[start_cpu], 4(%[rseq_abi])\n\t"                                    \
    "jnz 4f\n\t"


#define RSEQ_START_ABORT_DEF()                                                 \
    ".pushsection __rseq_failure, \"ax\"\n\t"                                  \
    ".byte 0x0f, 0xb9, 0x3d\n\t"                                               \
    ".long 0x53053053\n\t"                                                     \
    "4:\n\t"                                                                   \
    ""

#define RSEQ_END_ABORT_DEF() ".popsection\n\t"

#endif
