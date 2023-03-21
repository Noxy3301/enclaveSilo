#include <algorithm> // for sort function

#include "../Include/consts.h"

#include "include/atomic_tool.h"
#include "include/atomic_wrapper.h"
#include "include/transaction.h"
#include "include/log.h"
#include "include/util.h"

#include "Enclave_t.h"
#include "sgx_thread.h"

void TxExecutor::begin() {
    status_ = TransactionStatus::inFlight;
    max_wset_.obj_ = 0;
    max_rset_.obj_ = 0;
    nid_ = NotificationId(nid_counter_++, thid_, rdtscp());
}

// こっちのreadはindexからKeyをつかって対応するbodyを持ってきている
// readしたbodyの一貫性はread_internalで確認している
Status TxExecutor::read(Storage s, std::string key) {
    // these variable cause error (-fpermissive)
    // "crosses initialization of ..."
    // So it locate before first goto instruction.
    Tidword expected, check;
    Status stat;
    ReadElement<Tuple>* re;
    WriteElement<Tuple>* we;

    // read-own-writes or re-read from local read set
    re = searchReadSet(s, key);
    if (re) {
        goto FINISH_READ;
    }
    we = searchWriteSet(s, key);
    if (we) {
        goto FINISH_READ;
    }

    // Search tuple from data structure
    Tuple *tuple;
    tuple = Table.get(key);
    // tuple = Masstrees[get_storage(s)].get_value(key);   // TODO: ここをOCHに変える
    if (tuple == nullptr) return Status::WARN_NOT_FOUND;

    stat = read_internal(s, key, tuple);
    if (stat != Status::OK) {
        return stat;
    }

FINISH_READ:
    return Status::OK;
}

// read_internalはbodyの一貫性はread_internalで確認している
Status TxExecutor::read_internal(Storage s, std::string key, Tuple* tuple) {
    // TupleBody body;
    Tidword expected, check;
    int val;

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
        // body = TupleBody(key, tuple->body_.get_val(), tuple->body_.get_val_align());
        val = tuple->value_;

        //(d) performs a memory fence
        // don't need.
        // order of load don't exchange.

        //(e) checks the TID word again
        check.obj_ = loadAcquire(tuple->tidword_.obj_);
        if (expected == check) break;
        expected = check;
    }
    read_set_.emplace_back(s, key, tuple, val, expected);

    return Status::OK;
}

// void TxExecutor::read(uint64_t key) {
//     Tidword expected, check;
//     if (searchReadSet(key) || searchWriteSet(key)) goto FINISH_READ;
//     Tuple *tuple;
    
//     // TODO: getとかに一元化したほうが良いかも
// #if INDEX_PATTERN == 0
//         tuple = &Table[key];
// #elif INDEX_PATTERN == 1
//         for (int i = 0; i < TUPLE_NUM; i++) {
//             if (Table[i].key_ == key) {
//                 tuple = &Table[key];
//                 break;
//             }
//         }
// #elif INDEX_PATTERN == 2
//         tuple = Table.get(key);
// #endif
    
//     expected.obj_ = loadAcquire(tuple->tidword_.obj_);
    
//     for (;;) {
//         // (a) reads the TID word, spinning until the lock is clear
//         while (expected.lock) {
//             expected.obj_ = loadAcquire(tuple->tidword_.obj_);
//         }
//         // (b) checks whether the record is the latest version
//         // omit. because this is implemented by single version

//         // (c) reads the data
//         // memcpy(return_val_, tuple->value_, sizeof(uint64_t));
//         return_val_ = tuple->val_;    // 値渡し(memcpyと同じ挙動)になっているはず

//         // (d) performs a memory fence
//         // don't need.
//         // order of load don't exchange.    // Q:解説募
//         check.obj_ = loadAcquire(tuple->tidword_.obj_);
//         if (expected == check) break;
//         expected = check;
//     }
//     read_set_.emplace_back(key, tuple, return_val_, expected);

//     FINISH_READ:
//     return;
// }

Status TxExecutor::write(Storage s, std::string key) {
    if (searchWriteSet(s, key)) goto FINISH_WRITE;

    // Search tuple from data structure
    Tuple *tuple;
    ReadElement<Tuple> *re;
    re = searchReadSet(s, key);
    if (re) {
        tuple = re->rcdptr_;
    } else {
        tuple = Table.get(key);
        // tuple = Masstrees[get_storage(s)].get_value(key);   // TODO: OCHに変える
        if (tuple == nullptr) return Status::WARN_NOT_FOUND;
    }

    write_set_.emplace_back(s, key, tuple, tuple->value_, OpType::UPDATE); // TODO: valの扱い

FINISH_WRITE:
    return Status::OK;
}

// void TxExecutor::write(uint64_t key, uint64_t val) {
//     if (searchWriteSet(key)) goto FINISH_WRITE;
//     Tuple *tuple;
//     ReadElement *re;
//     re = searchReadSet(key);
//     if (re) {   //HACK: 仕様がわかってないよ(田中先生も)
//         tuple = re->rcdptr_;
//     } else {
//         // TODO: getとかに一元化したほうが良いかも
// #if INDEX_PATTERN == 0
//         tuple = &Table[key];
// #elif INDEX_PATTERN == 1
//         for (int i = 0; i < TUPLE_NUM; i++) {
//             if (Table[i].key_ == key) {
//                 tuple = &Table[key];
//                 break;
//             }
//         }
// #elif INDEX_PATTERN == 2
//         tuple = Table.get(key);
// #endif
//     }
//     write_set_.emplace_back(key, tuple, val);

//     FINISH_WRITE:
//     return;
// }

ReadElement<Tuple> *TxExecutor::searchReadSet(Storage s, std::string key) {
    for (auto &re : read_set_) {
        if (re.storage_ != s) continue;
        if (re.key_ == key) return &re;
    }

    return nullptr;
}

WriteElement<Tuple> *TxExecutor::searchWriteSet(Storage s, std::string key) {
    for (auto &we : write_set_) {
        if (we.storage_ != s) continue;
        if (we.key_ == key) return &we;
    }

    return nullptr;
}
// ReadElement *TxExecutor::searchReadSet(uint64_t key) {
//     for (auto itr = read_set_.begin(); itr != read_set_.end(); itr++) {
//         if ((*itr).key_ == key) return &(*itr);
//     }
//     return nullptr;
// }

// WriteElement *TxExecutor::searchWriteSet(std::uint64_t key) {
//     for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
//         if ((*itr).key_ == key) return &(*itr);
//     }
//     return nullptr;
// }

void TxExecutor::unlockWriteSet() {
    Tidword expected, desired;

    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
        if ((*itr).op_ == OpType::INSERT) continue;
        expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        desired = expected;
        desired.lock = 0;
        storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
    }
}

// void TxExecutor::unlockWriteSet() {
//     Tidword expected, desired;
//     for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
//         expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
//         desired = expected;
//         desired.lock = 0;
//         storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
//     }
// }

void TxExecutor::unlockWriteSet(std::vector<WriteElement<Tuple>>::iterator end) {
    Tidword expected, desired;

    for (auto itr = write_set_.begin(); itr != end; ++itr) {
        if ((*itr).op_ == OpType::INSERT) continue;
        expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        desired = expected;
        desired.lock = 0;
        storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
    }
}

// void TxExecutor::unlockWriteSet(std::vector<WriteElement>::iterator end) {
//     Tidword expected, desired;
//     for (auto itr = write_set_.begin(); itr != end; itr++) {
//         expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
//         desired = expected;
//         desired.lock = 0;
//         storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
//     }
// }

void TxExecutor::lockWriteSet() {
    Tidword expected, desired;

    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
        if (itr->op_ == OpType::INSERT) continue;
        expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        for (;;) {
            if (expected.lock) {
#if NO_WAIT_LOCKING_IN_VALIDATION
                this->status_ = TransactionStatus::aborted; // w-w conflictは即abort
                if (itr != write_set_.begin()) unlockWriteSet(itr);
                return;
#endif
            } else {
                desired = expected;
                desired.lock = 1;
                if (compareExchange((*itr).rcdptr_->tidword_.obj_, expected.obj_, desired.obj_)) break;
            }
        }
        max_wset_ = std::max(max_wset_, expected);
    }
}

// void TxExecutor::lockWriteSet() {
//     Tidword expected, desired;
//     for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
//         for (;;) {
//             expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
//             if (expected.lock) {
// #if NO_WAIT_LOCKING_IN_VALIDATION
//                 this->status_ = TransactionStatus::aborted; // w-w conflictは即abort
//                 if (itr != write_set_.begin()) unlockWriteSet(itr);
//                 return;
// #endif
//             } else {
//                 desired = expected;
//                 desired.lock = 1;
//                 if (compareExchange((*itr).rcdptr_->tidword_.obj_, expected.obj_, desired.obj_))
//                 break;
//             }
//         }
//         max_wset_ = std::max(max_wset_, expected);
//     }
// }

bool TxExecutor::validationPhase() { // Validation Phase
    // Phase 1 sorting write_set_ 
    sort(write_set_.begin(), write_set_.end());
    lockWriteSet();
#if NO_WAIT_LOCKING_IN_VALIDATION
    if (this->status_ == TransactionStatus::aborted) {
        result_->local_abort_by_validation1_++;
        return false;
    }
#endif

    asm volatile("":: : "memory");
    atomicStoreThLocalEpoch(thid_, atomicLoadGE());
    asm volatile("":: : "memory");

    /* Phase 2 abort if any condition of below is satisfied.
     * 1. tid of read_set_ changed from it that was got in Read Phase.
     * 2. not latest version
     * 3. the tuple is locked and it isn't included by its write set.*/

    Tidword check;
    for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
        // 1
        check.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        if ((*itr).get_tidword().epoch != check.epoch || (*itr).get_tidword().tid != check.tid) {
            this->status_ = TransactionStatus::aborted;
            result_->local_abort_by_validation2_++;
            unlockWriteSet();
            return false;
        }
        // 2
        // if (!check.latest) return false;

        // 3 the tuple is locked and it isn't included by its write set.
        if (check.lock && !searchWriteSet((*itr).storage_, (*itr).key_)) {
            this->status_ = TransactionStatus::aborted;
            result_->local_abort_by_validation3_++;
            unlockWriteSet();
            return false;
        }
        max_rset_ = std::max(max_rset_, check);
    }

    // loggingができる(current_bufferがあるか確認する)
    if (log_buffer_pool_.current_buffer_==NULL) {
        this->result_->local_abort_by_null_buffer_++;
        return false;
    }

    // goto Phase 3
    this->status_ = TransactionStatus::committed;
    return true;
}

// bool TxExecutor::validationPhase() {
//     // Phase1, sorting write_set_
//     sort(write_set_.begin(), write_set_.end());
//     lockWriteSet();
// #if NO_WAIT_LOCKING_IN_VALIDATION
//     if (this->status_ == TransactionStatus::aborted) {
//         this->result_->local_abort_by_validation1_++;
//         return false;  // w-w conflict検知時に弾く用
//     }
// #endif
//     asm volatile("":: : "memory");
//     atomicStoreThLocalEpoch(thid_, atomicLoadGE());
//     asm volatile("":: : "memory");
//     // Phase2, Read validation
//     Tidword check;
//     for (auto itr = read_set_.begin(); itr != read_set_.end(); itr++) {
//         // 1. tid of read_set_ changed from it that was got in Read Phase.
//         check.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
//         if ((*itr).get_tidword().epoch != check.epoch || (*itr).get_tidword().TID != check.TID) {
//             this->status_ = TransactionStatus::aborted;
//             this->result_->local_abort_by_validation2_++;
//             unlockWriteSet();
//             return false;
//         }
//         // 2. "omit" if (!check.latest) return false;
        
//         // 3. the tuple is locked and it isn't included by its write set.
//         if (check.lock && !searchWriteSet((*itr).key_)) {
//             this->status_ = TransactionStatus::aborted;
//             this->result_->local_abort_by_validation3_++;
//             unlockWriteSet();
//             return false;
//         }
//         max_rset_ = std::max(max_rset_, check); // TID算出用
//     }

//     // goto Phase 3
//     this->status_ = TransactionStatus::committed;
//     return true;
// }

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

                // memcpy((*itr).rcdptr_->body_.get_val_ptr(), (*itr).body_.get_val_ptr(), (*itr).body_.get_val_size());
                // std::cout << (*itr).rcdptr_->tidword_.tid << " " << maxtid.tid << std::endl;
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
                // ERR;
        }
    }
    read_set_.clear();
    write_set_.clear();
}



// void TxExecutor::writePhase() {
//     Tidword tid_a, tid_b, tid_c;    // TIDの算出
//     // (a) transaction中でRead/Writeの最大TIDより1大きい
//     tid_a = std::max(max_wset_, max_rset_);
//     tid_a.TID++;
//     // (b) workerが発行したTIDより1大きい
//     tid_b = mrctid_;
//     tid_b.TID++;
//     // (c) 上位32bitがcurrent epoch
//     tid_c.epoch = ThLocalEpoch[thid_];

//     Tidword maxtid = std::max({tid_a, tid_b, tid_c});
//     maxtid.lock = 0;
//     maxtid.latest = 1;
//     mrctid_ = maxtid;

//     // TODO: wal
//     wal(maxtid.obj_);

//     // write
//     for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
//         (*itr).rcdptr_->val_ = (*itr).get_val();    // write_set_に登録したvalをTable(rcdptr_)のvalに移す
//         storeRelease((*itr).rcdptr_->tidword_.obj_, maxtid.obj_);
//     }        
//     read_set_.clear();
//     write_set_.clear();
// }

void TxExecutor::abort() {
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

bool TxExecutor::isLeader() {
    return this->thid_ == 0;
}

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

void TxExecutor::epochWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop) {
    waitTime_ns(1*1000);
    if (thid_ == 0) siloLeaderWork(epoch_timer_start, epoch_timer_stop);
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
                                   uint64_t &epoch_timer_stop, const bool &quit) {
  std::uint64_t t = rdtscp();
  // pause this worker until Durable epoch catches up
  if (EPOCH_DIFF > 0) {
    if (pauseCondition()) {
      log_buffer_pool_.publish();
      do {
        epochWork(epoch_timer_start, epoch_timer_stop);
        if (loadAcquire(quit)) return;
      } while (pauseCondition());
    }
  }
  while (!log_buffer_pool_.is_ready()) {
    epochWork(epoch_timer_start, epoch_timer_stop);
    if (loadAcquire(quit)) return;
  }
  if (log_buffer_pool_.current_buffer_==NULL) std::abort();
  this->result_->local_abort_by_null_buffer_++;
  // sres_lg_->local_wait_depoch_latency_ += rdtscp() - t;
  // NOTE: sres_lg_使ってないので
}