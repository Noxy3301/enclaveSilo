#pragma once

#include <stdint.h>
#include <stdio.h>
#include <vector>

#include "../../Include/cache_line_size.h"
#include "../../Include/consts.h"

class Result {
    public:
        uint64_t local_abort_counts_ = 0; //alignas(CACHE_LINE_SIZE) するかも
        uint64_t local_commit_counts_ = 0;
        uint64_t total_abort_counts_ = 0;
        uint64_t total_commit_counts_ = 0;

        void displayAbortCounts();
        void displayAbortRate();
        void displayCommitCounts();
        void displayTps(size_t extime, size_t thread_num);
        void displayAllResult(size_t clocks_per_us, size_t extime, size_t thread_num);

        void addLocalAllResult(const Result &other);
        void addLocalAbortCounts(const uint64_t count);
        void addLocalCommitCounts(const uint64_t count);

};

class ResultLog {
    public:
        Result result_;
        uint64_t local_bkpr_latency_ = 0;
        uint64_t local_txn_latency_ = 0;
        uint64_t local_wait_depoch_latency_ = 0;
        uint64_t local_publish_latency_ = 0;
        uint64_t local_publish_counts_ = 0;

        uint64_t total_bkpr_latency_ = 0;
        uint64_t total_txn_latency_ = 0;
        uint64_t total_wait_depoch_latency_ = 0;
        uint64_t total_publish_latency_ = 0;
        uint64_t total_publish_counts_ = 0;

        void displayAllResult(size_t clock_per_us, size_t extime, size_t thread_num);
        
        void displayBackPressureLatencyRate(size_t clocks_per_us);
        void displayWaitDEpochLatency(size_t clocks_per_us);
        void displayPublishLatency(size_t clocks_per_us);

        void addLocalAllResult(const ResultLog &other);

        void addLocalBkprLatency(const uint64_t count);
        void addLocalTxnLatency(const uint64_t count);
        void addLocalWaitDEpochLatency(const uint64_t count);
        void addLocalPublishLatency(const uint64_t count);
        void addLocalPublishCounts(const uint64_t count);
};

extern std::vector<ResultLog> SiloResult(THREAD_NUM);
extern void initResult();