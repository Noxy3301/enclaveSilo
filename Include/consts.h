#pragma once

#define THREAD_NUM 9
#define LOGGER_NUM 3
#define TUPLE_NUM 1000000
//#define TUPLE_NUM 100
#define EXTIME 3
#define CACHE_LINE_SIZE 64

#define MAX_OPE 10
#define RRAITO 50
#define YCSB false
#define ZIPF_SKEW 0

#define EPOCH_TIME 40
#define CLOCKS_PER_US 2900

#define BUFFER_NUM 2
#define BUFFER_SIZE 512
#define EPOCH_DIFF 0

#define VAL_SIZE 4
#define PAGE_SIZE 4096
#define LOGSET_SIZE 1000

// Index pattern
// 0: ideal  - Key can be used as an array index. Thus, O(1) [Default]
// 1: linear - No index, need to linear scan to search item.
// 2: OCH    - Using Optimistic Cuckoo Hashing as index. wip
#define INDEX_PATTERN 0

#include <stdint.h>

struct returnResult {
    uint64_t local_abort_counts_ = 0;
    uint64_t local_commit_counts_ = 0;
};