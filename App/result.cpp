#include <iostream>
#include <vector>
#include <cmath>

#include "../Include/consts.h"
#include "../Include/result.h"

using namespace std;

void Result::displayAbortCounts() {
    cout << "abort_counts_:\t" << total_abort_counts_ << endl;
}

void Result::displayCommitCounts() {
    cout << "commit_counts_:\t" << total_commit_counts_ << endl;
}

void Result::displayAllResult() {
    displayCommitCounts();
    displayAbortCounts();
}

void Result::displayLocalDetailResult(const int thid) {
    cout << 
    "thread#"           << thid << 
    "\tcommit: "        << local_commit_counts_ << 
    "\tabort:"          << local_abort_counts_ << 
    "\tabort_VP1: "     << local_abort_by_validation1_ <<  // aborted by validation phase 1
    "\tabort_VP2: "     << local_abort_by_validation2_ <<  // aborted by validation phase 2
    "\tabort_VP3: "     << local_abort_by_validation3_ <<  // aborted by validation phase 3
    "\tabort_bNULL: "   << local_abort_by_null_buffer_ <<// aborted by NULL current buffer    
    endl;
}

void Result::displayDetailResult() {
    uint64_t result = total_commit_counts_ / EXTIME;

    cout << "[info]\tcommit_counts_:\t"     << total_commit_counts_ << endl;
    cout << "[info]\tabort_counts_:\t"      << total_abort_counts_ << endl;
    cout << "[info]\t-abort_validation1:\t" << total_abort_by_validation1_ << endl;
    cout << "[info]\t-abort_validation2:\t" << total_abort_by_validation2_ << endl;
    cout << "[info]\t-abort_validation3:\t" << total_abort_by_validation3_ << endl;
    cout << "[info]\t-abort_NULLbuffer:\t"  << total_abort_by_null_buffer_ << endl;
    cout << "[info]\tabort_rate:\t" << (double)total_abort_counts_ / (double)(total_commit_counts_ + total_abort_counts_) << endl;
    cout << "[info]\tlatency[ns]:\t" << powl(10.0, 9.0) / result * THREAD_NUM << endl;
    cout << "[info]\tthroughput[tps]:\t" << result << endl;
}

void Result::addLocalAbortCounts(const uint64_t count) {
    total_abort_counts_ += count;
}

void Result::addLocalCommitCounts(const uint64_t count) {
    total_commit_counts_ += count;
}

void Result::addLocalAbortDetailCounts(const Result &other) {
    total_abort_by_validation1_ += other.local_abort_by_validation1_;
    total_abort_by_validation2_ += other.local_abort_by_validation2_;
    total_abort_by_validation3_ += other.local_abort_by_validation3_;
    local_abort_by_null_buffer_ += other.local_abort_by_null_buffer_;
}

void Result::addLocalAllResult(const Result &other) {
    addLocalAbortCounts(other.local_abort_counts_);
    addLocalCommitCounts(other.local_commit_counts_);
    addLocalAbortDetailCounts(other);
}