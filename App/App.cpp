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

using namespace std;

#include "../Include/consts.h"
#include "../Include/util.h"
#include "../Include/logger.h"

#include "include/notifier.h"
#include "include/result.h"
#include "include/random.h"
#include "include/zipf.h"

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




void worker_th(int thid, char &ready, const bool &start, const bool &quit, std::atomic<Logger*> *logp) {
    ResultLog &myres_log = std::ref(SiloResult[thid]);
    Result &myres = std::ref(myres_log.result_);
    Xoroshiro128Plus rnd;
    rnd.init();
    FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

    __atomic_store_n(&ready, 1, __ATOMIC_RELEASE);
    while (true) {
        if (__atomic_load_n(&start, __ATOMIC_ACQUIRE)) break;
    }

    if (thid == 0) // epoch_timer_start = rdtscp();

    while (true) {
        if (__atomic_load_n(&quit, __ATOMIC_ACQUIRE)) break;
        // [enclave] make Procedure
        // [enclave] execute Transaction
        int ret = -1;
        ecall_execTransaction(global_eid, &ret, rnd.next(), rnd.next(), zipf(), thid);

    }
}

void logger_th(int thid, Notifier &notifier, std::atomic<Logger*> *logp) {
}

void waitForReady(const std::vector<char> &readys) {
    while (true) {
        bool failed = false;
        for (const auto &ready : readys) {
            if (!__atomic_load_n(&ready, __ATOMIC_ACQUIRE)) {
                failed = true;
            }
        }
        if (!failed) break;
    }
}

/* Application entry */
int SGX_CDECL main() {

    chrono::system_clock::time_point p1, p2, p3, p4;

    std::cout << "esilo: Silo_logging running within Enclave" << std::endl;

    p1 = chrono::system_clock::now();

    /* Initialize the enclave */
    if(initialize_enclave() < 0) {
        printf("Enter a character before exit ...\n");
        getchar();
        return -1; 
    }

    displayParameter();

    p2 = chrono::system_clock::now();

    ecall_makeDB(global_eid);

    p3 = chrono::system_clock::now();

    LoggerAffinity affin;   // Nodeごとに管理する用のやつ
    affin.init(THREAD_NUM, LOGGER_NUM);

    for (int i = 0; i < LOGGER_NUM; i++) {
        std::cout << "[info]\tNode#" << i << "\t";
        std::cout << "worker:";
        for (int j = 0; j < affin.nodes_[i].worker_cpu_.size(); j++) {
            std::cout << affin.nodes_[i].worker_cpu_[j] << ((j != affin.nodes_[i].worker_cpu_.size()-1) ? "," : "");
        }
        std::cout << "\tlogger:" << affin.nodes_[i].logger_cpu_ << std::endl;
    }

    bool start = false;
    bool quit = false;

    std::vector<char> readys(THREAD_NUM);
    std::atomic<Logger *> logs[LOGGER_NUM];
    Notifier notifier;
    std::vector<std::thread> lthv;  // logger threads
    std::vector<std::thread> wthv;  // worker threads

    int i = 0, j = 0;
    for (auto itr = affin.nodes_.begin(); itr != affin.nodes_.end(); itr++, j++) {
        int lcpu = itr->logger_cpu_;
        logs[j].store(0);
        lthv.emplace_back(logger_th, j, std::ref(notifier), &(logs[j]));
        for (auto wcpu = itr->worker_cpu_.begin(); wcpu != itr->worker_cpu_.end(); wcpu++, i++) {
            wthv.emplace_back(worker_th, i, std::ref(readys[i]), std::ref(start), std::ref(quit), &(logs[j]));  // logsはj番目(loggerと共通のやつ)
        }
    }

    waitForReady(readys);
    __atomic_store_n(&start, true, __ATOMIC_RELEASE);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 * EXTIME));
    __atomic_store_n(&quit, true, __ATOMIC_RELEASE);

    for (auto &th : lthv) th.join();
    for (auto &th : wthv) th.join();

    p4 = chrono::system_clock::now();

    double duration1 = static_cast<double>(chrono::duration_cast<chrono::microseconds>(p2 - p1).count() / 1000.0);
    double duration2 = static_cast<double>(chrono::duration_cast<chrono::microseconds>(p3 - p2).count() / 1000.0);
    double duration3 = static_cast<double>(chrono::duration_cast<chrono::microseconds>(p4 - p3).count() / 1000.0);

    std::cout << "[info]\tcreateEnclave:\t" << duration1/1000 << "s.\n";
    std::cout << "[info]\tmakeDB:\t\t" << duration2/1000 << "s.\n";
    std::cout << "[info]\texecutionTime:\t" << duration3/1000 << "s.\n";

    /* Destroy the enclave */
    sgx_destroy_enclave(global_eid);
    printf("[info]\tSampleEnclave successfully returned.\n");
    printf("[info]\tEnter a character before exit ...\n");
    getchar();
    return 0;
}
