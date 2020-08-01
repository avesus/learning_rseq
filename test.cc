#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <unistd.h>
#include "rseq_vec.h"

//__thread volatile rseq __rseq_abi;
__thread volatile uint64_t sum = 0;
__thread volatile uint64_t rseed = 0;

#define TEST_SIZE     ((1u) << 16)
#define MEM_ORDER     __ATOMIC_RELAXED
#define MEM_ORDER_CPP std::memory_order_relaxed
#define NTHREAD       (256UL)

uint64_t r = 1;

pthread_barrier_t b;

uint64_t inline __attribute__((always_inline)) __attribute__((const))
get_cycles() {
    uint32_t hi, lo;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (((uint64_t)lo) | (((uint64_t)hi) << 32));
}


void *
test_add_atomic(void * targ) {
    intptr_t * dst   = (intptr_t *)targ;
    uint64_t   start = 0, end = 0;
    register_thread();
    pthread_barrier_wait(&b);
    start = get_cycles();

    for (uint32_t i = 0; i < TEST_SIZE; ++i) {
        int cpu = RSEQ_SAFE_ACCESS(__rseq_abi.cpu_id_start);
        __atomic_fetch_add(dst + (cpu << 6), r, MEM_ORDER);
    }

    asm volatile("" : : "r,m"(dst) : "memory");
    end = get_cycles();
    /*        fprintf(stderr,
            "Cycles: %lu -> %.8E\n",
            end - start,
            (double)(end - start));*/
}


void *
test_or_rseq(void * targ) {
    rseed = rand();
    rseq_int * dst   = (rseq_int *)targ;
    intptr_t * ddst  = (intptr_t *)dst;
    uint64_t   start = 0, end = 0;
    register_thread();

    pthread_barrier_wait(&b);
    start = get_cycles();
    uint64_t t = 0;
    for (uint32_t i = 0; i < TEST_SIZE; ++i) {
        while (1) {
            uint32_t start_cpu = get_start_cpu();
            uint64_t _start     = *((uint64_t *)ddst + 64 * start_cpu);
            uint64_t _v = ((1UL) << (rseed % 64));
            rseed = 1103515245 * rseed + 12345;
                           t |= _v;
            if (!or_if_unset((uint64_t *)ddst + 64 * start_cpu,
                             _v,
                             start_cpu)) {
                uint64_t _end = *((uint64_t *)ddst + 64 * start_cpu);
                if (!(_start <= _end)) {
                    fprintf(stderr,
                            "Failure: 0x%016lX > 0x%016lX -> 0x%016lX\n",
                            _start,
                            _end,
                            _start & _end);
                }
                break;
            }
        }
    }

    asm volatile("" : : "r,m"(dst) : "memory");
    end = get_cycles();
    /*        fprintf(stderr,
            "Cycles: %lu -> %.8E\n",
            end - start,
            (double)(end - start));*/
}

void *
test_add1_rseq(void * targ) {
    rseq_int * dst   = (rseq_int *)targ;
    intptr_t * ddst  = (intptr_t *)dst;
    uint64_t   start = 0, end = 0;
    register_thread();

    pthread_barrier_wait(&b);
    start = get_cycles();

    for (uint32_t i = 0; i < TEST_SIZE; ++i) {
        dst->add2(r);
    }

    asm volatile("" : : "r,m"(dst) : "memory");
    end = get_cycles();
    /*        fprintf(stderr,
            "Cycles: %lu -> %.8E\n",
            end - start,
            (double)(end - start));*/
}

void *
test_add2_rseq(void * targ) {
    rseq_int * dst   = (rseq_int *)targ;
    intptr_t * ddst  = (intptr_t *)dst;
    uint64_t   start = 0, end = 0;
    register_thread();

    pthread_barrier_wait(&b);
    start = get_cycles();

    for (uint32_t i = 0; i < TEST_SIZE; ++i) {
        dst->incr();
    }

    asm volatile("" : : "r,m"(dst) : "memory");
    end = get_cycles();
    /*        fprintf(stderr,
            "Cycles: %lu -> %.8E\n",
            end - start,
            (double)(end - start));*/
}

void *
test_add3_rseq(void * targ) {
    rseq_int * dst   = (rseq_int *)targ;
    intptr_t * ddst  = (intptr_t *)dst;
    uint64_t   start = 0, end = 0;
    uint32_t   cpu = 0;
    register_thread();

    pthread_barrier_wait(&b);
    start = get_cycles();

    for (uint32_t i = 0; i < TEST_SIZE; ++i) {
        dst->fetch_add_getcpu(r, &cpu);
        dst->fetch_add(r);
    }

    asm volatile("" : : "r,m"(dst) : "memory");
    end = get_cycles();
    /*        fprintf(stderr,
            "Cycles: %lu -> %.8E\n",
            end - start,
            (double)(end - start));*/
}



int
main(int argc, char ** argv) {
    if(argc == 3) {
        r = 2;
    }
    pthread_barrier_init(&b, NULL, NTHREAD);

    uint64_t * atomic_cpus = (uint64_t *)calloc(8 * 64, 8);
    rseq_int * rseq_cpus   = (rseq_int *)calloc(sizeof(rseq_int), 1);

    pthread_t tids[NTHREAD];
    for (uint32_t i = 0; i < NTHREAD; i++) {
        if (argv[1][0] == 'A') {
            pthread_create(&tids[i], NULL, test_add2_rseq, (void *)rseq_cpus);
        }
        else if (argv[1][0] == 'B') {
            pthread_create(&tids[i], NULL, test_add1_rseq, (void *)rseq_cpus);
        }
        else if (argv[1][0] == 'C') {
            pthread_create(&tids[i], NULL, test_add3_rseq, (void *)rseq_cpus);
        }
        else if (argv[1][0] == 'D') {
            pthread_create(&tids[i], NULL, test_or_rseq, (void *)rseq_cpus);
        }
    }
    for (uint32_t i = 0; i < NTHREAD; i++) {
        pthread_join(tids[i], NULL);
    }

    intptr_t atomic_sum = 0, rseq_sum = 0;
    for (uint32_t i = 0; i < 8; i++) {
        atomic_sum += atomic_cpus[i];
        rseq_sum += rseq_cpus->v[i << 6];
    }

    for (uint32_t i = 0; i < 8; i++) {
        fprintf(stderr, "0x%016lX\n", rseq_cpus->v[i << 6]);
        if (rseq_cpus->v[i << 6] != (~(0UL))) {
            fprintf(stderr, "%d: NOT FULL\n", i);
        }
    }
    fprintf(stderr,
            "atomics: %lu == %lu\nrseq: %lu == %lu\n",
            atomic_sum,
            TEST_SIZE * NTHREAD,
            rseq_sum,
            TEST_SIZE * NTHREAD);
}
