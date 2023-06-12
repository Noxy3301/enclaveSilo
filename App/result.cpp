#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>

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

void printLine(int setw_length, std::vector<std::string> strings) {
    for (auto str : strings) {
        cout << std::setw(setw_length) << std::left << str;
    }
    cout << endl;
}

void Result::displayLocalDetailResult(const int thid) {
    int setw_length = 14;
    if (thid == 0) {
        printLine(setw_length, {"thread", "commit", "abort", "abort_VP1", "abort_VP2", "abort_VP3", "abort_bNULL"});
    }
    printLine(setw_length, {
        std::to_string(thid),
        std::to_string(local_commit_counts_), 
        std::to_string(local_abort_counts_),
        std::to_string(local_abort_by_validation1_),
        std::to_string(local_abort_by_validation2_),
        std::to_string(local_abort_by_validation3_),
        std::to_string(local_abort_by_null_buffer_)});
}

void Result::displayDetailResult() {
    uint64_t result = total_commit_counts_ / EXTIME;
    int setw_length = 30;
    printLine(setw_length, {"[info]\tcommit_counts_", std::to_string(total_commit_counts_)});
    printLine(setw_length, {"[info]\tabort_counts_", std::to_string(total_abort_counts_)});
    printLine(setw_length+2, {"[info]\t ├ abort_validation1", std::to_string(total_abort_by_validation1_)});
    printLine(setw_length+2, {"[info]\t ├ abort_validation2", std::to_string(total_abort_by_validation2_)});
    printLine(setw_length+2, {"[info]\t ├ abort_validation3", std::to_string(total_abort_by_validation3_)});
    printLine(setw_length+2, {"[info]\t └ abort_NULLbuffer", std::to_string(total_abort_by_null_buffer_)});
    printLine(setw_length, {"[info]\tabort_rate", std::to_string((double)total_abort_counts_ / (double)(total_commit_counts_ + total_abort_counts_))});
    printLine(setw_length, {"[info]\tlatency[ns]", std::to_string(powl(10.0, 9.0) / result * THREAD_NUM)});
    printLine(setw_length, {"[info]\tthroughput[tps]", std::to_string(result)});
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