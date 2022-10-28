#pragma once

//common
#define CLOCKS_PER_US 4000  // CPU_MHz. Use this info for measuring time. (i5-10440F, about 4GHz?)
#define EPOCH_TIME 40       // Epoch interval[ms].
#define EXTIME 3            // Execution time[sec].
#define MAX_OPE 10          // Total number of operations per single transaction.
#define RMW false           // (Read Modify Write)True means read modify write, false means blind write.
#define RRAITO 50           // read ratio of single transaction.
#define THREAD_NUM 9        // Total number of worker threads.
#define TUPLE_NUM 1000000   // Total number of records.
#define YCSB false          // True uses zipf_skew, false uses faster random generator.
#define ZIPF_SKEW 0         // zipf skew. 0 ~ 0.999...

//common_log    comment outはまだ未実装というか出てきてないので
// #define EPOCH_TIME_US 0
#define LOGGER_NUM 3        // Total number of logger threads.
// #define AFFINITY ""  定義予定なし
// #define NOTIFIER_CPU -1
#define BUFFER_NUM 2        // Number of log buffers per logger thread.
#define BUFFER_SIZE 512     // Size of log buffer in KiB
#define EPOCH_DIFF 0        // Epoch difference threshold to stop transaction (0 for no stop), というかEpoch同期法で採用しているやつ？だから使わないかも
// #define LATENCT_LOG false

// compile options
#define VAL_SIZE 4

#define PAGE_SIZE 4096

#define LOGSET_SIZE 1000