#pragma once
#include <vector>
#include <thread>
#include <iostream>

class LoggerNode {
public:
    int logger_cpu_;
    std::vector<int> worker_cpu_;

    /**
     * @brief LoggerNodeのコンストラクタ
     *        logger_cpu_とworker_cpu_をデフォルト初期化する
     */
    LoggerNode() : logger_cpu_(0), worker_cpu_() {}
};

class LoggerAffinity {
public:
    std::vector<LoggerNode> nodes_;
    unsigned worker_num_ = 0;
    unsigned logger_num_ = 0;
    
    void init(unsigned worker_num, unsigned logger_num);
};