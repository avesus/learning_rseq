#ifndef _SLAB_MANAGER_H_
#define _SLAB_MANAGER_H_

#include "obj_slab.h"
#include "rseq/rseq_base.h"
#include "super_slab.h"

#include <stdint.h>
#include <iostream>
#include <new>
#include <type_traits>
#include <typeinfo>


template<typename... Vals>
constexpr int32_t
get_N(int32_t n, Vals... vals) {
    int32_t temp[] = { static_cast<int32_t>(vals)... };
    return temp[n];
}


template<typename T, int32_t nlevels, int32_t level, int32_t... other_nvecs>
struct type_helper;


template<typename T, int32_t nlevel, int32_t... other_nvecs>
struct type_helper<T, nlevel, 0, other_nvecs...> {
    typedef slab<T, get_N(nlevel, other_nvecs...)> type;
};

template<typename T, int32_t nlevels, int32_t level, int32_t... other_nvecs>
struct type_helper {
    typedef super_slab<
        T,
        get_N(nlevels - level, other_nvecs...),
        get_N(nlevels - (level - 1), other_nvecs...),
        typename type_helper<T, nlevels, level - 1, other_nvecs...>::type>
        type;
};

template<typename T, int32_t levels, int32_t... level_nvecs>
struct slab_manager;

template<typename T, int32_t levels, int32_t... level_nvecs>
struct internal_slab_manager {
    using slab_t = typename slab_manager<T, levels, level_nvecs...>::slab_t;
    slab_t obj_slabs[8];
    internal_slab_manager() = default;
};

template<typename T, int32_t levels, int32_t... level_nvecs>
struct slab_manager {
    using slab_t =
        typename type_helper<T, levels - 1, levels - 1, level_nvecs...>::type;
    internal_slab_manager<T, levels, level_nvecs...> * m;

    slab_manager(const uint64_t base) {
        assert((base % 64) == 0);
        m = (internal_slab_manager<T, levels, level_nvecs...> *)base;
        new ((void * const)base)
            internal_slab_manager<T, levels, level_nvecs...>();
    }

    T *
    _allocate() {
        uint64_t ptr;
        do {
            const uint32_t start_cpu = get_start_cpu();
            ptr = m->obj_slabs[start_cpu]._allocate(start_cpu);
        } while (ptr == 0);
        return (T *)((ptr > 0x1) ? (ptr) : NULL);
    }
    void
    _free(T * addr) {
        if (((uint64_t)addr) < ((uint64_t)m)) {
            __builtin_unreachable();
        }
        const uint32_t from_cpu =
            (((uint64_t)addr) - ((uint64_t)m)) / sizeof(slab_t);
        if (from_cpu > 8) {
            __builtin_unreachable();
        }
        m->obj_slabs[from_cpu]._free(addr);
    }
};


#endif
