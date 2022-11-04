/*
 * Copyright (C) 2011-2021 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "Enclave.h"
#include "Enclave_t.h" /* print_string */
#include <stdarg.h>
#include <stdio.h> /* vsnprintf */
#include <string.h>

#include <sgx_trts.h>


#include <vector>
#include <algorithm>
#include <cstdint>

#include "../Include/consts.h"

#include "../Include/random.h"



/* 
 * printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */
int printf(const char* fmt, ...) {
    char buf[BUFSIZ] = { '\0' };
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
    return (int)strnlen(buf, BUFSIZ - 1) + 1;
}

// MARK: class(atomic_wrapper)

template<typename T>
T load(T &ptr) {
  return __atomic_load_n(&ptr, __ATOMIC_RELAXED);
}

template<typename T>
T loadAcquire(T &ptr) {
  return __atomic_load_n(&ptr, __ATOMIC_ACQUIRE);
}

template<typename T, typename T2>
void store(T &ptr, T2 val) {
  __atomic_store_n(&ptr, (T) val, __ATOMIC_RELAXED);
}

template<typename T, typename T2>
void storeRelease(T &ptr, T2 val) {
  __atomic_store_n(&ptr, (T) val, __ATOMIC_RELEASE);
}

template<typename T, typename T2>
bool compareExchange(T &m, T &before, T2 after) {
  return __atomic_compare_exchange_n(&m, &before, (T) after, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}


// MARK: class(data)

enum class Ope : uint8_t {
    READ,
    WRITE,
    READ_MODIFY_WRITE,
};

class Procedure {
    public:
        Ope ope_;
        uint64_t key_;

        Procedure() : ope_(Ope::READ), key_(0) {}
        Procedure(Ope ope, uint64_t key) : ope_(ope), key_(key) {}
};

struct TIDword {
    union {
        uint64_t obj_;
        struct {
            bool lock : 1;
            bool latest : 1;
            bool absent : 1;
            uint64_t TID : 29;
            uint64_t epoch : 32;
        };
    };
    TIDword() : epoch(0), TID(0), absent(false), latest(true), lock(false) {};

    bool operator == (const TIDword &right) const { return obj_ == right.obj_; }
    bool operator != (const TIDword &right) const { return !operator == (right); }
    bool operator < (const TIDword &right) const { return this->obj_ < right.obj_; }
};

class Tuple {
    public:
        alignas(CACHE_LINE_SIZE) TIDword tidword_;
        uint64_t key_;
        uint64_t val_;
};

// MARK: class(op_element)

class OpElement {
    public:
        uint64_t key_;
        Tuple *rcdptr_;

        OpElement() : key_(0), rcdptr_(nullptr) {}
        OpElement(uint64_t key) : key_(key) {}
        OpElement(uint64_t key, Tuple *rcdptr) : key_(key), rcdptr_(rcdptr) {}
};

class ReadElement : public OpElement {
    public:
        using OpElement::OpElement;

        ReadElement(uint64_t key, Tuple *rcdptr, uint64_t val, TIDword tidword) : OpElement::OpElement(key, rcdptr) {
            tidword_.obj_ = tidword.obj_;
            this->val_ = val;
        }

        bool operator < (const ReadElement &right) const {
            return this->key_ < right.key_;
        }
        
        TIDword get_tidword() {
            return tidword_;
        }
    
    private:
        TIDword tidword_;
        uint64_t val_;
};

class WriteElement : public OpElement {
    public:
        using OpElement::OpElement;

        WriteElement(uint64_t key, Tuple *rcdptr, uint64_t val) : OpElement::OpElement(key, rcdptr) {
            this->val_ = val;
        }

        bool operator < (const WriteElement &right) const {
            return this->key_ < right.key_;
        }

        uint64_t get_val() {
            return val_;
        }

    private:
        uint64_t val_;
};

// MARK: class(Result)

// class Result {
//     public:
//         uint64_t local_abort_counts_ = 0;
//         uint64_t local_commit_counts_ = 0;
//         uint64_t total_abort_counts_ = 0;
//         uint64_t total_commit_counts_ = 0;
// };

// MARK: global variables

std::vector<Tuple> Table(TUPLE_NUM);
std::vector<uint64_t> ThLocalEpoch(THREAD_NUM);
std::vector<uint64_t> CTIDW(THREAD_NUM);
std::vector<uint64_t> ThLocalDurableEpoch(LOGGER_NUM);
uint64_t DurableEpoch;
uint64_t GlobalEpoch = 1;
std::vector<returnResult> results(THREAD_NUM);

// std::vector<Result> results(THREAD_NUM); //TODO: mainの方でもsiloresultを定義しているので重複回避目的で名前変更しているけどここら辺どうやって扱うか考えておく

bool start = false;
bool quit = false;

// MARK: leaderWork(add GE)

// TSC: time stamp counter
// リセットされてからのCPUサイクル数をカウントするやつ
static uint64_t rdtscp() {
    uint64_t rax;
    uint64_t rdx;
    uint32_t aux;
    asm volatile("rdtscp" : "=a"(rax), "=d"(rdx), "=c"(aux)::);
    // store EDX:EAX.
    // 全ての先行命令を待機してからカウンタ値を読み取る．ただし，後続命令は
    // 同読み取り操作を追い越す可能性がある．   
    return (rdx << 32) | rax;
}

#define INLINE __attribute__((always_inline)) inline

INLINE uint64_t atomicLoadGE() {
    uint64_t result = __atomic_load_n(&(GlobalEpoch), __ATOMIC_ACQUIRE);
    return result;
}

INLINE void atomicStoreThLocalEpoch(unsigned int thid, uint64_t newval) {
    __atomic_store_n(&(ThLocalEpoch[thid]), newval, __ATOMIC_RELEASE);
}

INLINE void atomicAddGE() {
    uint64_t expected, desired;
    expected = atomicLoadGE();
    for (;;) {
        desired = expected + 1;
        if (__atomic_compare_exchange_n(&(GlobalEpoch), &expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQ_REL)) break;
    }
}

inline static bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold) {
    uint64_t diff = 0;
    diff = stop - start;
    if (diff > threshold) {
        return true;
    } else {
        return false;
    }
}

bool chkEpochLoaded() {
    uint64_t nowepo = atomicLoadGE();
    // leader_workを実行しているのはthid:0だからforは1から回している？
    for (unsigned int i = 1; i < THREAD_NUM; i++) {
        if (__atomic_load_n(&(ThLocalEpoch[i]), __ATOMIC_ACQUIRE) != nowepo) return false;
    }
    return true;
}

void leaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop) {
    epoch_timer_stop = rdtscp();
    if (chkClkSpan(epoch_timer_start, epoch_timer_stop, EPOCH_TIME * CLOCKS_PER_US * 1000) && chkEpochLoaded()) {
        atomicAddGE();
        epoch_timer_start = epoch_timer_stop;
    }
}

// MARK: class(TxExecutor)

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

        //TODO: log?
        TransactionStatus status_;
        unsigned int thid_;
        TIDword mrctid_;
        TIDword max_rset_, max_wset_;

        uint64_t write_val_;
        uint64_t return_val_;

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

};

TxExecutor::TxExecutor(int thid) : thid_(thid) {
    read_set_.reserve(MAX_OPE);
    write_set_.reserve(MAX_OPE);
    pro_set_.reserve(MAX_OPE);

    max_rset_.obj_ = 0;
    max_wset_.obj_ = 0;
}

void TxExecutor::begin() {
    status_ = TransactionStatus::InFlight;
    max_wset_.obj_ = 0;
    max_rset_.obj_ = 0;
}

void TxExecutor::read(uint64_t key) {
    TIDword expected, check;
    if (searchReadSet(key) || searchWriteSet(key)) goto FINISH_READ;
    Tuple *tuple;
    tuple = &Table[key];
    expected.obj_ = loadAcquire(tuple->tidword_.obj_);
    
    for (;;) {
        // (a) reads the TID word, spinning until the lock is clear
        while (expected.lock) {
            expected.obj_ = loadAcquire(tuple->tidword_.obj_);
        }
        // (b) checks whether the record is the latest version
        // omit. because this is implemented by single version

        // (c) reads the data
        // memcpy(return_val_, tuple->value_, sizeof(uint64_t));
        return_val_ = tuple->val_;    // 値渡し(memcpyと同じ挙動)になっているはず

        // (d) performs a memory fence
        // don't need.
        // order of load don't exchange.    // Q:解説募
        check.obj_ = loadAcquire(tuple->tidword_.obj_);
        if (expected == check) break;
        expected = check;
    }
    read_set_.emplace_back(key, tuple, return_val_, expected);

    FINISH_READ:
    return;
}

void TxExecutor::write(uint64_t key, uint64_t val) {
    if (searchWriteSet(key)) goto FINISH_WRITE;
    Tuple *tuple;
    ReadElement *re;
    re = searchReadSet(key);
    if (re) {   //HACK: 仕様がわかってないよ(田中先生も)
        tuple = re->rcdptr_;
    } else {
        tuple = &Table[key];
    }
    write_set_.emplace_back(key, tuple, val);

    FINISH_WRITE:
    return;
}

ReadElement *TxExecutor::searchReadSet(uint64_t key) {
    for (auto itr = read_set_.begin(); itr != read_set_.end(); itr++) {
        if ((*itr).key_ == key) return &(*itr);
    }
    return nullptr;
}

WriteElement *TxExecutor::searchWriteSet(std::uint64_t key) {
    for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
        if ((*itr).key_ == key) return &(*itr);
    }
    return nullptr;
}

void TxExecutor::unlockWriteSet() {
    TIDword expected, desired;
    for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
        expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        desired = expected;
        desired.lock = 0;
        storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
    }
}

void TxExecutor::unlockWriteSet(std::vector<WriteElement>::iterator end) {
    TIDword expected, desired;
    for (auto itr = write_set_.begin(); itr != end; itr++) {
        expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        desired = expected;
        desired.lock = 0;
        storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
    }
}

void TxExecutor::lockWriteSet() {
    TIDword expected, desired;
    for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
        expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        for (;;) {
            if (expected.lock) {
                this->status_ = TransactionStatus::Aborted; // w-w conflictは即abort
                if (itr != write_set_.begin()) unlockWriteSet(itr);
                return;
            } else {
                desired = expected;
                desired.lock = 1;
                if (compareExchange((*itr).rcdptr_->tidword_.obj_, expected.obj_, desired.obj_)) break;
            }
        }
        max_wset_ = std::max(max_wset_, expected);
    }
}

bool TxExecutor::validationPhase() {
    // Phase1, sorting write_set_
    sort(write_set_.begin(), write_set_.end());
    lockWriteSet();
    if (this->status_ == TransactionStatus::Aborted) return false;  // w-w conflict検知時に弾く用
    asm volatile("":: : "memory");
    atomicStoreThLocalEpoch(thid_, atomicLoadGE());
    asm volatile("":: : "memory");
    // Phase2, Read validation
    TIDword check;
    for (auto itr = read_set_.begin(); itr != read_set_.end(); itr++) {
        // 1. tid of read_set_ changed from it that was got in Read Phase.
        check.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        if ((*itr).get_tidword().epoch != check.epoch || (*itr).get_tidword().TID != check.TID) {
            this->status_ = TransactionStatus::Aborted;
            unlockWriteSet();
            return false;
        }
        // 2. "omit" if (!check.latest) return false;
        
        // 3. the tuple is locked and it isn't included by its write set.
        if (check.lock && !searchWriteSet((*itr).key_)) {
            this->status_ = TransactionStatus::Aborted;
            unlockWriteSet();
            return false;
        }
        max_rset_ = std::max(max_rset_, check); // TID算出用
    }

    // goto Phase 3
    this->status_ = TransactionStatus::Committed;
    return true;
}

// TODO: void TxExecutor::wal(std::uint64_t ctid)

void TxExecutor::writePhase() {
    TIDword tid_a, tid_b, tid_c;    // TIDの算出
    // (a) transaction中でRead/Writeの最大TIDより1大きい
    tid_a = std::max(max_wset_, max_rset_);
    tid_a.TID++;
    // (b) workerが発行したTIDより1大きい
    tid_b = mrctid_;
    tid_b.TID++;
    // (c) 上位32bitがcurrent epoch
    tid_c.epoch = ThLocalEpoch[thid_];

    TIDword maxtid = std::max({tid_a, tid_b, tid_c});
    maxtid.lock = 0;
    maxtid.latest = 1;
    mrctid_ = maxtid;

    // TODO: wal

    // write
    for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
        (*itr).rcdptr_->val_ = (*itr).get_val();    // write_set_に登録したvalをTable(rcdptr_)のvalに移す
        storeRelease((*itr).rcdptr_->tidword_.obj_, maxtid.obj_);
    }        
    read_set_.clear();
    write_set_.clear();
}

void TxExecutor::abort() {
    read_set_.clear();
    write_set_.clear();
}


// MARK: utilities

void ecall_initDB() {
    // init Table
    for (int i = 0; i < TUPLE_NUM; i++) {
        Tuple *tmp;
        tmp = &Table[i];
        tmp->tidword_.epoch = 1;
        tmp->tidword_.latest = 1;
        tmp->tidword_.lock = 0;
        tmp->key_ = i;
        tmp->val_ = 0;
    }

    for (int i = 0; i < THREAD_NUM; i++) {
        ThLocalEpoch[i] = 0;
        CTIDW[i] = ~(uint64_t)0;
    }

    for (int i = 0; i < LOGGER_NUM; i++) {
        ThLocalDurableEpoch[i] = 0;
    }
    DurableEpoch = 0;
}

void makeProcedure(std::vector<Procedure> &pro, Xoroshiro128Plus &rnd) {
    pro.clear();
    for (int i = 0; i < MAX_OPE; i++) {
        uint64_t tmpkey, tmpope;
        tmpkey = rnd.next() % TUPLE_NUM;
        if ((rnd.next() % 100) == RRAITO) {
            pro.emplace_back(Ope::READ, tmpkey);
        } else {
            pro.emplace_back(Ope::WRITE, tmpkey);
        }
    }
}

// MARK: enclave function

// NOTE: enclave内で__atomic系の処理できたっけ？できるなら直でそのまま呼びたい
// というかquitは管理しなくてもいいかも
void ecall_sendStart() {
    __atomic_store_n(&start, true, __ATOMIC_RELEASE);
}

void ecall_sendQuit() {
    __atomic_store_n(&quit, true, __ATOMIC_RELEASE);
}


void ecall_worker_th(int thid) {
    TxExecutor trans(thid);
    returnResult &myres = std::ref(results[thid]);
    uint64_t epoch_timer_start, epoch_timer_stop;
    
    unsigned init_seed;
    sgx_read_rand((unsigned char *) &init_seed, 4);
    Xoroshiro128Plus rnd(init_seed);

    // Xoroshiro128Plus rnd(123456);   //seed値に使っている？とりあえず定数で置いておく

    // rnd.init()

    while (true) {
        if (__atomic_load_n(&start, __ATOMIC_ACQUIRE)) break;
    }

    if (thid == 0) epoch_timer_start = rdtscp();

    while (true) {
        if (__atomic_load_n(&quit, __ATOMIC_ACQUIRE)) break;
        
        makeProcedure(trans.pro_set_, rnd); // ocallで生成したprocedureをTxExecutorに移し替える

    RETRY:

        if (thid == 0) leaderWork(epoch_timer_start, epoch_timer_stop);
        if (__atomic_load_n(&quit, __ATOMIC_ACQUIRE)) break;

        trans.begin();
        for (auto itr = trans.pro_set_.begin(); itr != trans.pro_set_.end(); itr++) {
            if ((*itr).ope_ == Ope::READ) {
                trans.read((*itr).key_);
            } else if ((*itr).ope_ == Ope::WRITE) {
                trans.write((*itr).key_);
            } else {
                // ERR;
                // DEBUG
                printf("おい！なんか変だぞ！\n");
                return;
            }
        }
   
        if (trans.validationPhase()) {
            trans.writePhase();
            storeRelease(myres.local_commit_counts_, loadAcquire(myres.local_commit_counts_) + 1);
        } else {
            trans.abort();
            myres.local_abort_counts_++;
            goto RETRY;
        }
    }
    return;
}

void ecall_logger_th() {
    
}

uint64_t ecall_getAbortResult(int thid) {
    return results[thid].local_abort_counts_;
}

uint64_t ecall_getCommitResult(int thid) {
    return results[thid].local_commit_counts_;
}