#pragma once

#include <cstdint>

#include "atomic_tool.h"
#include "procedure.h"
#include "random.h"
#include "tsc.h"

#include <time.h>

inline static bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold) {
    uint64_t diff = 0;
    diff = stop - start;
    if (diff > threshold) {
        return true;
    } else {
        return false;
    }
}

inline static void waitTime_ns(const uint64_t time) {
    uint64_t start = rdtscp();
    uint64_t end = 0;
    for (;;) {
        end = rdtscp();
        if (end - start > time * CLOCKS_PER_US) break;   // ns換算にしたいけど除算はコストが高そうなので1000倍して調整
    }
}

extern bool chkEpochLoaded();
extern void leaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop);
extern void makeProcedure(std::vector<Procedure> &pro, Xoroshiro128Plus &rnd);