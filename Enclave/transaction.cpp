#include <algorithm> // for sort function

#include "../Include/consts.h"

#include "include/atomic_tool.h"
#include "include/atomic_wrapper.h"
#include "include/transaction.h"
#include "include/log.h"
#include "include/util.h"

#include "Enclave_t.h"
#include "sgx_thread.h"

void TxExecutor::abort() {
    // remove inserted records
    // for (auto &we : write_set_) {
    //     if (we.op_ == OpType::INSERT) {
    //         Masstrees[get_storage(we.storage_)].remove_value(we.key_); //
    //         TODO: delete implementation delete we.rcdptr_;
    //     }
    // }

    read_set_.clear();
    write_set_.clear();
}

void TxExecutor::begin() {
    status_ = TransactionStatus::inFlight;
    max_wset_.obj_ = 0;
    max_rset_.obj_ = 0;
    nid_ = NotificationId(nid_counter_++, thid_, rdtscp()); // for logging
}

Status TxExecutor::insert(Storage s, std::string_view key, TupleBody &&body) {
#if ADD_ANALYSIS
    std::uint64_t start = rdtscp();
#endif

    if (searchWriteSet(s, key))
        return Status::WARN_ALREADY_EXISTS;

    //   Tuple* tuple = Masstrees[get_storage(s)].get_value(key);
    Tuple *tuple = Table[get_storage(s)].get(key);
    // #if ADD_ANALYSIS
    //   ++result_->local_tree_traversal_;
    // #endif
    if (tuple != nullptr) {
        return Status::WARN_ALREADY_EXISTS;
    }

    tuple = new Tuple();
    tuple->init(std::move(body));

    //   Status stat = Masstrees[get_storage(s)].insert_value(key, tuple);
    Status stat = Table[get_storage(s)].put(key, tuple, 1); // CHECK: OCHはTIDを使っているのか確認
    if (stat == Status::WARN_ALREADY_EXISTS) {
        delete tuple;
        return stat;
    }

    write_set_.emplace_back(s, key, tuple, OpType::INSERT);

    // #if ADD_ANALYSIS
    //   result_->local_write_latency_ += rdtscp() - start;
    // #endif
    return Status::OK;
}

void TxExecutor::lockWriteSet() {
    Tidword expected, desired;

    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
        if (itr->op_ == OpType::INSERT)
            continue;
        expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        for (;;) {
            if (expected.lock) {
#if NO_WAIT_LOCKING_IN_VALIDATION
                this->status_ =
                    TransactionStatus::aborted; // w-w conflictは即abort
                if (itr != write_set_.begin())
                    unlockWriteSet(itr);
                return;
#endif
            } else {
                desired = expected;
                desired.lock = 1;
                if (compareExchange((*itr).rcdptr_->tidword_.obj_,
                                    expected.obj_, desired.obj_))
                    break;
            }
        }
        max_wset_ = std::max(max_wset_, expected);
    }
}

// こっちのreadはindexからKeyをつかって対応するbodyを持ってきている
// readしたbodyの一貫性はread_internalで確認している
Status TxExecutor::read(Storage s, std::string_view key, TupleBody **body) {
    // these variable cause error (-fpermissive)
    // "crosses initialization of ..."
    // So it locate before first goto instruction.
    Tidword expected, check;
    Status stat;
    ReadElement<Tuple> *re;
    WriteElement<Tuple> *we;

    // read-own-writes or re-read from local read set
    re = searchReadSet(s, key);
    if (re) {
        *body = &(re->body_);
        goto FINISH_READ;
    }
    we = searchWriteSet(s, key);
    if (we) {
        *body = &(we->body_);
        goto FINISH_READ;
    }

    // Search tuple from data structure
    Tuple *tuple;
    tuple = Table[get_storage(s)].get(key);
    if (tuple == nullptr)
        return Status::WARN_NOT_FOUND;

    stat = read_internal(s, key, tuple);
    if (stat != Status::OK) {
        return stat;
    }
    *body = &(read_set_.back().body_);

FINISH_READ:
    return Status::OK;
}

// read_internalはbodyの一貫性はread_internalで確認している
Status TxExecutor::read_internal(Storage s, std::string_view key,
                                 Tuple *tuple) {
    TupleBody body;
    Tidword expected, check;

    //(a) reads the TID word, spinning until the lock is clear

    expected.obj_ = loadAcquire(tuple->tidword_.obj_);
    // check if it is locked.
    // spinning until the lock is clear

    for (;;) {
        while (expected.lock) {
            expected.obj_ = loadAcquire(tuple->tidword_.obj_);
        }

        //(b) checks whether the record is the latest version
        // omit. because this is implemented by single version

        if (expected.absent) {
            return Status::WARN_NOT_FOUND;
        }

        //(c) reads the data
        body = TupleBody(key, tuple->body_.get_val(),
                         tuple->body_.get_val_align());

        //(d) performs a memory fence
        // don't need.
        // order of load don't exchange.

        //(e) checks the TID word again
        check.obj_ = loadAcquire(tuple->tidword_.obj_);
        if (expected == check)
            break;
        expected = check;
        // #if ADD_ANALYSIS
        //     ++result_->local_extra_reads_;
        // #endif
    }
    read_set_.emplace_back(s, key, tuple, std::move(body), expected);
#if SLEEP_READ_PHASE
    sleepTics(SLEEP_READ_PHASE);
#endif

    return Status::OK;
}

// TODO: scan implementation
// Status TxExecutor::scan(const Storage s,
//                         std::string_view left_key, bool l_exclusive,
//                         std::string_view right_key, bool r_exclusive,
//                         std::vector<TupleBody*>& result) {}

// void tx_delete([[maybe_unused]]std::uint64_t key) {}

ReadElement<Tuple> *TxExecutor::searchReadSet(Storage s, std::string_view key) {
    for (auto &re : read_set_) {
        if (re.storage_ != s)
            continue;
        if (re.key_ == key)
            return &re;
    }

    return nullptr;
}

WriteElement<Tuple> *TxExecutor::searchWriteSet(Storage s,
                                                std::string_view key) {
    for (auto &we : write_set_) {
        if (we.storage_ != s)
            continue;
        if (we.key_ == key)
            return &we;
    }

    return nullptr;
}

void TxExecutor::unlockWriteSet() {
    Tidword expected, desired;

    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
        if ((*itr).op_ == OpType::INSERT)
            continue;
        expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        desired = expected;
        desired.lock = 0;
        storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
    }
}

void TxExecutor::unlockWriteSet(
    std::vector<WriteElement<Tuple>>::iterator end) {
    Tidword expected, desired;

    for (auto itr = write_set_.begin(); itr != end; ++itr) {
        if ((*itr).op_ == OpType::INSERT)
            continue;
        expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        desired = expected;
        desired.lock = 0;
        storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
    }
}

bool TxExecutor::validationPhase() { // Validation Phase
#if ADD_ANALYSIS
    std::uint64_t start = rdtscp();
#endif

    /* Phase 1
     * lock write_set_ sorted.*/
    sort(write_set_.begin(), write_set_.end());
    lockWriteSet();
#if NO_WAIT_LOCKING_IN_VALIDATION
    if (this->status_ == TransactionStatus::aborted) {
        result_->local_abort_by_validation1_++;
        return false;
    }
#endif

    asm volatile("" ::: "memory");
    atomicStoreThLocalEpoch(thid_, atomicLoadGE());
    asm volatile("" ::: "memory");

    /* Phase 2 abort if any condition of below is satisfied.
     * 1. tid of read_set_ changed from it that was got in Read Phase.
     * 2. not latest version
     * 3. the tuple is locked and it isn't included by its write set.*/

    Tidword check;
    for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
        // 1
        check.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        if ((*itr).get_tidword().epoch != check.epoch ||
            (*itr).get_tidword().tid != check.tid) {
            // #if ADD_ANALYSIS
            //       result_->local_vali_latency_ += rdtscp() - start;
            // #endif
            this->status_ = TransactionStatus::aborted;
            result_->local_abort_by_validation2_++;
            unlockWriteSet();
            return false;
        }
        // 2
        // if (!check.latest) return false;

        // 3
        if (check.lock && !searchWriteSet((*itr).storage_, (*itr).key_)) {
            // #if ADD_ANALYSIS
            //       result_->local_vali_latency_ += rdtscp() - start;
            // #endif
            result_->local_abort_by_validation3_++;
            this->status_ = TransactionStatus::aborted;
            unlockWriteSet();
            return false;
        }
        max_rset_ = std::max(max_rset_, check);
    }

    // goto Phase 3
    // #if ADD_ANALYSIS
    //   result_->local_vali_latency_ += rdtscp() - start;
    // #endif
    this->status_ = TransactionStatus::committed;
    return true;
}

void TxExecutor::wal(std::uint64_t ctid) {
    Tidword old_tid;
    Tidword new_tid;
    old_tid.obj_ = loadAcquire(CTIDW[thid_]);
    new_tid.obj_ = ctid;
    bool new_epoch_begins = (old_tid.epoch != new_tid.epoch);
    log_buffer_pool_.push(ctid, nid_, write_set_, new_epoch_begins);
    if (new_epoch_begins) {
        // store CTIDW
        __atomic_store_n(&(CTIDW[thid_]), ctid, __ATOMIC_RELEASE);
    }
}

Status TxExecutor::write(Storage s, std::string_view key, TupleBody &&body) {
#if ADD_ANALYSIS
    std::uint64_t start = rdtscp();
#endif

    if (searchWriteSet(s, key))
        goto FINISH_WRITE;

    /**
     * Search tuple from data structure.
     */
    Tuple *tuple;
    ReadElement<Tuple> *re;
    re = searchReadSet(s, key);
    if (re) {
        tuple = re->rcdptr_;
    } else {
        tuple = Table[get_storage(s)].get(key);
        // #if ADD_ANALYSIS
        //     ++result_->local_tree_traversal_;
        // #endif
        if (tuple == nullptr)
            return Status::WARN_NOT_FOUND;
    }

    write_set_.emplace_back(s, key, tuple, std::move(body), OpType::UPDATE);

FINISH_WRITE:

    // #if ADD_ANALYSIS
    //   result_->local_write_latency_ += rdtscp() - start;
    // #endif
    return Status::OK;
}

void TxExecutor::writePhase() {
    // It calculates the smallest number that is
    //(a) larger than the TID of any record read or written by the transaction,
    //(b) larger than the worker's most recently chosen TID,
    // and (C) in the current global epoch.

    Tidword tid_a, tid_b, tid_c;

    // calculates (a)
    // about read_set_
    tid_a = std::max(max_wset_, max_rset_);
    tid_a.tid++;

    // calculates (b)
    // larger than the worker's most recently chosen TID,
    tid_b = mrctid_;
    tid_b.tid++;

    // calculates (c)
    tid_c.epoch = ThLocalEpoch[thid_];

    // compare a, b, c
    Tidword maxtid = std::max({tid_a, tid_b, tid_c});
    maxtid.lock = 0;
    maxtid.latest = 1;
    mrctid_ = maxtid;

    wal(maxtid.obj_);

    // write(record, commit-tid)
    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
        // update and unlock
        switch ((*itr).op_) {
        case OpType::UPDATE: {
            memcpy((*itr).rcdptr_->body_.get_val_ptr(),
                   (*itr).body_.get_val_ptr(), (*itr).body_.get_val_size());
            storeRelease((*itr).rcdptr_->tidword_.obj_, maxtid.obj_);
            break;
        }
        case OpType::INSERT: {
            maxtid.absent = false;
            storeRelease((*itr).rcdptr_->tidword_.obj_, maxtid.obj_);
            break;
        }
        default:
            printf("okashiizo @writePhase");
            // ERR; // TODO: error handling
        }
    }

    read_set_.clear();
    write_set_.clear();
}

bool TxExecutor::commit() {
    if (validationPhase()) {
        writePhase();
        return true;
    } else {
        return false;
    }
}

bool TxExecutor::isLeader() { return this->thid_ == 0; }

void TxExecutor::leaderWork() {
    siloLeaderWork(this->epoch_timer_start, this->epoch_timer_stop);
}

/*
 *  silo_logging
 */

bool TxExecutor::pauseCondition() {
    auto dlepoch = loadAcquire(ThLocalDurableEpoch[logger_thid_]);
    return loadAcquire(ThLocalEpoch[thid_]) > dlepoch + EPOCH_DIFF;
}

void TxExecutor::epochWork(uint64_t &epoch_timer_start,
                           uint64_t &epoch_timer_stop) {
    waitTime_ns(1 * 1000);
    if (thid_ == 0)
        siloLeaderWork(epoch_timer_start, epoch_timer_stop);
    Tidword old_tid;
    old_tid.obj_ = loadAcquire(CTIDW[thid_]);
    // load GE
    atomicStoreThLocalEpoch(thid_, atomicLoadGE());
    uint64_t new_epoch = loadAcquire(ThLocalEpoch[thid_]);
    if (old_tid.epoch != new_epoch) {
        Tidword tid;
        tid.epoch = new_epoch;
        tid.lock = 0;
        tid.latest = 1;
        // store CTIDW
        __atomic_store_n(&(CTIDW[thid_]), tid.obj_, __ATOMIC_RELEASE);
    }
}

// TODO:コピペなので要確認
void TxExecutor::durableEpochWork(uint64_t &epoch_timer_start,
                                  uint64_t &epoch_timer_stop,
                                  const bool &quit) {
    std::uint64_t t = rdtscp();
    // pause this worker until Durable epoch catches up
    if (EPOCH_DIFF > 0) {
        if (pauseCondition()) {
            log_buffer_pool_.publish();
            do {
                epochWork(epoch_timer_start, epoch_timer_stop);
                if (loadAcquire(quit))
                    return;
            } while (pauseCondition());
        }
    }
    while (!log_buffer_pool_.is_ready()) {
        epochWork(epoch_timer_start, epoch_timer_stop);
        if (loadAcquire(quit))
            return;
    }
    if (log_buffer_pool_.current_buffer_ == NULL) {
        this->result_->local_abort_by_null_buffer_++;
        std::abort();
    }
    // sres_lg_->local_wait_depoch_latency_ += rdtscp() - t;
    // NOTE: sres_lg_使ってないので
}