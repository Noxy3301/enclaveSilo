#pragma once

#include <stdint.h>

#include "consts_benchmark.h"

class WorkerResult {
public:
    uint64_t local_commit_count_ = 0;
    uint64_t local_abort_count_ = 0;
    uint64_t local_abort_vp1_count_ = 0;
    uint64_t local_abort_vp2_count_ = 0;
    uint64_t local_abort_vp3_count_ = 0;
    uint64_t local_abort_nullBuffer_count_ = 0;
#ifdef ADD_ANALYSIS
    uint64_t local_read_time_ = 0;
    uint64_t local_read_internal_time_ = 0;
    uint64_t local_write_time_ = 0;
    uint64_t local_validation_time_ = 0;
    uint64_t local_write_phase_time_ = 0;
    uint64_t local_masstree_read_get_value_time_ = 0;
    uint64_t local_masstree_write_get_value_time_ = 0;
    uint64_t local_durable_epoch_work_time_ = 0;
    uint64_t local_make_procedure_time_ = 0;
#endif
};

class LoggerResult {
public:
    uint64_t byte_count_ = 0;
    uint64_t write_latency_ = 0;
    uint64_t wait_latency_ = 0;
};

enum AbortReason : uint8_t {
    ValidationPhase1,
    ValidationPhase2,
    ValidationPhase3,
    NullCurrentBuffer,
};

enum LoggerResultType : uint8_t {
    ByteCount,
    WriteLatency,
    WaitLatency,
};

enum class OpType : uint8_t {
    NONE,
    READ,
    WRITE,
    INSERT,
    DELETE,
    SCAN,
    RMW,
};

#define INDEX_USE_MASSTREE 0
#define INDEX_USE_OCH 1