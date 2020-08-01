#ifndef _RSEQ_VEV_H_
#define _RSEQ_VEV_H_
#include <rseq/rseq_base.h>
#include <rseq/rseq_ops.h>
#include <stdint.h>

struct rseq_int {
    uint64_t v[8 * 64];

    // using at&t syntax for now
    uint64_t
    fetch_add_getcpu(uint64_t _v, uint32_t * cpu) {
        return rseq_fetch_add_getcpu(v, _v, cpu);
    }

    uint64_t
    fetch_add(uint64_t _v) {
        return rseq_fetch_add(v, _v);
    }


    void
    add(uint64_t _v) {
        rseq_add(v, _v);
    }

    void
    add2(uint64_t _v) {
        rseq_ptr_add(v, &_v);
    }


    void
    incr() {
        rseq_incr(v);
    }

};

#endif
