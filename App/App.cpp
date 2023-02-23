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


#include <stdio.h>
#include <string.h>
#include <assert.h>

# include <unistd.h>
# include <pwd.h>
# define MAX_PATH FILENAME_MAX

#include "sgx_urts.h"
#include "App.h"
#include "Enclave_u.h"

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

// logfile生成用
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream> //filestream

#include "include/random.h"
#include "include/zipf.h"
#include "../Include/consts.h"

using namespace std;


/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    { SGX_ERROR_UNEXPECTED, "Unexpected error occurred.", NULL },
    { SGX_ERROR_INVALID_PARAMETER, "Invalid parameter.", NULL },
    { SGX_ERROR_OUT_OF_MEMORY, "Out of memory.", NULL},
    { SGX_ERROR_ENCLAVE_LOST, "Power transition occurred.", "Please refer to the sample \"PowerTransition\" for details."},
    { SGX_ERROR_INVALID_ENCLAVE, "Invalid enclave image.", NULL},
    { SGX_ERROR_INVALID_ENCLAVE_ID, "Invalid enclave identification.", NULL},
    { SGX_ERROR_INVALID_SIGNATURE, "Invalid enclave signature.", NULL},
    { SGX_ERROR_OUT_OF_EPC, "Out of EPC memory.", NULL},
    { SGX_ERROR_NO_DEVICE, "Invalid SGX device.", "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."},
    { SGX_ERROR_MEMORY_MAP_CONFLICT, "Memory map conflicted.", NULL},
    { SGX_ERROR_INVALID_METADATA, "Invalid enclave metadata.", NULL},
    { SGX_ERROR_DEVICE_BUSY, "SGX device was busy.", NULL},
    { SGX_ERROR_INVALID_VERSION, "Enclave version was invalid.", NULL},
    { SGX_ERROR_INVALID_ATTRIBUTE, "Enclave was not authorized.", NULL},
    { SGX_ERROR_ENCLAVE_FILE_ACCESS, "Can't open enclave file.", NULL},
};

/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret) {
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist/sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++) {
        if(ret == sgx_errlist[idx].err) {
            if(NULL != sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }
    
    if (idx == ttl)
    	printf("Error code is 0x%X. Please refer to the \"Intel SGX SDK Developer Reference\" for more details.\n", ret);
}

/* Initialize the enclave:
 *   Call sgx_create_enclave to initialize an enclave instance
 */
int initialize_enclave(void) {
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    
    /* Call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }

    return 0;
}

/* OCall functions */
void ocall_print_string(const char *str) {
    /* Proxy/Bridge will check the length and null-terminate 
     * the input string to prevent buffer overflow. 
     */
    printf("%s", str);
}

// MARK: class(LoggerAffinity)

class LoggerNode {
    public:
        int logger_cpu_;
        std::vector<int> worker_cpu_;

        LoggerNode() {}
};

class LoggerAffinity {
    public:
        std::vector<LoggerNode> nodes_;
        unsigned worker_num_ = 0;
        unsigned logger_num_ = 0;
        void init(unsigned worker_num, unsigned logger_num);
};

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

// MARK: siloResult

class Result {
    public:
        uint64_t local_abort_counts_ = 0;
        uint64_t local_commit_counts_ = 0;
        uint64_t total_abort_counts_ = 0;
        uint64_t total_commit_counts_ = 0;
};

std::vector<Result> SiloResult(THREAD_NUM);

// MARK: thread function

void worker_th(int thid, int gid) {
    returnResult ret;
    ecall_worker_th(global_eid, thid, gid);  // thread.emplace_backで直接渡せる気がしないし、こっちで受け取ってResultの下処理をしたい
    ecall_getAbortResult(global_eid, &ret.local_abort_counts_, thid);
    ecall_getCommitResult(global_eid, &ret.local_commit_counts_, thid);

    SiloResult[thid].local_commit_counts_ = ret.local_commit_counts_ ;
    SiloResult[thid].local_abort_counts_ = ret.local_abort_counts_;
    // std::cout << "worker_end" << std::endl;
}

void logger_th(int thid) {
    ecall_logger_th(global_eid, thid);
}

// MARK: utilities

void waitForReady(const std::vector<int> &readys) {
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

// Xoroshiro128Plus rnd;
// FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);  //関数内で宣言すると割り算処理繰り返すからクソ重いぞ！

void displayParameter() {
    cout << "#clocks_per_us:\t" << CLOCKS_PER_US << endl;
    cout << "#epoch_time:\t" << EPOCH_TIME << endl;
    cout << "#extime:\t" << EXTIME << endl;
    cout << "#max_ope:\t" << MAX_OPE << endl;
    // cout << "#rmw:\t\t" << RMW << endl;
    cout << "#rratio:\t" << RRAITO << endl;
    cout << "#thread_num:\t" << THREAD_NUM << endl;
    cout << "#tuple_num:\t" << TUPLE_NUM << endl;
    cout << "#ycsb:\t\t" << YCSB << endl;
    cout << "#zipf_skew:\t" << ZIPF_SKEW << endl;
    cout << "#logger_num:\t" << LOGGER_NUM << endl;
    cout << "#buffer_num:\t" << BUFFER_NUM << endl;
    cout << "#buffer_size:\t" << BUFFER_SIZE << endl;
}


uint64_t total_commit_counts_ = 0;
uint64_t total_abort_counts_ = 0;

void displayResult() {
    // 各threadのcommit/abort数表示
    for (int i = 0; i < THREAD_NUM; i++) {
        cout << "thread#" << i << "\tcommit: " << SiloResult[i].local_commit_counts_ << "\tabort:" << SiloResult[i].local_abort_counts_ << endl;
        total_commit_counts_ += SiloResult[i].local_commit_counts_;
        total_abort_counts_ += SiloResult[i].local_abort_counts_;
    }

    cout << "[info]\tcommit_counts_:\t" << total_commit_counts_ << endl;
    cout << "[info]\tabort_counts_:\t" << total_abort_counts_ << endl;
    cout << "[info]\tabort_rate:\t" << (double)total_abort_counts_ / (double)(total_commit_counts_ + total_abort_counts_) << endl;

    uint64_t result = total_commit_counts_ / EXTIME;
    cout << "[info]\tlatency[ns]:\t" << powl(10.0, 9.0) / result * THREAD_NUM << endl;
    cout << "[info]\tthroughput[tps]:\t" << result << endl;
}

void create_logfiles(int logger_num) {
    // 複数回テストするときにファイル処理がめんどいのでコマンドで消し飛ばす
    char command[128];
	sprintf(command, "rm -f -r logs");
	system(command);
    
    if (::mkdir("logs", 0755)) printf("ERR!");
    // log*.sealの生成
    for (int i = 0; i < logger_num; i++) {
        std::string filename = "logs/log" + to_string(i) + ".seal";
        int fd_ = ::open(filename.c_str(), O_WRONLY|O_CREAT, 0644);
        if (fd_ == -1) {
                perror("open failed");
                std::abort();
        }
    }
    // pepochの生成
    std::string filename = "logs/pepoch.seal";
    int fd_ = ::open(filename.c_str(), O_WRONLY|O_CREAT, 0644);
    if (fd_ == -1) {
            perror("open failed");
            std::abort();
    }
}

void write_sealData(std::string filePath, const uint8_t* sealed_data, const size_t sealed_size) {
    int fd_;
    fd_ = ::open(filePath.c_str(), O_WRONLY|O_APPEND);
    size_t s = 0;
    while (s < sealed_size) {  // writeは最大size - s(byte)だけ書き出すから事故対策でwhile loopしている？
        ssize_t r = ::write(fd_, (char*)sealed_data + s, sealed_size - s);    // returnは書き込んだbyte数
        if (r <= 0) {
            perror("write failed");
            std::abort();
        }
        s += r;
    }

    if (::fsync(fd_) == -1) {
        perror("fsync failed");
        std::abort();
    }

    if (::close(fd_) == -1) {
        perror("close failed");
        std::abort();
    }
}

std::atomic<int> ocall_count(0);

int ocall_save_logfile(int thid, const uint8_t* sealed_data, const size_t sealed_size) {
    int expected = ocall_count.load();
    while (!ocall_count.compare_exchange_weak(expected, expected + 1));
    std::string filePath = "logs/log" + to_string(thid) + ".seal";
    write_sealData(filePath, sealed_data, sealed_size);
    return 0;
}

int ocall_save_pepochfile(const uint8_t* sealed_data, const size_t sealed_size) {
    int expected = ocall_count.load();
    while (!ocall_count.compare_exchange_weak(expected, expected + 1));
    std::string filePath = "logs/pepoch.seal";
    write_sealData(filePath, sealed_data, sealed_size);
    return 0;
}


/* Application entry */
int SGX_CDECL main() {

    chrono::system_clock::time_point p1, p2, p3, p4, p5, p6;

    std::cout << "esilo: Silo_logging running within Enclave" << std::endl;
    std::cout << "ported from silo_minimum(commitID:a720168)" << std::endl;
    displayParameter();

    p1 = chrono::system_clock::now();

    /* Initialize the enclave */
    if(initialize_enclave() < 0) {
        printf("Enter a character before exit ...\n");
        getchar();
        return -1; 
    }

    p2 = chrono::system_clock::now();

    ecall_initDB(global_eid);
    LoggerAffinity affin;
    affin.init(THREAD_NUM, LOGGER_NUM); // logger/worker実行threadの決定

    std::vector<std::thread> lthv;
    std::vector<std::thread> wthv;

    create_logfiles(LOGGER_NUM);

    p3 = chrono::system_clock::now();

    int i = 0, j = 0;
    for (auto itr = affin.nodes_.begin(); itr != affin.nodes_.end(); itr++, j++) {
        lthv.emplace_back(logger_th, j);    // TODO: add some arguments
        for (auto wcpu = itr->worker_cpu_.begin(); wcpu != itr->worker_cpu_.end(); wcpu++, i++) {
            wthv.emplace_back(worker_th, i, j);  // TODO: add some arguments
        }
    }
    
    ecall_waitForReady(global_eid);
    p4 = chrono::system_clock::now();
    ecall_sendStart(global_eid);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 * EXTIME));
    ecall_sendQuit(global_eid);

    p5 = chrono::system_clock::now();

    for (auto &th : lthv) th.join();
    for (auto &th : wthv) th.join();

    p6 = chrono::system_clock::now();

    for (int i = 0; i < LOGGER_NUM; i++) {
        ecall_showLoggerResult(global_eid, i);
    }

    double duration1 = static_cast<double>(chrono::duration_cast<chrono::microseconds>(p2 - p1).count() / 1000.0);
    double duration2 = static_cast<double>(chrono::duration_cast<chrono::microseconds>(p3 - p2).count() / 1000.0);
    double duration3 = static_cast<double>(chrono::duration_cast<chrono::microseconds>(p4 - p3).count() / 1000.0);
    double duration4 = static_cast<double>(chrono::duration_cast<chrono::microseconds>(p5 - p4).count() / 1000.0);
    double duration5 = static_cast<double>(chrono::duration_cast<chrono::microseconds>(p6 - p5).count() / 1000.0);

    displayResult();

    std::cout << "[info]\tcreateEnclave:\t" << duration1/1000 << "s.\n";
    std::cout << "[info]\tmakeDB:\t\t" << duration2/1000 << "s.\n";
    std::cout << "[info]\tcreateThread:\t" << duration3/1000 << "s.\n";
    std::cout << "[info]\texecutionTime:\t" << duration4/1000 << "s.\n";
    std::cout << "[info]\tdestroyThread:\t" << duration5/1000 << "s.\n";
    std::cout << "[info]\tocall_count(write):\t" << ocall_count.load() << std::endl;
    uint64_t ret_durableEpoch = 0;
    ecall_showDurableEpoch(global_eid, &ret_durableEpoch);
    std::cout << "[info]\tdurableEpoch:\t" << ret_durableEpoch << std::endl;

    std::cout << "=== for copy&paste ===" << std::endl;
    std::cout << total_commit_counts_ << std::endl;
    std::cout << total_abort_counts_ << std::endl;
    std::cout << (double)total_abort_counts_ / (double)(total_commit_counts_ + total_abort_counts_) << std::endl;
    uint64_t result = total_commit_counts_ / EXTIME;
    std::cout << powl(10.0, 9.0) / result * THREAD_NUM << std::endl;;  // latency
    std::cout << ret_durableEpoch << std::endl;
    std::cout << result << std::endl;; // throughput

    /* Destroy the enclave */
    sgx_destroy_enclave(global_eid);
    printf("[info]\tSampleEnclave successfully returned.\n");
    printf("[info]\tEnter a character before exit ...\n");
    getchar();
    return 0;
}

