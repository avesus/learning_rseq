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

/*
    ".pushsection __rseq_cs, \"aw\"\n\t"    // creation section
    ".balign " #alignment"\n\t"             // alignment at least 32
    "3:\n\t"                                // struct info jump label
                                            // struct is rseq_info
    ".long 0x0\n"                           // version = 0
    ".long 0x0\n"                           // flags = 0
    ".quad 1f\n"                            // start_ip = 1f (label 1, forward)
    ".quad 2f - 1f\n"                       // post_commit_offset = (start_cs
                                               label - end_cs label)
    ".quad 4f\n"                            // abort label = 4f (label 4)
    ".popsection\n\t"                       // end section
*/


#define RSEQ_CS_ARR_DEF()                                                      \
    ".pushsection __rseq_cs_ptr_array, \"aw\"\n\t"                             \
    ".quad 3b\n\t"                                                             \
    ".popsection\n\t"

/*
    ".pushsection __rseq_cs_ptr_array, \"aw\"\n\t"  // create ptr section
    ".quad 3b\n\t"                                  // set ptr to addr of
                                                       rseq_info
    ".popsection\n\t"                               // end section
*/

#define RSEQ_PREP_CS_DEF()                                                     \
    "leaq 3b (%%rip), %%rax\n\t"                                               \
    "movq %%rax, 8(%[rseq_abi])\n\t"                                           \
    "1:\n\t"

/*
    "leaq 3b (%%rip), %%rax\n\t"        // get set for rseq_info struct
    "movq %%rax, 8(%[rseq_abi])\n\t"    // store in ptr field in __rseq_abi
    "1:\n\t"                            // start critical section label
*/

#define RSEQ_CMP_CUR_VS_START_CPUS()                                           \
    "cmpl %[start_cpu], 4(%[rseq_abi])\n\t"                                    \
    "jnz 4f\n\t"

/*
    "cmpl %[start_cpu], 4(%[rseq_abi])\n\t" // get cpu in 4(%[rseq_abi]) and
                                               compare to %[start_cpu] which is
                                               passed as param to function
    "jnz 4f\n\t"                            // if not equal abort (label 4)
*/

#define RSEQ_START_ABORT_DEF()                                                 \
    ".pushsection __rseq_failure, \"ax\"\n\t"                                  \
    ".byte 0x0f, 0xb9, 0x3d\n\t"                                               \
    ".long 0x53053053\n\t"                                                     \
    "4:\n\t"                                                                   \
    ""
/*
  ".pushsection __rseq_failure, \"ax\"\n\t" // create failure section
    ".byte 0x0f, 0xb9, 0x3d\n\t"            // not 100% sure on this, I think
                                               this in conjunction with .long
                                               0x53053053 is storing invalid
                                               operations to halt execution
    ".long 0x53053053\n\t"                  // see above
    "4:\n\t"                                // abort label
    ""                                      // not sure why this is needed
*/

#define RSEQ_END_ABORT_DEF() ".popsection\n\t"

/*
    ".popsection\n\t"   // end failure section
*/


/*
Type assembly will look like as follow:
foo(..., uint32_t start_cpu) 
    RSEQ_INFO_DEF(32) 
    RSEQ_CS_ARR_DEF() 
    RSEQ_PREP_CS_DEF()
    RSEQ_CMP_CUR_VS_START_CPUS()

    <actual critical section here>
    "2:\n\t" (this is end label of critical section)

    RSEQ_START_ABORT_DEF()
    <logical for abort here>
        // if this is goto generally jmp %l[abort]
        // otherwise some actual logic (usually set return var)
    RSEQ_END_ABORT_DEF()
    : <output variables, only if NOT goto asm>
    : <input variables> +
     [ start_cpu ] "r"(start_cpu), // required
     [ rseq_abi ] "g"(&__rseq_abi) // required
    : <clobber registers> +
      "memory", "cc", "rax" // minimum clobbers
    #ifdef IS_GOTO_ASM
    : <jump labels OUTSIDE of the inline asm>
    #endif
*/

#endif
