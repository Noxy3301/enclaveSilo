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

#include "utility/app_util.h"
#include "logger_affinity/logger_affinity.h"
#include "../Include/consts.h"
#include "../Include/result.h"

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

void worker_th(int thid, int gid) {
    ecall_worker_th(global_eid, thid, gid);
}

void logger_th(int thid) {
    ecall_logger_th(global_eid, thid);
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
    DisplayResults results; // 結果を保持・表示するオブジェクト
    LoggerAffinity affin;   // ロガーとワーカースレッドのアフィニティを管理するオブジェクト
    std::vector<std::thread> logger_threads;    // ロガースレッドを保持するベクター
    std::vector<std::thread> worker_threads;    // ワーカースレッドを保持するベクター

#if SHOW_DETAILS
    results.displayParameter();
#endif
    // DB作成開始のタイムスタンプを追加する
    results.addTimestamp();

    /* Initialize the enclave */
    if(initialize_enclave() < 0) {
        printf("Enter a character before exit ...\n");
        getchar();
        return -1; 
    }
    // DBを初期化する
    ecall_initDB(global_eid);
    
    // ログファイルの作成
    create_logfiles(LOGGER_NUM);

    // スレッドのアフィニティオブジェクトを初期化する
    affin.init(WORKER_NUM, LOGGER_NUM);

    // スレッド作成開始のタイムスタンプを追加する
    results.addTimestamp();

    // CPUアフィニティに基づいてロガーとワーカースレッドを作成する
    int i = 0, j = 0;
    for (auto itr = affin.nodes_.begin(); itr != affin.nodes_.end(); itr++, j++) {
        logger_threads.emplace_back(logger_th, j);    // TODO: add some arguments
        for (auto wcpu = itr->worker_cpu_.begin(); wcpu != itr->worker_cpu_.end(); wcpu++, i++) {
            worker_threads.emplace_back(worker_th, i, j);  // TODO: add some arguments
        }
    }

    // すべてのスレッドが準備完了するのを待つ
    ecall_waitForReady(global_eid);

    // スレッドの実行開始のタイムスタンプを追加する
    results.addTimestamp();

    // スレッドの実行を開始し、所定の時間待つ
    ecall_sendStart(global_eid);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 * EXTIME));
    ecall_sendQuit(global_eid);

    // スレッドの破壊開始のタイムスタンプを追加する
    results.addTimestamp();

    // すべてのロガーとワーカースレッドが完了するのを待つ
    for (auto &th : logger_threads) th.join();
    for (auto &th : worker_threads) th.join();

    // スレッドの破壊終了のタイムスタンプを追加する
    results.addTimestamp();
    
    // ワーカーとロガースレッドからの結果を収集し、表示する
    results.getWorkerResult();
    results.getLoggerResult();
    results.displayResult();

    sgx_destroy_enclave(global_eid);

    return 0;
}

