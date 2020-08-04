#ifndef _OBJ_SLAB_H_
#define _OBJ_SLAB_H_

#include <rseq/rseq_ops.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <x86intrin.h>

#define PRINT(...)  // fprintf(stderr, __VA_ARGS__);

void
massert(uint32_t b) {
    assert(b);
}

constexpr uint64_t inline __attribute__((always_inline)) next_p2(uint64_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}

enum align_policy { none = 0, up = 1, down = 2 };

template<typename T, uint32_t nvecs, align_policy ap>
constexpr uint64_t inline __attribute__((always_inline)) capacity() {
    if constexpr (ap == align_policy::none) {
        return 64 * nvecs;
    }
    else if constexpr (ap == align_policy::up) {
        uint64_t raw_size = 128 + 64 * nvecs * sizeof(T);
        return (next_p2(raw_size) - 128) / sizeof(T);
    }
    else {
        uint64_t raw_size = 128 + 64 * nvecs * sizeof(T);
        return (next_p2((raw_size >> 1) + 1) - 128) / sizeof(T);
    }
}


template<typename T, uint32_t nvecs, align_policy ap>
constexpr uint64_t inline __attribute__((always_inline)) calculate_nvecs() {
    if constexpr (ap == align_policy::none) {
        return nvecs;
    }
    else {
        return (capacity<T, nvecs, ap>() + 63) / 64;
    }
}


template<typename T, uint32_t nvecs, align_policy ap>
constexpr uint64_t inline __attribute__((always_inline)) final_vec_fullness() {
    return 64 -
           (64 * calculate_nvecs<T, nvecs, ap>() - capacity<T, nvecs, ap>());
}

template<typename T, uint32_t nvecs, align_policy ap>
constexpr uint64_t inline __attribute__((always_inline)) final_vec_init() {
    return final_vec_fullness<T, nvecs, ap>() < 64
               ? (((1UL) << final_vec_fullness<T, nvecs, ap>()) - 1)
               : (~(0UL));
}


template<typename T, uint32_t nvecs = 8, align_policy ap = align_policy::none> //, align_policy ap = align_policy::none>
struct slab {

    static constexpr const uint32_t true_nvecs = nvecs;

    uint64_t available_slots[true_nvecs] __attribute__((aligned(64)));
    uint64_t freed_slots[true_nvecs] __attribute__((aligned(64)));
    T        obj_arr[capacity<T, nvecs, ap>()];


    slab() = default; /*{
        memset(available_slots, ~0, (true_nvecs - 1) * sizeof(uint64_t));
        available_slots[true_nvecs - 1] = final_vec_init<T, nvecs, ap>();
        memset(freed_slots, 0, nvecs * sizeof(uint64_t));
        }*/


    void
    _free(T * addr) {
        if (((uint64_t)addr) < ((uint64_t)(&obj_arr[0]))) {
            __builtin_unreachable();
        }

        uint64_t pos =
            (((uint64_t)addr) - ((uint64_t)(&obj_arr[0]))) / sizeof(T);

        __atomic_fetch_or(freed_slots + (pos / 64),
                          ((1UL) << (pos % 64)),
                          __ATOMIC_RELAXED);
    }

    uint64_t
    _allocate(uint32_t start_cpu) {
        for (uint32_t i = 0; i < true_nvecs; ++i) {
            // try allocate
            if (~available_slots[i]) {
                PRINT("(1)Input[%d]: 0x%016lX\n", i, available_slots[i]);
                const uint32_t idx = _tzcnt_u64(~available_slots[i]);
                if (or_if_unset(available_slots + i,
                                ((1UL) << idx),
                                start_cpu)) {
                    return 0;
                }
                PRINT("(1)Output[%d][%d]: 0x%016lX\n",
                      i,
                      idx,
                      available_slots[i]);
                return ((uint64_t)&obj_arr[64 * i + idx]);
            }
            // try free
            else if (freed_slots[i]) {
                PRINT("(2)Input: 0x%016lX\n", freed_slots[i]);
                const uint64_t reclaimed_slots =
                    try_reclaim_free_slots(available_slots + i,
                                           freed_slots + i,
                                           start_cpu);
                PRINT("(2)Output: 0x%016lX\n", reclaimed_slots);
                if (reclaimed_slots) {
                    __atomic_fetch_xor(freed_slots + i,
                                       reclaimed_slots,
                                       __ATOMIC_RELAXED);
                    // absolutely do use return value from this
                    return ((uint64_t)(
                        &obj_arr[64 * i + _tzcnt_u64(reclaimed_slots)]));
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
