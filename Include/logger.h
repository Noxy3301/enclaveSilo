#pragma once    // TODO:いる？

#include <mutex>
#include <condition_variable>   // 特定の条件を満たすまでthreadを待機させるためのクラス
#include <queue>
#include <unordered_map>
#include <unordered_set>        // indexがないset集合

// #include "log_writer.h"
// #include "log_buffer.h"
// #include "log_queue.h"
// #include "transaction_log.h"
// #include "notifier.h"


class LoggerNode {
    public:
        int logger_cpu_;
        std::vector<int> worker_cpu_;

        LoggerNode() {}
        // manual affinityはしないからstringを受け取る方はパス
};

// node_の中に、worker:loggerがn:1で入っている(即ちnode_の要素数はloggerの数)
class LoggerAffinity {
    public:
        std::vector<LoggerNode> nodes_;
        unsigned worker_num_ = 0;
        unsigned logger_num_ = 0;
        void init(unsigned worker_num, unsigned logger_num);
};

// class Logger {
//     private:
//         std::mutex mutex_;
//         std::thread thread_;
//         std::condition_variable cv_finish_;
//         bool joined_ = false;
//         std::size_t capacity_ = 1000;
//         unsigned int counter_ = 0;

//         void logging(bool quit);
//         void rotate_logfile(uint64_t epoch);
//         void show_result();

//     public:
//         int thid_;
//         std::vector<int> thid_vec_;
//         std::unordered_set<int> thid_set_;
//         LogQueue queue_;
//         PosixWriter logfile_;
//         std::string logdir_;
//         std::string logpath_;
//         std::uint64_t rotate_epoch_ = 100;
//         Notifier notifier_;
//         Notifier &notifier_stats_;
//         // NidStats nid_stats_;
//         NidBuffer nid_buffer_;
//         std::size_t byte_count_ = 0;
//         std::size_t write_count_ = 0;
//         std::size_t buffer_count_ = 0;
//         std::uint64_t wait_latency_ = 0;
//         std::uint64_t write_latency_ = 0;
//         std::uint64_t write_start_ = 0;
//         std::uint64_t write_end_ = 0;
//         // Stats depoch_diff_;

//         Logger(int i, Notifier &n) : thid_(i), notifier_stats_(n) {}

//         void add_tx_executor(TxExecutorLog &trans);
//         void worker();
//         void send_nid_to_notifier(std::uint64_t min_epoch, bool quit);
//         std::uint64_t find_min_epoch();
//         void wait_deq();
//         void worker_end(int thid);
//         void logger_end();
// };