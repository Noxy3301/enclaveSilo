#pragma once

#include <cstddef>

// common settings
#define THREAD_NUM 9        // Number of worker thread
#define LOGGER_NUM 3        // Number of logger thread
#define EPOCH_TIME 40       // Epoch interval[msec]
#define CLOCKS_PER_US 3500  // CPU_MHz (Use this info for measuring time)
#define EXTIME 3            // Execution time[sec]
#define CACHE_LINE_SIZE 64  // CPU cache line size

// YCSB settings
#define TUPLE_NUM 1000000   // Total number of records
#define MAX_OPE 10          // Total number of operations per single transaction
#define RRAITO 50           // read ratio of single transaction
#define ZIPF_SKEW 0         // zipf skew (0 ~ 0.999...)

// Logger settings
#define BUFFER_NUM 2
#define BUFFER_SIZE 512
#define EPOCH_DIFF 0
#define LOGSET_SIZE 1000

#define VAL_SIZE 4
#define PAGE_SIZE 4096

// show result details flag
#define SHOW_DETAILS 1  // If true(1), show settings, each worker's result, and details (e.g., latency, abort rate ...)
#define ADD_ANALYSIS 1  // If true(1), store the abort reason (e.g., validation_1, validation_2 ...)

// no wait optimization
#define NO_WAIT_LOCKING_IN_VALIDATION 1

// Index pattern
// 0: OCH       - Using Optimistic Cuckoo Hashing as index.
// 1: linear    - No index, need to linear scan to search item.
// 2: Masstree  - Using Masstree as index. wip
#define INDEX_PATTERN 0

// Benchmark pattern
// 0: Use TPC-C-NP benchmark. wip
// 1: Use YCSB benchmark.
#define BENCHMARK 0

// TPC-C-NP settings
#define TPCC_NUM_WH 1            // The number of warehouses
#define TPCC_PERC_PAYMENT 50     // The percentage of Payment transactions
#define TPCC_PERC_ORDER_STATUS 0 // The percentage of Order-Status transactions
#define TPCC_PERC_DELIVERY 0     // The percentage of Delivery transactions
#define TPCC_PERC_STOCK_LEVEL 0  // The percentage of Stock-Level transactions
#define TPCC_INTERACTIVE_MS 0    // Sleep milliseconds per SQL(-equivalent) unit

constexpr std::size_t DIST_PER_WARE{10};    // default 10
constexpr std::size_t MAX_ITEMS{100000};    // default 100000
constexpr std::size_t CUST_PER_DIST{3000};  // default 3000
constexpr std::size_t LASTNAME_LEN{16};     // default 16

// #define DEBUG 1