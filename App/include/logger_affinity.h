#pragma once

#include <vector>

class LoggerNode {
    public:
        int logger_cpu_;
        std::vector<int> worker_cpu_;

        LoggerNode() {}
};

class LoggerAffinity {
    public:
        std::vector<LoggerNode> nodes_;
        unsigned worker_num_ = 0;
        unsigned logger_num_ = 0;
        void init(unsigned worker_num, unsigned logger_num);
};