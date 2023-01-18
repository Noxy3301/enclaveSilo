#pragma once

#include <vector>

#include "silo_op_element.h"
#include "procedure.h"
#include "log_buffer.h"
#include "notifier.h"

#include "../../Include/result.h"

enum class TransactionStatus : uint8_t {
    InFlight,
    Committed,
    Aborted,
};

class TxExecutor {
    public:
        std::vector<ReadElement> read_set_;
        std::vector<WriteElement> write_set_;
        std::vector<Procedure> pro_set_;

        std::vector<LogRecord> log_set_;
        LogHeader latest_log_header_;

        Result *sres_;

        TransactionStatus status_;
        unsigned int thid_;
        TIDword mrctid_;
        TIDword max_rset_, max_wset_;

        uint64_t write_val_;
        uint64_t return_val_;

        // TxExecutorLog
        ResultLog *sres_lg_;
        LogBufferPool log_buffer_pool_;
        NotificationId nid_;
        std::uint32_t nid_counter_ = 0; // Notification ID
        int logger_thid_;

        TxExecutor(int thid);

        void begin();
        void read(uint64_t key);
        void write(uint64_t key, uint64_t val = 0);
        ReadElement *searchReadSet(uint64_t key);
        WriteElement *searchWriteSet(uint64_t key);
        void unlockWriteSet();
        void unlockWriteSet(std::vector<WriteElement>::iterator end);
        void lockWriteSet();
        bool validationPhase();
        // TODO void wal(std::uint64_t ctid);
        void writePhase();
        void abort();
    
        void wal(std::uint64_t ctid);
        bool pauseCondition();
        void epochWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop);
        void durableEpochWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop, const bool &quit);

};