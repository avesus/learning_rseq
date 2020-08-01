#ifndef _OBJ_SLAB_H_
#define _OBJ_SLAB_H_

#include <rseq/rseq_ops.h>

#include <stdint.h>
#include <stdio.h>
#include <x86intrin.h>

#define PRINT(...)  // fprintf(stderr, __VA_ARGS__);
template<typename T>
struct slab {
    uint64_t available_slots[8] __attribute__((aligned(64)));
    uint64_t freed_slots[8] __attribute__((aligned(64)));
    T        obj_arr[0];


    void
    _free(T * addr) {
        uint64_t pos =
            (((uint64_t)addr) - ((uint64_t)(&obj_arr[0]))) / sizeof(T);
        __atomic_fetch_or(freed_slots + (pos / 64),
                          ((1UL) << (pos % 64)),
                          __ATOMIC_RELAXED);
    }

    uint64_t
    _allocate(uint32_t start_cpu) {
        for (uint32_t i = 0; i < 8; i++) {
            // try allocate
            if (available_slots[i]) {
                PRINT("(1)Input: 0x%016lX\n", available_slots[i]);
                uint32_t idx = _tzcnt_u64(available_slots[i]);
                if (or_if_unset(available_slots + i,
                                ((1UL) << idx),
                                start_cpu)) {
                    return 0;
                }
                PRINT("(1)Output[%d]: 0x%016lX\n", idx, available_slots[i]);
                return ((uint64_t)&obj_arr[64 * i + idx]);
            }
            // try free
            else if (freed_slots[i]) {
                PRINT("(2)Input: 0x%016lX\n", freed_slots[i]);
                const uint64_t reclaimed_slots =
                    try_reclaim_free_slots(available_slots + i,
                                           freed_slots + i,
                                           start_cpu);
                PRINT("(2)Output[%lu]: 0x%016lX\n", idx, reclaimed_slots);
                if (reclaimed_slots) {
                    __atomic_fetch_xor(freed_slots + i,
                                       reclaimed_slots,
                                       __ATOMIC_RELAXED);
                    // absolutely do use return value from this
                    return ((uint64_t)(&obj_arr[64 * i + _tzcnt_u64(reclaimed_slots)]));
                }
                else {
                    return 0;
                }
            }
        }
        return 0x1;
    }
};

#endif
