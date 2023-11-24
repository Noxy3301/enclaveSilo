#include "include/silo_transaction.h"


inline int string_to_int(std::string &str_key) {
    int key = 0;
    for (int i = 0; i < str_key.size(); i++) {
        key = key * 10 + (str_key[i] - '0');
    }
    return key;
}

// トランザクションのライフサイクル管理
void TxExecutor::begin() {
    status_ = TransactionStatus::InFlight;
    max_wset_.obj_ = 0;
    max_rset_.obj_ = 0;

    // TODO: notificationID counterの実装
    // nid_ = NotificationId(nid_counter_++, thid_, rdtscp());
}

void TxExecutor::abort() {
    // remove inserted records
//     for (auto &we : write_set_) {
//         if (we.op_ == OpType::INSERT) {
// #if INDEX_PATTERN == INDEX_USE_MASSTREE
//             masstree.remove_value(we.key_, gc_);
//             delete we.value_;  // insertでwrite_set_に作成したvalue_を渡しているのでここでdeleteできる
// #endif
//         }
//     }

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

// トランザクションの操作

Status TxExecutor::insert(uint64_t &uint64t_key, uint64_t &uint64t_value) {
#if INDEX_PATTERN == INDEX_USE_MASSTREE
    key_.set({uint64t_key}, 8);
#endif

    // If the key already exists in write_sets_, return WARN_ALREADY_EXISTS.
    if (searchWriteSet(uint64t_key)) return Status::WARN_ALREADY_EXISTS;

    // If the key already exists in masstree, return WARN_ALREADY_EXISTS.
#if INDEX_PATTERN == INDEX_USE_MASSTREE
    Value *found_value = masstree.get_value(key_);
#elif INDEX_PATTERN == INDEX_USE_OCH
    Value *found_value = Table.get(static_cast<int>(uint64t_key));
#endif
    if (found_value != nullptr) return Status::WARN_ALREADY_EXISTS;

    // absent bitが立っているvalueを作成して、Masstreeに挿入する
    Value *value = new Value(uint64t_value);
    value->tidword_.absent = true;
#if INDEX_PATTERN == INDEX_USE_MASSTREE
    Status status = masstree.insert_value(key_, value, gc_);
    if (status == Status::WARN_ALREADY_EXISTS) {
        delete value;
        return status;
    }
#endif
    // write_set_は指定したvalueのbody_(std::string)をstr_valueで更新する
    // insertの場合、value->body_ == str_valueだけど、write_set_の形式に合わせることで、writePhase()での処理を共通化してる
    write_set_.emplace_back(uint64t_key, value, uint64t_value, OpType::INSERT);

    return Status::OK;
}

// TODO: delete implementation
// void TxExecutor::tx_delete(Key &key) {}

Status TxExecutor::read(uint64_t &uint64t_key) {
#ifdef ADD_ANALYSIS
    uint64_t start_read = rdtscp();
    // to avoid bypass
    uint64_t start_masstree_read = 0;
    uint64_t end_masstree_read = 0;
    uint64_t end_read = 0;
#endif
    // Place variables before the first goto instruction to avoid "crosses initialization of ..." error under -fpermissive.
#if INDEX_PATTERN == INDEX_USE_MASSTREE
    key_.set({uint64t_key}, 8); // MEMO:まあ最適化っていうことでこれでいいか
#endif
    Value *found_value; // TODO: found_valueを返すべきか？

    TIDword expected, check;
    Status status;
    ReadElement *readElement;
    WriteElement *writeElement;

    // read-own-writes or re-read from local read set
    readElement = searchReadSet(uint64t_key);
    if (readElement) {
        found_value = readElement->value_;  // copy value from read set
        goto FINISH_READ;
    }
    writeElement = searchWriteSet(uint64t_key);
    if (writeElement) {
        found_value = writeElement->value_; // copy value from write set
        goto FINISH_READ;
    }
#ifdef ADD_ANALYSIS
    start_masstree_read = rdtscp();
#endif
#if INDEX_PATTERN == INDEX_USE_MASSTREE
    found_value = masstree.get_value(key_);
#elif INDEX_PATTERN == INDEX_USE_OCH
    found_value = Table.get(static_cast<int>(uint64t_key)); // MEMO:事故りそう
#endif
#ifdef ADD_ANALYSIS
    end_masstree_read = rdtscp();
    workerResults[worker_thid_].local_masstree_read_get_value_time_ += end_masstree_read - start_masstree_read;
#endif
    if (found_value == nullptr) {
#ifdef ADD_ANALYSIS
        end_read = rdtscp();
        workerResults[worker_thid_].local_read_time_ += end_read - start_read;
#endif
        return Status::WARN_NOT_FOUND;
    }
    status = read_internal(uint64t_key, key_, found_value);
    if (status != Status::OK) {
#ifdef ADD_ANALYSIS
        end_read = rdtscp();
        workerResults[worker_thid_].local_read_time_ += end_read - start_read;
#endif
        return status;
    }

FINISH_READ:
#ifdef ADD_ANALYSIS
        end_read = rdtscp();
        workerResults[worker_thid_].local_read_time_ += end_read - start_read;
#endif
    return Status::OK;
}

// read_set_に追加する前に、直前のreadで取得したvalueが変更されていないかを確認する
Status TxExecutor::read_internal(uint64_t &uint64t_key, Key &key, Value *value) {
#ifdef ADD_ANALYSIS
    uint64_t start_read_internal = rdtscp();
#endif
    TIDword expected, check;

    // (a) reads the TID word, spinning until the lock is clear
    expected.obj_ = loadAcquire(value->tidword_.obj_);
    
    // check if it is locked.
    // spinning until the lock is clear
    for (;;) {
        while (expected.lock) {
            expected.obj_ = loadAcquire(value->tidword_.obj_);
        }

        // (b) checks whether the record is the latest version
        //     - omit. because this is implemented by single version

        if (expected.absent) {
#ifdef ADD_ANALYSIS
            uint64_t end_read_internal = rdtscp();
            workerResults[worker_thid_].local_read_internal_time_ += end_read_internal - start_read_internal;
#endif
            return Status::WARN_NOT_FOUND;
        }

        // (c) reads the data
        //     - since value is already a pointer to Value, you don’t need to do anything special here to read the data.
        // TODO: value.body_を取得するならここ？

        // (d) performs a memory fence
        //     - don't need, order of load don't exchange.

        // (e) checks the TID word again
        check.obj_ = loadAcquire(value->tidword_.obj_);
        if (expected == check) {
            break;
        }
        expected = check;
    }

    read_set_.emplace_back(uint64t_key, value, expected);

#ifdef ADD_ANALYSIS
    uint64_t end_read_internal = rdtscp();
    workerResults[worker_thid_].local_read_internal_time_ += end_read_internal - start_read_internal;
#endif
    return Status::OK;
}


/**
 * @brief Performs a local update of a record within TxExecutor using the provided value.
 *
 * @param key The key identifying the record to be updated.
 * @param value Pointer to the new value to update the record with.
 * @return Status::OK if the value is successfully updated; Status::WARN_NOT_FOUND if the key is not found.
 * 
 * @note In this implementation, records are registered in TxExecutor's write_set_, and during the commit phase, the records are updated based on the information in write_set_
 */
Status TxExecutor::write(uint64_t &uint64t_key, uint64_t &uint64t_value) {
#ifdef ADD_ANALYSIS
    uint64_t start_write = rdtscp();
#endif
    // Place variables before the first goto instruction to avoid "crosses initialization of ..." error under -fpermissive.

    uint64_t start_masstree_read = 0;
    uint64_t end_masstree_read = 0;
    uint64_t start_temp = 0;
    uint64_t end_temp = 0;
        
#if INDEX_PATTERN == INDEX_USE_MASSTREE
    key_.set({uint64t_key}, 8);
#endif

    Value *found_value;
    ReadElement *readElement;
    start_temp = rdtscp();
    end_temp = rdtscp();

    if (searchWriteSet(uint64t_key)) goto FINISH_WRITE;

    readElement = searchReadSet(uint64t_key);
    if (readElement) {
        found_value = readElement->value_;
    } else {
#ifdef ADD_ANALYSIS
    start_masstree_read = rdtscp();
#endif
#if INDEX_PATTERN == INDEX_USE_MASSTREE
        found_value = masstree.get_value(key_);
#elif INDEX_PATTERN == INDEX_USE_OCH
        found_value = Table.get(static_cast<int>(uint64t_key));
#endif
#ifdef ADD_ANALYSIS
    end_masstree_read = rdtscp();
    workerResults[worker_thid_].local_masstree_write_get_value_time_ += end_masstree_read - start_masstree_read;
#endif
        if (found_value == nullptr) {
#ifdef ADD_ANALYSIS
            uint64_t end_write = rdtscp();
            workerResults[worker_thid_].local_write_time_ += end_write - start_write - (end_temp - start_temp);
#endif
            return Status::WARN_NOT_FOUND;
        }
    }



    write_set_.emplace_back(uint64t_key, found_value, uint64t_value, OpType::WRITE);

FINISH_WRITE:

#ifdef ADD_ANALYSIS
    uint64_t end_write = rdtscp();
    workerResults[worker_thid_].local_write_time_ += end_write - start_write - (end_temp - start_temp);
#endif

    return Status::OK;
}

// TODO: scan implementation
// Status TxExecutor::scan(Key &left_key, bool l_exclusive, Key &right_key, bool r_exclusive, std::vector<Value *> &result) {}

/**
 * @brief Locks objects in the write set of a transaction.
 *
 * @note The method skips over objects in the write set with an operation type
 * of INSERT.
 * 
 * @note If a write-write conflict is detected (i.e. another transaction has
 * already locked the object), this transaction is immediately aborted.
 */
void TxExecutor::lockWriteSet() {
    TIDword expected, desired;

    for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
        if (itr->op_ == OpType::INSERT) continue;
        expected.obj_ = loadAcquire((*itr).value_->tidword_.obj_);
        for (;;) {
            if (expected.lock) {
                status_ = TransactionStatus::Aborted; // write-write conflict is immediately aborted
                if (itr != write_set_.begin()) unlockWriteSet(itr);
                return;
            } else {
                desired = expected;
                desired.lock = 1;
                if (compareExchange((*itr).value_->tidword_.obj_, expected.obj_, desired.obj_)) {
                    break;
                }
            }
        }
        max_wset_ = std::max(max_wset_, expected);
    }
}

void TxExecutor::unlockWriteSet() {
    TIDword expected, desired;

    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
        if ((*itr).op_ == OpType::INSERT) continue;
        expected.obj_ = loadAcquire((*itr).value_->tidword_.obj_);
        desired = expected;
        desired.lock = 0;
        storeRelease((*itr).value_->tidword_.obj_, desired.obj_);
    }
}

void TxExecutor::unlockWriteSet(std::vector<WriteElement>::iterator end) {
    TIDword expected, desired;

    for (auto itr = write_set_.begin(); itr != end; ++itr) {
        if ((*itr).op_ == OpType::INSERT) continue;
        expected.obj_ = loadAcquire((*itr).value_->tidword_.obj_);
        desired = expected;
        desired.lock = 0;
        storeRelease((*itr).value_->tidword_.obj_, desired.obj_);
    }
}

/**
 * @brief Executes the validation phase of a transaction.
 * 
 * @return Returns true if the transaction passes the validation phase and is committed.
 * Returns false if the transaction is aborted during the validation phase.
 */
bool TxExecutor::validationPhase() {
#ifdef ADD_ANALYSIS
    uint64_t start_validation_phase = rdtscp();
#endif
    // === Phase 1 ===
    // sort and lock write_set_
    sort(write_set_.begin(), write_set_.end());
    lockWriteSet();

    // update thread local epoch
    asm volatile("":: : "memory");
    atomicStoreThLocalEpoch(worker_thid_, atomicLoadGE());
    asm volatile("":: : "memory");

    // === Phase 2 ===
    // abort if any condition of below is satisfied.
    //   1. tid of read_set_ changed from it that was got in Read Phase.
    //   2. not latest version.
    //   3. the tuple is locked and it isn't included by its write set.

    TIDword check;
    for (auto itr = read_set_.begin(); itr != read_set_.end(); itr++) {
        // [1]
        check.obj_ = loadAcquire((*itr).value_->tidword_.obj_);
        if ((*itr).get_tidword().epoch != check.epoch || (*itr).get_tidword().TID != check.TID) {
            status_ = TransactionStatus::Aborted;
            unlockWriteSet();
#ifdef ADD_ANALYSIS
            uint64_t end_validation_phase = rdtscp();
            workerResults[worker_thid_].local_validation_time_ += end_validation_phase - start_validation_phase;
#endif
            return false;
        }

        // [2]
        // if (!check.latest) return false;

        // [3]
        if (check.lock && !searchWriteSet((*itr).key_)) {
            status_ = TransactionStatus::Aborted;
            unlockWriteSet();
#ifdef ADD_ANALYSIS
            uint64_t end_validation_phase = rdtscp();
            workerResults[worker_thid_].local_validation_time_ += end_validation_phase - start_validation_phase;
#endif
            return false;
        }

        // update max_rset_
        max_rset_ = std::max(max_rset_, check);
    }

    // === Phase 3 ===
    this->status_ = TransactionStatus::Committed;
#ifdef ADD_ANALYSIS
    uint64_t end_validation_phase = rdtscp();
    workerResults[worker_thid_].local_validation_time_ += end_validation_phase - start_validation_phase;
#endif
    return true;
}

void TxExecutor::writePhase() {
#ifdef ADD_ANALYSIS
    uint64_t start_write_phase = rdtscp();
#endif
    // Determines the minimal number(TID) that is:
    // [a] Greater than the TID of any record read or written by the transaction,
    // [b] Exceeds the worker's most recently chosen TID,
    // [c] Within the current global epoch.

    TIDword tid_a, tid_b, tid_c;

    // [a] is satisfied by max_rset_ and max_wset_.
    tid_a = std::max(max_rset_, max_wset_);
    tid_a.TID++;

    // [b] is satisfied by mrctid_.
    tid_b = mrctid_;
    tid_b.TID++;

    // [c] is satisfied by ThLocalEpoch[worker_thid_].
    tid_c.epoch = ThLocalEpoch[worker_thid_];

    // compare a, b, c, and calculate maxtid.
    TIDword maxtid = std::max({tid_a, tid_b, tid_c});
    maxtid.lock = 0;
    maxtid.latest = 1;
    mrctid_ = maxtid;

    // write ahead logging
    wal(maxtid.obj_);

    // TODO: old_valueはnewでヒープ領域に作成しているから、garbage collectionに回さないといけないのでは？
    // write
    for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
        // update and unlock
        switch ((*itr).op_) {
            case OpType::WRITE:
                itr->value_->body_ = itr->get_new_value_body();
                storeRelease(itr->value_->tidword_.obj_, maxtid.obj_);
                break;
            case OpType::INSERT:
                maxtid.absent = false;
                storeRelease(itr->value_->tidword_.obj_, maxtid.obj_);
                break;
            case OpType::DELETE:
                // TODO: delete implementation
                break;
            default:
                assert(false);  // unreachable
                break;
        }
    }

    // clear read_set_ and write_set_
    read_set_.clear();
    write_set_.clear();

#ifdef ADD_ANALYSIS
    uint64_t end_write_phase = rdtscp();
    workerResults[worker_thid_].local_write_phase_time_ += end_write_phase - start_write_phase;
#endif
}

// Write-Ahead Logging
void TxExecutor::wal(std::uint64_t ctid) {
    TIDword old_tid, new_tid;
    old_tid.obj_ = loadAcquire(CTIDW[worker_thid_]);
    new_tid.obj_ = ctid;
    bool new_epoch_begins = (old_tid.epoch != new_tid.epoch);
    log_buffer_pool_.push(ctid, nid_, write_set_, new_epoch_begins);
    if (new_epoch_begins) {
        // store CTIDW
        __atomic_store_n(&(CTIDW[worker_thid_]), ctid, __ATOMIC_RELEASE);
    }
}

// エポック管理
bool TxExecutor::isLeader() {
    return this->worker_thid_;
}

void TxExecutor::leaderWork() {
    siloLeaderWork(epoch_timer_start, epoch_timer_stop);
}

void TxExecutor::epochWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop) {
    if (worker_thid_ == 0) siloLeaderWork(epoch_timer_start, epoch_timer_stop);
    TIDword old_tid;
    old_tid.obj_ = loadAcquire(CTIDW[worker_thid_]);
    // load Global Epoch
    atomicStoreThLocalEpoch(worker_thid_, atomicLoadGE());
    uint64_t new_epoch = loadAcquire(ThLocalEpoch[worker_thid_]);
    if (old_tid.epoch != new_epoch) {
        TIDword tid;
        tid.epoch = new_epoch;
        tid.lock = 0;
        tid.latest = 1;

        // store CTIDW
        __atomic_store_n(&(CTIDW[worker_thid_]), tid.obj_, __ATOMIC_RELEASE);
    }
}

void TxExecutor::durableEpochWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop, const bool &quit) {
    // Wait until the log buffer pool is ready, performing epoch work in the meantime.
    while (!log_buffer_pool_.is_ready()) {
        epochWork(epoch_timer_start, epoch_timer_stop);
        // If quit is true, exit the function early.
        if (loadAcquire(quit)) return;
    }

    // If the current buffer in the log buffer pool is NULL, abort the program.
    if (log_buffer_pool_.current_buffer_ == NULL) std::abort();

    // [NOTE] The following line, currently commented out, would calculate and store the wait latency.
    // sres_lg_->local_wait_depoch_latency_ += rdtscp() - t;
}

ReadElement* TxExecutor::searchReadSet(uint64_t &uint64t_key) {
    for (auto& re : read_set_) {
        if (re.key_ == uint64t_key) return &re;
    }
    return nullptr;
}

WriteElement *TxExecutor::searchWriteSet(uint64_t &uint64t_key) {
    for (auto& we : write_set_) {
        if (we.key_ == uint64t_key) return &we;
    }
    return nullptr;
}