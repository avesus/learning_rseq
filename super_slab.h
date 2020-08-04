#ifndef _SUPER_SLAB_H_
#define _SUPER_SLAB_H_

#include "obj_slab.h"


template<typename T,
         uint32_t nvecs        = 8,
         uint32_t inner_nvec   = 8,
         typename inner_slab_t = slab<T, inner_nvec>>
struct super_slab {
    uint64_t     available_obj_slabs[nvecs] __attribute__((aligned(64)));
    uint64_t     freed_obj_slabs[nvecs] __attribute__((aligned(64)));
    inner_slab_t inner_slabs[64 * nvecs];

    super_slab() = default; /*{
         memset(available_obj_slabs, ~0, nvecs * sizeof(uint64_t));
         memset(freed_obj_slabs, 0, nvecs * sizeof(uint64_t));
         }*/

    void
    _free(T * addr) {
        if (((uint64_t)addr) < ((uint64_t)(&inner_slabs[0]))) {
            __builtin_unreachable();
        }
        uint64_t pos = (((uint64_t)addr) - ((uint64_t)(&inner_slabs[0]))) /
                       sizeof(slab<T, inner_nvec>);
        (inner_slabs + pos)->_free(addr);
        __atomic_fetch_or(freed_obj_slabs + (pos / 64),
                          (1UL) << (pos % 64),
                          __ATOMIC_RELAXED);
    }
    uint64_t
    _allocate(const uint32_t start_cpu) {
        for (uint32_t i = 0; i < nvecs; ++i) {
            while (1) {
                while (~available_obj_slabs[i]) {
                    const uint32_t idx = _tzcnt_u64(~available_obj_slabs[i]);
                    const uint64_t ret =
                        (inner_slabs + 64 * i + idx)->_allocate(start_cpu);
                    if (ret > 0x1) {  // fast path of successful allocation
                        return ret;
                    }
                    else if (ret) {  // full
                        if (or_if_unset(available_obj_slabs + i,
                                        ((1UL) << idx),
                                        start_cpu)) {
                            return 0;
                        }
                        continue;
                    }
                    return 0;
                }
                if (freed_obj_slabs[i]) {
                    const uint64_t reclaimed_obj_slabs =
                        try_reclaim_all_free_slots(available_obj_slabs + i,
                                                   freed_obj_slabs + i,
                                                   start_cpu);
                    if (reclaimed_obj_slabs) {
                        __atomic_fetch_xor(freed_obj_slabs + i,
                                           reclaimed_obj_slabs,
                                           __ATOMIC_RELAXED);
                        continue;
                    }
                    return 0;
                }
                // continues will reset, if we ever faill through to here we
                // want to stop
                break;
            }
        }
        return 0x1;
    }
};

#endif
