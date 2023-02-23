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

#include "include/logger.h"
#include "include/notifier.h"

#include "include/atomic_tool.h"
#include "include/atomic_wrapper.h"
#include "include/silo_op_element.h"
#include "include/transaction.h"
#include "include/tsc.h"
#include "include/util.h"
#include "include/zipf.h"

#include "include/debug.h"


std::vector<Tuple> Table(TUPLE_NUM);
std::vector<uint64_t> ThLocalEpoch(THREAD_NUM);
std::vector<uint64_t> CTIDW(THREAD_NUM);
std::vector<uint64_t> ThLocalDurableEpoch(LOGGER_NUM);
uint64_t DurableEpoch;
uint64_t GlobalEpoch = 1;
std::vector<returnResult> results(THREAD_NUM);
std::atomic<Logger *> logs[LOGGER_NUM];
Notifier notifier;
std::vector<int> readys(THREAD_NUM);

bool start = false;
bool quit = false;

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

void ecall_sendStart() {
    __atomic_store_n(&start, true, __ATOMIC_RELEASE);
}

void ecall_sendQuit() {
    __atomic_store_n(&quit, true, __ATOMIC_RELEASE);
}


void ecall_worker_th(int thid, int gid) {
    TxExecutor trans(thid);
    returnResult &myres = std::ref(results[thid]);
    uint64_t epoch_timer_start, epoch_timer_stop;
    
    unsigned init_seed;
    sgx_read_rand((unsigned char *) &init_seed, 4);
    Xoroshiro128Plus rnd(init_seed);

    Logger *logger;
    std::atomic<Logger*> *logp = &(logs[gid]);  // loggerのthreadIDを指定したいからgidを使う
    for (;;) {
        logger = logp->load();
        if (logger != 0) break;
        waitTime_ns(100);
        // std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }
    logger->add_tx_executor(trans);

    __atomic_store_n(&readys[thid], 1, __ATOMIC_RELEASE);

    while (true) {
        if (__atomic_load_n(&start, __ATOMIC_ACQUIRE)) break;
    }

    if (thid == 0) epoch_timer_start = rdtscp();

    while (true) {
        if (__atomic_load_n(&quit, __ATOMIC_ACQUIRE)) break;
        
        makeProcedure(trans.pro_set_, rnd); // ocallで生成したprocedureをTxExecutorに移し替える

    RETRY:

        if (thid == 0) leaderWork(epoch_timer_start, epoch_timer_stop);
        trans.durableEpochWork(epoch_timer_start, epoch_timer_stop, quit);
        
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

    trans.log_buffer_pool_.terminate();
    logger->worker_end(thid);

    return;
}

void ecall_logger_th(int thid) {
    Logger logger(thid, std::ref(notifier));
    notifier.add_logger(&logger);
    std::atomic<Logger*> *logp = &(logs[thid]);
    logp->store(&logger);
    logger.worker();
    return;
}

uint64_t ecall_getAbortResult(int thid) {
    return results[thid].local_abort_counts_;
}

uint64_t ecall_getCommitResult(int thid) {
    return results[thid].local_commit_counts_;
}

void ecall_showLoggerResult(int thid) {
    Logger *logger;
    std::atomic<Logger*> *logp = &(logs[thid]);
    logger = logp->load();
    logger->show_result();
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