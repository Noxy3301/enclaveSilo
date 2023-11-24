#pragma once

// -------------------
// Data configurations
// -------------------
// The total number of tuples.
#define TUPLE_NUM 1000000

// -------------------
// Execution configurations
// -------------------
// The execution time in seconds.
#define EXTIME 10
// The size of a cache line in bytes.
#define CACHE_LINE_SIZE 64

// -------------------
// Operation configurations
// -------------------
// The maximum number of operations.
#define MAX_OPE 10
// The ratio (percentage) of read operations.
#define RRAITO 50
// Flag to determine if using YCSB (Yahoo! Cloud Serving Benchmark).
#define YCSB false
// The skewness for Zipf distribution. 0 indicates uniform distribution.
#define ZIPF_SKEW 0

// -------------------
// Data size configurations
// -------------------
// The size of the value in bytes.
#define VAL_SIZE 4
// The size of a page in bytes.
#define PAGE_SIZE 4096
// The size of a log set.
#define LOGSET_SIZE 1000

// -------------------
// Display configurations
// -------------------
// Flag to determine if detailed results should be shown. 0 for no, 1 for yes.
#define SHOW_DETAILS 1

// -------------------
// Optimization configurations
// -------------------
// Flag for "no-wait" optimization during validation. 1 to enable, 0 to disable.
#define NO_WAIT_LOCKING_IN_VALIDATION 1

// -------------------
// Indexing configurations
// -------------------
// Index pattern definitions:
// 0: INDEX_USE_MASSTREE
//      - A high-performance, concurrent, ordered index (B+ Tree) suitable for multi-core systems.
// 1: INDEX_USE_OCH
//      - A high-performance, concurrent hashing technique with efficient handling of hash collisions.
#define INDEX_PATTERN 1

#define ADD_ANALYSIS 1