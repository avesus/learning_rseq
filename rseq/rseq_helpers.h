#ifndef _RSEQ_HELPERS_H_
#define _RSEQ_HELPERS_H_

#include <stdint.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <unistd.h>

#include "HEADER_INFO.h"

#include "rseq_defines.h"

#define RSEQ_SAFE_ACCESS(X) (*(__volatile__  __typeof__(X) *)&(X))
#define RSEQ_SAFE_WRITE(X, Y) RSEQ_SAFE_ACCESS(X) = (Y)

void
register_thread() {
    const uint32_t ret =
        syscall(NR_rseq, &__rseq_abi, sizeof(__rseq_abi), 0, RSEQ_SIGNATURE);
    // double initialization
    if (ret) {
        --rseq_refcount;
    }
}

void ALWAYS_INLINE
init_thread() {
    if (++rseq_refcount == 1) {
        register_thread();
    }
}

// current cpu
uint32_t ALWAYS_INLINE PURE_ATTR
get_cur_cpu() noexcept {
    return RSEQ_SAFE_ACCESS(__rseq_abi.cpu_id);
}

// cpu to start sequence on
uint32_t ALWAYS_INLINE PURE_ATTR
get_start_cpu() noexcept {
    return RSEQ_SAFE_ACCESS(__rseq_abi.cpu_id_start);
}

//////////////////////////////////////////////////////////////////////
// try to initialize before each call
uint32_t init_and_get_cur_cpu() noexcept {
    init_thread();
    return get_cur_cpu();
}

uint32_t init_and_get_start_cpu() noexcept {
    init_thread();
    return get_start_cpu();
}


//////////////////////////////////////////////////////////////////////
// check for unitialized return (this has not worked for me)
uint32_t get_cur_cpu_check() noexcept {
    const int32_t cur_cpu = get_cur_cpu();
    if(BRANCH_UNLIKELY(cur_cpu < 0)) {
        init_thread();
        return get_cur_cpu();
    }
    return cur_cpu;
}



uint32_t get_start_cpu_check() noexcept {
    const int32_t start_cpu = get_start_cpu();
    if(BRANCH_UNLIKELY(start_cpu < 0)) {
        init_thread();
        return get_start_cpu();
    }
    return start_cpu;
}



void ALWAYS_INLINE
clear_rseq() noexcept {
    RSEQ_SAFE_WRITE(__rseq_abi.ptr, 0);
}


#endif
