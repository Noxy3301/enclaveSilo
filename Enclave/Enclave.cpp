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

// include Silo
#include "silo_cc/include/silo_logger.h"
#include "silo_cc/include/silo_notifier.h"
#include "silo_cc/include/silo_util.h"

// include Masstree
#include "masstree/include/masstree.h"

// include OptCuckoo
#include "optcuckoo/optcuckoo.cpp"

// include utilities
#include "../Include/structures.h"
#include "global_variables.h"
#include "utils/atomic_wrapper.h"
#include "utils/zipf.h"

#if INDEX_PATTERN == INDEX_USE_MASSTREE
Masstree masstree;
#elif INDEX_PATTERN == INDEX_USE_OCH
OptCuckoo<Value*> Table(TUPLE_NUM*2);
#endif

uint64_t GlobalEpoch = 1;                       // Global Epoch
std::vector<uint64_t> ThLocalEpoch(WORKER_NUM); // 各ワーカースレッドのLocal epoch, Global epochを参照せず、epoch更新時に更新されるLocal epochを参照してtxを処理する
std::vector<uint64_t> CTIDW(WORKER_NUM);        // 各ワーカースレッドのCommit Timestamp ID, TID算出時にWorkerが発行したTIDとして用いられたものが保存される(TxExecutorのmrctid_と同じ値を持っているはず...)

uint64_t DurableEpoch;                                  // Durable Epoch, 永続化された全てのデータのエポックの最大値を表す(epoch <= DのtxはCommit通知ができる)
std::vector<uint64_t> ThLocalDurableEpoch(LOGGER_NUM);  // 各ロガースレッドのLocal durable epoch, Global durable epcohの算出に使う

std::vector<WorkerResult> workerResults(WORKER_NUM);    // ワーカースレッドの実行結果を格納するベクター
std::vector<LoggerResult> loggerResults(LOGGER_NUM);    // ロガースレッドの実行結果を格納するベクター

std::atomic<Logger *> logs[LOGGER_NUM]; // ロガーオブジェクトのアトミックなポインタを保持する配列, 複数のスレッドからの安全なアクセスが可能
Notifier notifier;                      // 通知オブジェクト, スレッド間でのイベント通知を管理する

std::vector<int> readys(WORKER_NUM);    // 各ワーカースレッドの準備状態を表すベクター, 全てのスレッドが準備完了するのを待つ
bool start = false;
bool quit = false;

// MARK: enclave function

// NOTE: enclave内で__atomic系の処理できたっけ？できるなら直でそのまま呼びたい
// というかquitは管理しなくてもいいかも

void ecall_waitForReady() {
    while (true) {
        bool failed = false;
        for (const auto &ready : readys) {
            if (!__atomic_load_n(&ready, __ATOMIC_ACQUIRE)) {
                failed = true;
                break;
            }
        }
        if (!failed) break;
    }
}

void ecall_initDB() {
    GarbageCollector gc;    // NOTE: このgcは使われない
    for (uint64_t i = 0; i < TUPLE_NUM; i++) {
#if INDEX_PATTERN == INDEX_USE_MASSTREE
        Key key({i}, 8);
        Status status = masstree.insert_value(key, new Value{i}, gc);
        if (status == Status::WARN_ALREADY_EXISTS) {
            // std::cout << "key { " << i << " } already exists" << std::endl;
        }
#elif INDEX_PATTERN == INDEX_USE_OCH
        Value *value = new Value{i};
        Table.put(i, value, 0);
#endif
    }
}

// make procedure for YCSB workload (read/write only)
void makeProcedure(std::vector<Procedure> &pro, Xoroshiro128Plus &rnd) {
    pro.clear();
    for (int i = 0; i < MAX_OPE; i++) {
        uint64_t tmpkey, tmpope;
        tmpkey = rnd.next() % TUPLE_NUM;
        if ((rnd.next() % 100) < RRAITO) {
            pro.emplace_back(OpType::READ, tmpkey, tmpkey);
        } else {
            pro.emplace_back(OpType::WRITE, tmpkey, tmpkey);
        }
    }
}

void ecall_sendStart() { __atomic_store_n(&start, true, __ATOMIC_RELEASE); }
void ecall_sendQuit() { __atomic_store_n(&quit, true, __ATOMIC_RELEASE); }

unsigned get_rand() {
    // 乱数生成器（引数にシードを指定可能）, [0, (2^32)-1] の一様分布整数を生成
    static std::mt19937 mt32(0);
    return mt32();
}

void ecall_worker_th(int thid, int gid) {
    TxExecutor trans(thid);
    WorkerResult &myres = std::ref(workerResults[thid]);
    uint64_t epoch_timer_start, epoch_timer_stop;
    
    unsigned init_seed;
    init_seed = get_rand();
    // sgx_read_rand((unsigned char *) &init_seed, 4);
    Xoroshiro128Plus rnd(init_seed);

    Logger *logger;
    std::atomic<Logger*> *logp = &(logs[gid]);  // loggerのthreadIDを指定したいからgidを使う
    for (;;) {
        logger = logp->load();
        if (logger != 0) break;
        waitTime_ns(100);
    }
    logger->add_tx_executor(trans);

    __atomic_store_n(&readys[thid], 1, __ATOMIC_RELEASE);

    while (true) {
        if (__atomic_load_n(&start, __ATOMIC_ACQUIRE)) break;
    }

    if (thid == 0) epoch_timer_start = rdtscp();

    while (true) {
        if (__atomic_load_n(&quit, __ATOMIC_ACQUIRE)) break;
        
#ifdef ADD_ANALYSIS
        uint64_t start_make_procedure = rdtscp();
#endif
        makeProcedure(trans.pro_set_, rnd); // ocallで生成したprocedureをTxExecutorに移し替える
#ifdef ADD_ANALYSIS
        uint64_t end_make_procedure = rdtscp();
        myres.local_make_procedure_time_ += end_make_procedure - start_make_procedure;
#endif
    RETRY:

#ifdef ADD_ANALYSIS
        uint64_t start_durable_epoch_work = rdtscp();
#endif
        // if (thid == 0) leaderWork(epoch_timer_start, epoch_timer_stop);
        trans.durableEpochWork(epoch_timer_start, epoch_timer_stop, quit);
#ifdef ADD_ANALYSIS
        uint64_t end_durable_epoch_work = rdtscp();
        myres.local_durable_epoch_work_time_ += end_durable_epoch_work - start_durable_epoch_work;
#endif
        if (__atomic_load_n(&quit, __ATOMIC_ACQUIRE)) break;

        Status status;

        trans.begin();
        for (auto itr = trans.pro_set_.begin(); itr != trans.pro_set_.end(); itr++) {
            if ((*itr).ope_ == OpType::READ) {
#if INDEX_PATTERN == INDEX_USE_OCH
                trans.read((*itr).key_);
#elif INDEX_PATTERN == INDEX_USE_MASSTREE
                status = trans.read((*itr).key_);
                if (status == Status::WARN_NOT_FOUND) {
                    trans.abort();
                    myres.local_abort_count_++;
                    goto RETRY;
                }
#endif
            } else if ((*itr).ope_ == OpType::WRITE) {
#if INDEX_PATTERN == INDEX_USE_OCH
                trans.write((*itr).key_, (*itr).value_);
#elif INDEX_PATTERN == INDEX_USE_MASSTREE
                status = trans.write((*itr).key_, (*itr).value_);
                if (status == Status::WARN_NOT_FOUND) {
                    trans.abort();
                    myres.local_abort_count_++;
                    goto RETRY;
                }
#endif
            } else {
                // ERR;
                assert(false);
            }
        }
   
        if (trans.validationPhase()) {
            trans.writePhase();
            storeRelease(myres.local_commit_count_, loadAcquire(myres.local_commit_count_) + 1);
        } else {
            trans.abort();
            myres.local_abort_count_++;
            goto RETRY;
        }
    }

    trans.log_buffer_pool_.terminate();
    logger->worker_end(thid);

    return;
}

void ecall_logger_th(int thid) {
    Logger logger(thid, std::ref(notifier), std::ref(loggerResults[thid]));
    notifier.add_logger(&logger);
    std::atomic<Logger*> *logp = &(logs[thid]);
    logp->store(&logger);
    logger.worker();
    return;
}

uint64_t ecall_getAbortCount(int thid) {
    return workerResults[thid].local_abort_count_;
}

uint64_t ecall_getCommitCount(int thid) {
    return workerResults[thid].local_commit_count_;
}

uint64_t ecall_getSpecificAbortCount(int thid, int reason) {
    switch (reason) {
        case AbortReason::ValidationPhase1:
            return workerResults[thid].local_abort_vp1_count_;
        case AbortReason::ValidationPhase2:
            return workerResults[thid].local_abort_vp2_count_;
        case AbortReason::ValidationPhase3:
            return workerResults[thid].local_abort_vp3_count_;
        case AbortReason::NullCurrentBuffer:
            return workerResults[thid].local_abort_nullBuffer_count_;
        default:
            assert(false);  // ここに来てはいけない
            return 0;
    }
}

#ifdef ADD_ANALYSIS
uint64_t ecall_get_analysis(int thid, int type) {
    switch (type) {
        case 0:
            return workerResults[thid].local_read_time_;
        case 1:
            return workerResults[thid].local_read_internal_time_;
        case 2:
            return workerResults[thid].local_write_time_;
        case 3:
            return workerResults[thid].local_validation_time_;
        case 4:
            return workerResults[thid].local_write_phase_time_;
        case 5:
            return workerResults[thid].local_masstree_read_get_value_time_;
        case 6:
            return workerResults[thid].local_durable_epoch_work_time_;
        case 7:
            return workerResults[thid].local_make_procedure_time_;
        case 8:
            return workerResults[thid].local_masstree_write_get_value_time_;
        default:
            assert(false);  // ここに来てはいけない
            return 0;
    }
}
#endif

uint64_t ecall_getLoggerCount(int thid, int type) {
	switch (type) {
		case LoggerResultType::ByteCount:
			return loggerResults[thid].byte_count_;
		case LoggerResultType::WriteLatency:
			return loggerResults[thid].write_latency_;
        case LoggerResultType::WaitLatency:
            return loggerResults[thid].wait_latency_;
		default: 
			assert(false);  // ここに来てはいけない
            return 0;
	}
}

uint64_t ecall_showDurableEpoch() {
    uint64_t min_dl = __atomic_load_n(&(ThLocalDurableEpoch[0]), __ATOMIC_ACQUIRE);
    for (unsigned int i=1; i < LOGGER_NUM; ++i) {
        uint64_t dl = __atomic_load_n(&(ThLocalDurableEpoch[i]), __ATOMIC_ACQUIRE);
        if (dl < min_dl) {
            min_dl = dl;
        }
    }
    return min_dl;
}