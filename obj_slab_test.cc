#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>
#include <new>

#include "obj_slab.h"
#include "rseq/rseq_base.h"
#include "super_slab.h"

#include "slab_manager.h"

const uint32_t os_nvec = 8;
#ifdef SUPER_SLAB
const uint32_t ss_nvec    = 1;
const uint64_t expec_mult = ss_nvec * 64;
typedef super_slab<uint64_t,
                   ss_nvec,
                   os_nvec,
                   slab<uint64_t, os_nvec, align_policy::none>>
    test_slab_t;
#else
const uint64_t                  expec_mult = 1;
typedef slab<uint64_t, os_nvec> test_slab_t;
#endif

#ifdef STD_MALLOC
    #define _allocate(X) malloc(sizeof(uint64_t))
    #define _free(X, Y)  free(Y)
#else
    #define _allocate(X) s[X]->_allocate(X)
    #define _free(X, Y)  s[X]->_free(Y)
#endif

#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))
#define MIN(X, Y) ((X) > (Y) ? (Y) : (X))

#define TEST_SIZE (1 << 17)
#define NTHREAD   (256)

pthread_barrier_t b;

__thread uint32_t sum = 0;

uint64_t inline __attribute__((always_inline)) __attribute__((const))
get_cycles() {
    uint32_t hi, lo;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (((uint64_t)lo) | (((uint64_t)hi) << 32));
}

uint64_t
ts_to_ns(struct timespec * ts) {
    return 1000UL * 1000UL * 1000UL * ts->tv_sec + ts->tv_nsec;
}

uint64_t
ts_ns_dif(struct timespec * ts_end, struct timespec * ts_start) {
    return ts_to_ns(ts_end) - ts_to_ns(ts_start);
}

uint64_t true_sum         = 0;
uint64_t true_time_cycles = 0;
uint64_t true_time_ns     = 0;


void *
test_batch_alloc_free(void * targ) {
    sum                           = 0;
    const uint64_t EXPECTED_TOTAL = TEST_SIZE * NTHREAD;

    test_slab_t ** s = (test_slab_t **)targ;
    init_thread();
    pthread_barrier_wait(&b);

    struct timespec ts_start, ts_end;
    uint64_t        start, end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    start = get_cycles();

    uint32_t       active_allocs = 0;
    const uint32_t batch_size =
        MAX(MIN(((expec_mult * os_nvec * 64) / NTHREAD), 128), 1);


    uint64_t * ptrs[batch_size];
    uint32_t   cpus[batch_size];
    for (uint32_t i = 0; i < TEST_SIZE; i++) {


        uint32_t start_cpu;
        uint64_t p;
        do {
            start_cpu = get_start_cpu();
            p         = (uint64_t)_allocate(start_cpu);
        } while (p == 0);
        if (p > 0x1) {
            sum++;
            ptrs[active_allocs] = (uint64_t *)p;
            cpus[active_allocs] = start_cpu;
            if (++active_allocs == batch_size) {
                active_allocs = 0;
                for (uint32_t free_idx = 0; free_idx < batch_size; ++free_idx) {
                    _free(cpus[free_idx], ptrs[free_idx]);
                }
            }
        }
    }

    end = get_cycles();
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    __atomic_fetch_add(&true_sum, sum, __ATOMIC_RELAXED);
    __atomic_fetch_add(&true_time_ns,
                       ts_ns_dif(&ts_end, &ts_start),
                       __ATOMIC_RELAXED);
    __atomic_fetch_add(&true_time_cycles, end - start, __ATOMIC_RELAXED);
    return (void *)EXPECTED_TOTAL;
}


void *
test_alloc_free(void * targ) {
    sum                           = 0;
    const uint64_t EXPECTED_TOTAL = TEST_SIZE * NTHREAD;
    test_slab_t ** s              = (test_slab_t **)targ;
    init_thread();
    pthread_barrier_wait(&b);

    struct timespec ts_start, ts_end;
    uint64_t        start, end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    start = get_cycles();
    for (uint32_t i = 0; i < TEST_SIZE; i++) {

        uint64_t p;
        uint32_t start_cpu;
        do {
            start_cpu = get_start_cpu();
            p         = (uint64_t)_allocate(start_cpu);
        } while (p == 0);
        if (p > 0x1) {
            sum++;
            _free(start_cpu, (uint64_t *)p);
        }
    }

    end = get_cycles();
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    __atomic_fetch_add(&true_sum, sum, __ATOMIC_RELAXED);
    __atomic_fetch_add(&true_time_ns,
                       ts_ns_dif(&ts_end, &ts_start),
                       __ATOMIC_RELAXED);
    __atomic_fetch_add(&true_time_cycles, end - start, __ATOMIC_RELAXED);
    return (void *)EXPECTED_TOTAL;
}

void *
test_alloc(void * targ) {
    sum = 0;

    // if threads < cores this could NOT fit full
    const uint64_t EXPECTED_TOTAL =
        MIN(MIN(8, NTHREAD) * os_nvec * 64 * expec_mult, NTHREAD * TEST_SIZE);
    test_slab_t ** s = (test_slab_t **)targ;
    init_thread();
    pthread_barrier_wait(&b);

    struct timespec ts_start, ts_end;
    uint64_t        start, end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    start = get_cycles();
    for (uint32_t i = 0; i < TEST_SIZE; i++) {

        uint64_t p;
        do {
            uint32_t start_cpu = get_start_cpu();
            p                  = (uint64_t)_allocate(start_cpu);
        } while (p == 0);
        if (p > 0x1) {
            sum++;
        }
    }

    end = get_cycles();
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    __atomic_fetch_add(&true_sum, sum, __ATOMIC_RELAXED);
    __atomic_fetch_add(&true_time_ns,
                       ts_ns_dif(&ts_end, &ts_start),
                       __ATOMIC_RELAXED);
    __atomic_fetch_add(&true_time_cycles, end - start, __ATOMIC_RELAXED);
    return (void *)EXPECTED_TOTAL;
}


#define NTEST_FUNCTIONS 3
void * (*test_functions[NTEST_FUNCTIONS])(void *) = { &test_alloc_free,
                                                      &test_batch_alloc_free,
                                                      &test_alloc };

static const char * function_names[NTEST_FUNCTIONS]{ "test_alloc_free",
                                                     "test_batch_alloc_free",
                                                     "test_alloc" };


int
main(int argc, char ** argv) {
    pthread_barrier_init(&b, NULL, NTHREAD);

    test_slab_t ** s = (test_slab_t **)calloc(8, sizeof(test_slab_t **));

    pthread_t tids[NTHREAD];

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, (1 << 16));
    pthread_attr_setguardsize(&attr, 0);

    for (uint32_t f_idx = 0; f_idx < NTEST_FUNCTIONS; f_idx++) {
        for (uint32_t i = 0; i < 8; i++) {
            s[i] = (test_slab_t *)aligned_alloc(64, sizeof(test_slab_t));
            memset(s[i], 0, 1024);
            new ((void * const)s[i]) test_slab_t();
        }


        for (uint32_t i = 0; i < NTHREAD; i++) {
            if (i < 8) {
                cpu_set_t cset;
                CPU_ZERO(&cset);
                CPU_SET(i, &cset);
                pthread_attr_t _attr;
                pthread_attr_init(&_attr);
                pthread_attr_setstacksize(&_attr, (1 << 16));
                pthread_attr_setguardsize(&_attr, 0);
                pthread_attr_setaffinity_np(&_attr, sizeof(cpu_set_t), &cset);
                pthread_create(tids + i,
                               &_attr,
                               test_functions[f_idx],
                               (void *)(s));
            }
            else {
                pthread_create(tids + i,
                               &attr,
                               test_functions[f_idx],
                               (void *)(s));
            }
        }

        uint64_t EXPECTED_TOTAL;
        for (uint32_t i = 0; i < NTHREAD; i++) {
            pthread_join(tids[i], (void **)(&EXPECTED_TOTAL));
        }

        fprintf(stderr, "%s ->\n\t", function_names[f_idx]);
        fprintf(stderr, "T = %d, N = %d\n\t", NTHREAD, TEST_SIZE);
        fprintf(stderr,
                "\tCycles       : %.4E\n\t",
                (double)(true_time_cycles / NTHREAD));
        fprintf(stderr,
                "\tCycles Per Op: %lu\n\t",
                (true_time_cycles / NTHREAD) / TEST_SIZE);
        fprintf(stderr,
                "\tnsec         : %.4E\n\t",
                (double)(true_time_ns / NTHREAD));
        fprintf(stderr,
                "\tnsec Per Op  : %lu\n\t",
                (true_time_ns / NTHREAD) / TEST_SIZE);

        fprintf(stderr, "%lu == %lu\n", true_sum, EXPECTED_TOTAL);
#ifndef STD_MALLOC
        assert(true_sum == EXPECTED_TOTAL);
#endif
        true_sum         = 0;
        true_time_cycles = 0;
        true_time_ns     = 0;
        for (uint32_t i = 0; i < 8; i++) {
            free(s[i]);
        }
    }
}
