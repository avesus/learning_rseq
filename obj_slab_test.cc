#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <unistd.h>
#include <string.h>

#include "rseq/rseq_base.h"
#include "obj_slab.h"

#define TEST_SIZE (1 << 16)
#define NTHREAD (2)

pthread_barrier_t b;

__thread uint32_t sum = 0;

uint64_t inline __attribute__((always_inline)) __attribute__((const))
get_cycles() {
    uint32_t hi, lo;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (((uint64_t)lo) | (((uint64_t)hi) << 32));
}


uint64_t true_sum = 0;
uint64_t true_time = 0;


void *
test_batch_alloc_free(void * targ) {
    const uint64_t EXPECTED_TOTAL = TEST_SIZE * NTHREAD;
    
    slab<uint64_t> ** s = (slab<uint64_t> **)targ;
    register_thread();
    pthread_barrier_wait(&b);

    uint64_t start, end;
    start = get_cycles();

    uint32_t active_allocs = 0;
    uint64_t * ptrs[64];
    uint32_t cpus[64];
    for(uint32_t i = 0; i < TEST_SIZE; i++) {


        uint32_t start_cpu;
        uint64_t p;
        do {
            start_cpu = get_start_cpu();
            p = s[start_cpu]->_allocate(start_cpu);
        } while (p == 0);
        if(!(p & 0x1)) {
            sum++;
            ptrs[active_allocs] = (uint64_t *)p;
            cpus[active_allocs] = start_cpu;
            if(++active_allocs == 64) {
                active_allocs = 0;
                for(uint32_t f_idx = 0; f_idx < 64; f_idx++) {
                    s[cpus[f_idx]]->_free(ptrs[f_idx]);
                }
            }

        }
    }
    
    end = get_cycles();
    __atomic_fetch_add(&true_sum, sum, __ATOMIC_RELAXED);
    __atomic_fetch_add(&true_time, end - start, __ATOMIC_RELAXED);
    return (void *)EXPECTED_TOTAL;
}


void *
test_alloc_free(void * targ) {
        const uint64_t EXPECTED_TOTAL = TEST_SIZE * NTHREAD;
    slab<uint64_t> ** s = (slab<uint64_t> **)targ;
    register_thread();
    pthread_barrier_wait(&b);

    uint64_t start, end;
    start = get_cycles();
    for(uint32_t i = 0; i < TEST_SIZE; i++) {

        uint64_t p;
        uint32_t start_cpu;
        do {
            start_cpu = get_start_cpu();
            p = s[start_cpu]->_allocate(start_cpu);
        } while (p == 0);
        if(!(p & 0x1)) {
            sum++;
            s[start_cpu]->_free((uint64_t *)p);
        }
    }
    
    end = get_cycles();
    __atomic_fetch_add(&true_sum, sum, __ATOMIC_RELAXED);
    __atomic_fetch_add(&true_time, end - start, __ATOMIC_RELAXED);
        return (void *)EXPECTED_TOTAL;
}

void *
test_alloc(void * targ) {
    const uint64_t EXPECTED_TOTAL = 8 * 8 * 64;
    slab<uint64_t> ** s = (slab<uint64_t> **)targ;
    register_thread();
    pthread_barrier_wait(&b);

    uint64_t start, end;
    start = get_cycles();
    for(uint32_t i = 0; i < TEST_SIZE; i++) {

        uint64_t p;
        uint32_t start_cpu;
        do {
            start_cpu = get_start_cpu();
            p = s[start_cpu]->_allocate(start_cpu);
        } while (p == 0);
        if(!(p & 0x1)) {
            sum++;
        }
    }
    
    end = get_cycles();
    __atomic_fetch_add(&true_sum, sum, __ATOMIC_RELAXED);
    __atomic_fetch_add(&true_time, end - start, __ATOMIC_RELAXED);
    return (void *)EXPECTED_TOTAL;
}



int
main(int argc, char ** argv) {
    pthread_barrier_init(&b, NULL, NTHREAD);
    
    slab<uint64_t> ** s = (slab<uint64_t> **)calloc(8, sizeof(slab<uint64_t> **));
    for(uint32_t i = 0; i < 8; i++) {
        s[i] = (slab<uint64_t> *)aligned_alloc(64, sizeof(slab<uint64_t>));
        memset(s[i], -1, 8 * sizeof(uint64_t));

    }
    
    pthread_t tids[NTHREAD];


    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, (1 << 16));
    pthread_attr_setguardsize(&attr, 0);
    
    for(uint32_t i = 0; i < NTHREAD; i++) {
        pthread_create(tids + i, &attr, &test_batch_alloc_free, (void *)(s));
    }

    uint64_t EXPECTED_TOTAL;
    for(uint32_t i = 0; i < NTHREAD; i++) {
        pthread_join(tids[i], (void **)(&EXPECTED_TOTAL));
    }
    fprintf(stderr, "Time: %lu -> %.4E\"cycles\"\n", true_time / NTHREAD, (double)(true_time / NTHREAD));
    fprintf(stderr, "Total: %lu == %lu\n", true_sum, EXPECTED_TOTAL);
    assert(true_sum == EXPECTED_TOTAL);
}
