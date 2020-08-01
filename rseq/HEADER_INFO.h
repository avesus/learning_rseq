#ifndef _CPP_ATTRIBUTES_H_
#define _CPP_ATTRIBUTES_H_

#define CACHE_LINE_SIZE 64

#define _UNREACHABLE_ __builtin_unreachable()
#define IMPOSSIBLE_VALUES(cond)                                                \
    {                                                                          \
        if (cond) {                                                            \
            _UNREACHABLE_;                                                     \
        }                                                                      \
    }

#define BRANCH_LIKELY(cond)   __builtin_expect(cond, 1)
#define BRANCH_UNLIKELY(cond) __builtin_expect(cond, 0)

#define PREFETCH(addr) __builtin_prefetch((const void * const)(addr))
#define PREFETCH_COND(cond, addr)                                              \
    {                                                                          \
        if constexpr (cond) {                                                  \
            __builtin_prefetch((const void * const)(addr));                    \
        }                                                                      \
    }

#define ALIGN_ATTR(alignment) __attribute__((aligned(alignment)))
#define PTR_ALIGNED_TO(addr, alignment)                                        \
    __builtin_assume_aligned(addr, alignment)


#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NEVER_INLINE  __attribute__((never_inline))


#define HOT_ATTR   __attribute__((hot))
#define COLD_ATTR  __attribute__((cold))
#define PURE_ATTR  __attribute__((pure))
#define CONST_ATTR __attribute__((const))


#endif
