/**
 * This file contains macro definitions configuring various aspects of 
 * the database system program, including thread, data, execution, 
 * time, buffer, data size, display, optimization, and more.
 *
 * These macro definitions and data structures are shared between
 * App and Enclave, ensuring consistent data and mutual understanding,
 * thereby improving the readability and simplification of the 
 * overall system.
 */

#pragma once

#include "consts_benchmark.h"
#include "structures.h"

// -------------------
// Thread configurations
// -------------------
// The number of worker threads.
#define WORKER_NUM 16
// The number of logger threads.
#define LOGGER_NUM 16

// -------------------
// Time configurations
// -------------------
// The epoch duration in milliseconds.
#define EPOCH_TIME 40
// Clocks per microsecond for the target hardware.
#define CLOCKS_PER_US 2700

// -------------------
// Buffer configurations
// -------------------
// The number of buffers.
#define BUFFER_NUM 65536
// The maximum number of log entries that can be buffered before triggering a publish.
#define MAX_BUFFERED_LOG_ENTRIES 1000
// The epoch difference.
#define EPOCH_DIFF 1

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