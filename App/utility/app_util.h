#pragma once

#include <iostream>
#include <cmath>
#include <vector>
#include <cassert>
#include <chrono>
#include <iomanip>

#include "../../Include/consts.h"

class DisplayResults {
public:
    DisplayResults() : workerResults(WORKER_NUM), loggerResults(LOGGER_NUM) {}

    void addTimestamp();
    void displayParameter();
    void displayResult();
    double calculateDurationTime_ms(size_t startTimeIndex, size_t endTimeIndex);
    void getWorkerResult();
    void getLoggerResult();

    uint64_t total_commit_count_ = 0;
    uint64_t total_abort_count_ = 0;
    uint64_t total_abort_vp1_count_ = 0;
    uint64_t total_abort_vp2_count_ = 0;
    uint64_t total_abort_vp3_count_ = 0;
    uint64_t total_abort_nullBuffer_count_ = 0;

    std::vector<std::chrono::system_clock::time_point> timestamps;
    std::vector<WorkerResult> workerResults;
    std::vector<LoggerResult> loggerResults;
};