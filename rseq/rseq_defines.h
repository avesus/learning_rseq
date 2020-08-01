#ifndef _RSEQ_DEFINES_H_
#define _RSEQ_DEFINES_H_

#include <stdint.h>

// for now these really need to be #defines
#define RSEQ_SIGNATURE 0x53053053
#define NR_rseq  334


enum rseq_cpu_id_state {
    RSEQ_CPU_ID_UNINITIALIZED       = -1,
    RSEQ_CPU_ID_REGISTRATION_FAILED = -2,
};

enum rseq_flags {
    RSEQ_FLAG_UNREGISTER = (1 << 0),
};

enum rseq_cs_flags_bit {
    RSEQ_CS_FLAG_NO_RESTART_ON_PREEMPT_BIT = 0,
    RSEQ_CS_FLAG_NO_RESTART_ON_SIGNAL_BIT  = 1,
    RSEQ_CS_FLAG_NO_RESTART_ON_MIGRATE_BIT = 2,
};

enum rseq_cs_flags {
    RSEQ_CS_FLAG_NO_RESTART_ON_PREEMPT =
        (1U << RSEQ_CS_FLAG_NO_RESTART_ON_PREEMPT_BIT),
    RSEQ_CS_FLAG_NO_RESTART_ON_SIGNAL =
        (1U << RSEQ_CS_FLAG_NO_RESTART_ON_SIGNAL_BIT),
    RSEQ_CS_FLAG_NO_RESTART_ON_MIGRATE =
        (1U << RSEQ_CS_FLAG_NO_RESTART_ON_MIGRATE_BIT),
};

struct _rseq_info {
    uint32_t version;
    uint32_t flags;
    uint64_t start_ip;
    uint64_t post_commit_offset;
    uint64_t abort_ip;
} __attribute__((aligned(32)));


struct _rseq_def {
    uint32_t cpu_id_start;
    uint32_t cpu_id;
    uint64_t ptr;
    uint32_t flags;
} __attribute__((aligned(32)));

typedef struct _rseq_def rseq_def;
typedef struct _rseq_info rseq_info;

__thread rseq_def __rseq_abi;
__thread uint32_t rseq_refcount;


#endif
