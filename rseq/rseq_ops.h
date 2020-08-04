#ifndef _RSEQ_OPS_H_
#define _RSEQ_OPS_H_

#include <rseq/rseq_asm_defs.h>
#include <rseq/rseq_base.h>
#include <stdint.h>


template<uint32_t idx_spread = 64>
uint64_t
rseq_fetch_add_getcpu(uint64_t * v_begin, uint64_t _v, uint32_t * cpu) {
    uint32_t keep_going;
    uint32_t start_cpu;
    do {
        keep_going           = 0;
        start_cpu            = get_cur_cpu();
        uint64_t * v_cpu_ptr = v_begin + idx_spread * start_cpu;
        asm volatile(
                RSEQ_INFO_DEF(32)
                RSEQ_CS_ARR_DEF()
                RSEQ_PREP_CS_DEF()
                RSEQ_CMP_CUR_VS_START_CPUS()

                /* start critical section contents */
                "xaddq %[_v], (%[v_cpu_ptr])\n\t"
                "2:\n\t"  // post_commit_ip - start_ip
                /* end critical section contents */

                RSEQ_START_ABORT_DEF()
                "movl $1, %[keep_going]\n\t"
                "jmp 2b\n\t"
                RSEQ_END_ABORT_DEF()

                /* start output labels */
                : [ keep_going ] "+g"(keep_going), [ _v ] "+g"(_v)
                /* end output labels */

                /* start input labels */
                :
                [ start_cpu ] "r"(
                    start_cpu),  // anything but a register on this is a mistake
                [ rseq_abi ] "g"(&__rseq_abi),  // anything but a register on
                                                // this is a mistake
                [ v_cpu_ptr ] "g"(v_cpu_ptr)


                /* end input labels */
                : "memory", "cc", "rax");
    } while (keep_going);
    // success
    *cpu = start_cpu;
    return _v;
}

template<uint32_t idx_spread = 64>
uint64_t
rseq_fetch_add(uint64_t * v_begin, uint64_t _v) {
    uint32_t keep_going;
    do {
        keep_going           = 0;
        uint32_t   start_cpu = get_cur_cpu();
        uint64_t * v_cpu_ptr = v_begin + idx_spread * start_cpu;
        asm volatile(
                /* start prep instructions */
                RSEQ_INFO_DEF(32)
                RSEQ_INFO_DEF(32)
                RSEQ_CS_ARR_DEF()
                RSEQ_PREP_CS_DEF()
                RSEQ_CMP_CUR_VS_START_CPUS()

                /* start critical section contents */
                "xaddq %[_v], (%[v_cpu_ptr])\n\t"
                "2:\n\t"  // post_commit_ip - start_ip
                /* end critical section contents */

                RSEQ_START_ABORT_DEF()
                "movl $1, %[keep_going]\n\t"
                "jmp 2b\n\t"
                RSEQ_END_ABORT_DEF()

                /* start output labels */
                : [ keep_going ] "+g"(keep_going), [ _v ] "+g"(_v)
                /* end output labels */

                /* start input labels */
                :
                [ start_cpu ] "r"(
                    start_cpu),  // anything but a register on this is a mistake
                [ rseq_abi ] "g"(&__rseq_abi),  // anything but a register on
                                                // this is a mistake
                [ v_cpu_ptr ] "g"(v_cpu_ptr)


                /* end input labels */
                : "memory", "cc", "rax");

    } while (keep_going);
    // success
    return _v;
}

uint32_t
try_rseq_add(uint64_t * v_cpu_ptr, uint64_t _v, uint32_t start_cpu) {
    asm volatile goto(
        RSEQ_INFO_DEF(32) RSEQ_CS_ARR_DEF() RSEQ_PREP_CS_DEF()
            RSEQ_CMP_CUR_VS_START_CPUS()

        /* start critical section contents */
        "addq %[_v], (%[v_cpu_ptr])\n\t"
        "2:\n\t"  // post_commit_ip - start_ip
        /* end critical section contents */

        RSEQ_START_ABORT_DEF() "jmp %l[abort]\n\t" RSEQ_END_ABORT_DEF()

        /* start output labels */
        :
        /* end output labels */

        /* start input labels */
        : [ start_cpu ] "r"(
              start_cpu),  // anything but a register on this is a mistake
          [ rseq_abi ] "g"(&__rseq_abi),  // anything but a register on
                                          // this is a mistake
          [ v_cpu_ptr ] "g"(v_cpu_ptr),
          [ _v ] "g"(_v)


        /* end input labels */
        : "memory", "cc", "rax"
        : abort);
    return 0;
abort:
    return 1;
}

template<uint32_t idx_spread = 64>
void
rseq_add(uint64_t * v_begin, uint64_t _v) {
    uint32_t start_cpu;
    do {
        start_cpu = get_start_cpu();
    } while (try_rseq_add(v_begin + idx_spread * start_cpu, _v, start_cpu));
}


uint32_t
try_rseq_ptr_add(uint64_t * v_cpu_ptr, uint64_t * _v, uint32_t start_cpu) {
    asm volatile goto(
        RSEQ_INFO_DEF(32) RSEQ_CS_ARR_DEF() RSEQ_PREP_CS_DEF()
            RSEQ_CMP_CUR_VS_START_CPUS()

        /* start critical section contents */
        "movq (%[_v]), %%rcx\n\t"
        "salq $1, %%rcx\n\t"
        "addq %%rcx, (%[v_cpu_ptr])\n\t"
        "2:\n\t"  // post_commit_ip - start_ip
        /* end critical section contents */

        RSEQ_START_ABORT_DEF() "jmp %l[abort]\n\t" RSEQ_END_ABORT_DEF()

        /* start output labels */
        :
        /* end output labels */

        /* start input labels */
        : [ start_cpu ] "r"(
              start_cpu),  // anything but a register on this is a mistake
          [ rseq_abi ] "g"(&__rseq_abi),  // anything but a register on
                                          // this is a mistake
          [ v_cpu_ptr ] "g"(v_cpu_ptr),
          [ _v ] "g"(_v)


        /* end input labels */
        : "memory", "cc", "rax", "rcx"
        : abort);
    return 0;
abort:
    return 1;
}

template<uint32_t idx_spread = 64>
void
rseq_ptr_add(uint64_t * v_begin, uint64_t * _v) {
    uint32_t start_cpu;
    do {
        start_cpu = get_start_cpu();
    } while (try_rseq_ptr_add(v_begin + idx_spread * start_cpu, _v, start_cpu));
}


uint32_t
try_rseq_incr(uint64_t * v_cpu_ptr, uint32_t start_cpu) {
    asm volatile goto(
        RSEQ_INFO_DEF(32) RSEQ_CS_ARR_DEF() RSEQ_PREP_CS_DEF()
            RSEQ_CMP_CUR_VS_START_CPUS()
        /* start critical section contents */
        "incq (%[v_cpu_ptr])\n\t"
        "2:\n\t"  // post_commit_ip - start_ip
        /* end critical section contents */

        RSEQ_START_ABORT_DEF() "jmp %l[abort]\n\t" RSEQ_END_ABORT_DEF()

        /* start output labels */
        :
        /* end output labels */

        /* start input labels */
        : [ start_cpu ] "g"(
              start_cpu),  // anything but a register on this is a mistake
          [ rseq_abi ] "g"(&__rseq_abi),  // anything but a register on
                                          // this is a mistake
          [ v_cpu_ptr ] "g"(v_cpu_ptr)
        /* end input labels */
        : "memory", "cc", "rax"
        : abort);
    return 0;
abort:
    return 1;
}

template<uint32_t idx_spread = 64>
void
rseq_incr(uint64_t * v_begin) {
    uint32_t start_cpu;
    do {
        start_cpu = get_start_cpu();
    } while (try_rseq_incr(v_begin + idx_spread * start_cpu, start_cpu));
}
/*
uint64_t __attribute__((noinline))
try_reclaim_free_slots(uint64_t * v_cpu_ptr,
                       uint64_t * free_v_cpu_ptr,
                       uint32_t   start_cpu) {

    uint64_t unset_mask, idx;
    asm volatile(
        RSEQ_INFO_DEF(32) RSEQ_CS_ARR_DEF() RSEQ_PREP_CS_DEF()
            RSEQ_CMP_CUR_VS_START_CPUS()
        
        "movq (%[free_v_cpu_ptr]), %[unset_mask]\n\t"  // get current free vec
        "testq %[unset_mask], %[unset_mask]\n\t"              // if is 0 nothing to do
        "jz 2f\n\t"
        "tzcnt %[unset_mask], %[idx]\n\t"         // get free bit
        
        "leaq 1(%[idx]), %%rcx\n\t"
        "sarq %%cl, %[unset_mask]\n\t"
        "salq %%cl, %[unset_mask]\n\t"
        
        "xorq %[unset_mask], (%[v_cpu_ptr])\n\t"       // xor bits
        "2:\n\t"
        RSEQ_START_ABORT_DEF()
        "movq $0, %[unset_mask]\n\t"
        "jmp 2b\n\t"
        RSEQ_END_ABORT_DEF()
        : [ unset_mask ] "+r"(unset_mask), [idx] "+r" (idx)
        : [ start_cpu ] "r"(start_cpu), 
          [ rseq_abi ] "g"(&__rseq_abi),
          [ v_cpu_ptr ] "g"(v_cpu_ptr),
          [ free_v_cpu_ptr ] "g"(free_v_cpu_ptr)
        : "memory", "cc", "rax", "rcx");
    
    if (unset_mask) {
        // needs to be atomic here because other cores can access freed_slots
        // bit vec. (but only this core can unset hence why we can use rseq for
        // updating the available_slots bit vec
        __atomic_fetch_xor(free_v_cpu_ptr,
                           unset_mask | ((1UL) << idx),
                           __ATOMIC_RELAXED);
        return idx;
    }
    return 64;
}
*/

uint64_t __attribute__((noinline))
try_reclaim_free_slots(uint64_t * v_cpu_ptr,
                       uint64_t * free_v_cpu_ptr_then_temp,
                       uint32_t   start_cpu) {
    uint64_t ret_reclaimed_slots;
    asm volatile(
        RSEQ_INFO_DEF(32)
        RSEQ_CS_ARR_DEF()
        RSEQ_PREP_CS_DEF()
        RSEQ_CMP_CUR_VS_START_CPUS()
        
        "movq (%[free_v_cpu_ptr_then_temp]), %[ret_reclaimed_slots]\n\t"  // get current free vec
        "testq %[ret_reclaimed_slots], %[ret_reclaimed_slots]\n\t"              // if is 0 nothing to do
        "jz 2f\n\t"
        "leaq -1(%[ret_reclaimed_slots]), %[free_v_cpu_ptr_then_temp]\n\t" // now temp
        "andq %[ret_reclaimed_slots], %[free_v_cpu_ptr_then_temp]\n\t"
        
        "xorq %[free_v_cpu_ptr_then_temp], (%[v_cpu_ptr])\n\t"       // xor bits
        "2:\n\t"
        RSEQ_START_ABORT_DEF()
        "movq $0, %[ret_reclaimed_slots]\n\t"
        "jmp 2b\n\t"
        RSEQ_END_ABORT_DEF()
        : [ ret_reclaimed_slots ] "+g"(ret_reclaimed_slots)
        : [ start_cpu ] "g"(start_cpu), 
          [ rseq_abi ] "g"(&__rseq_abi),
          [ v_cpu_ptr ] "g"(v_cpu_ptr),
          [ free_v_cpu_ptr_then_temp ] "g"(free_v_cpu_ptr_then_temp)
        : "memory", "cc", "rax");
    return ret_reclaimed_slots;
}


uint64_t __attribute__((noinline))
try_reclaim_all_free_slots(uint64_t * v_cpu_ptr,
                           uint64_t * free_v_cpu_ptr,
                           uint32_t   start_cpu) {
    uint64_t ret_reclaimed_slots;
    asm volatile(
        RSEQ_INFO_DEF(32) RSEQ_CS_ARR_DEF() RSEQ_PREP_CS_DEF()
            RSEQ_CMP_CUR_VS_START_CPUS()
        
        "movq (%[free_v_cpu_ptr]), %[ret_reclaimed_slots]\n\t"  // get current free vec
        "testq %[ret_reclaimed_slots], %[ret_reclaimed_slots]\n\t"              // if is 0 nothing to do
        "jz 2f\n\t"
        "xorq %[ret_reclaimed_slots], (%[v_cpu_ptr])\n\t"       // xor bits
        "2:\n\t"
        RSEQ_START_ABORT_DEF()
        "movq $0, %[ret_reclaimed_slots]\n\t"
        "jmp 2b\n\t"
        RSEQ_END_ABORT_DEF()
        : [ ret_reclaimed_slots ] "+r"(ret_reclaimed_slots)
        : [ start_cpu ] "g"(start_cpu), 
          [ rseq_abi ] "g"(&__rseq_abi),
          [ v_cpu_ptr ] "g"(v_cpu_ptr),
          [ free_v_cpu_ptr ] "g"(free_v_cpu_ptr)
        : "memory", "cc", "rax");
    return ret_reclaimed_slots;
}


uint32_t
or_if_unset(uint64_t * v_cpu_ptr, uint64_t new_bit_mask, uint32_t start_cpu) {
    asm volatile goto(
        RSEQ_INFO_DEF(32) RSEQ_CS_ARR_DEF() RSEQ_PREP_CS_DEF()
            RSEQ_CMP_CUR_VS_START_CPUS()
        /* start critical section contents */
        "testq %[new_bit_mask], (%[v_cpu_ptr])\n\t"
        "jnz 4f\n\t"
        "orq %[new_bit_mask], (%[v_cpu_ptr])\n\t"
        "2:\n\t"  // post_commit_ip - start_ip
        /* end critical section contents */

        RSEQ_START_ABORT_DEF() "jmp %l[abort]\n\t" RSEQ_END_ABORT_DEF()

        /* start output labels */
        :
        /* end output labels */

        /* start input labels */
        : [ start_cpu ] "g"(start_cpu),
          [ new_bit_mask ] "g"(new_bit_mask),
          [ rseq_abi ] "g"(&__rseq_abi),
          [ v_cpu_ptr ] "g"(v_cpu_ptr)
        /* end input labels */
        : "memory", "cc", "rax"
        : abort);
    return 0;
abort:
    return 1;
}


uint32_t
xor_if_set(uint64_t * v_cpu_ptr, uint64_t new_bit_mask, uint32_t start_cpu) {
    asm volatile goto(
        RSEQ_INFO_DEF(32) RSEQ_CS_ARR_DEF() RSEQ_PREP_CS_DEF()
            RSEQ_CMP_CUR_VS_START_CPUS()
        /* start critical section contents */
        "testq %[new_bit_mask], (%[v_cpu_ptr])\n\t"
        "jz 4f\n\t"
        "xorq %[new_bit_mask], (%[v_cpu_ptr])\n\t"
        "2:\n\t"  // post_commit_ip - start_ip
        /* end critical section contents */

        RSEQ_START_ABORT_DEF() "jmp %l[abort]\n\t" RSEQ_END_ABORT_DEF()

        /* start output labels */
        :
        /* end output labels */

        /* start input labels */
        : [ start_cpu ] "g"(start_cpu),
          [ new_bit_mask ] "g"(new_bit_mask),
          [ rseq_abi ] "g"(&__rseq_abi),
          [ v_cpu_ptr ] "g"(v_cpu_ptr)
        /* end input labels */
        : "memory", "cc", "rax"
        : abort);
    return 0;
abort:
    return 1;
}

#endif
