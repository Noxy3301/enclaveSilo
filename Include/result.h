#pragma once

#include <cstdint>

class Result {
    public:
        uint64_t local_abort_counts_ = 0;
        uint64_t local_commit_counts_ = 0;
        uint64_t local_abort_by_validation1_ = 0;
        uint64_t local_abort_by_validation2_ = 0;
        uint64_t local_abort_by_validation3_ = 0;
        uint64_t local_abort_by_null_buffer_ = 0;


        uint64_t total_abort_counts_ = 0;
        uint64_t total_commit_counts_ = 0;
        uint64_t total_abort_by_validation1_ = 0;
        uint64_t total_abort_by_validation2_ = 0;
        uint64_t total_abort_by_validation3_ = 0;
        uint64_t total_abort_by_null_buffer_ = 0;


        void displayAbortCounts();
        void displayCommitCounts();
        void displayAllResult();

        void displayLocalDetailResult(const int thid);
        void displayDetailResult();

        void addLocalAbortCounts(const uint64_t count);
        void addLocalCommitCounts(const uint64_t count);

        void addLocalAbortDetailCounts(const Result &other);

        void addLocalAllResult(const Result &other);
};