#include "logger_affinity.h"

/**
 * @brief worker_numとlogger_numを基にLoggerNodeのリストを初期化する
 * 
 * @param worker_num 初期化するworkerの数
 * @param logger_num 初期化するloggerの数
 */
void LoggerAffinity::init(unsigned worker_num, unsigned logger_num) {
    unsigned num_cpus = std::thread::hardware_concurrency();
    if (logger_num > num_cpus || worker_num > num_cpus) {
        std::cout << "too many threads" << std::endl;
    }
    // LoggerAffinityのworker_numとlogger_numにコピー
    worker_num_ = worker_num;
    logger_num_ = logger_num;
    
    for (unsigned i = 0; i < logger_num; i++) {
        nodes_.emplace_back();
    }
    unsigned thread_num = logger_num + worker_num;
    if (thread_num > num_cpus) {
        for (unsigned i = 0; i < worker_num; i++) {
            nodes_[i * logger_num/worker_num].worker_cpu_.emplace_back(i);
        }
        for (unsigned i = 0; i < logger_num; i++) {
            nodes_[i].logger_cpu_ = nodes_[i].worker_cpu_.back();
        }
    } else {
        for (unsigned i = 0; i < thread_num; i++) {
            nodes_[i * logger_num/thread_num].worker_cpu_.emplace_back(i);
        }
        for (unsigned i = 0; i < logger_num; i++) {
            nodes_[i].logger_cpu_ = nodes_[i].worker_cpu_.back();
            nodes_[i].worker_cpu_.pop_back();
        }
    }
}