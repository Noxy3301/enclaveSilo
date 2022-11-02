#define THREAD_NUM 10
#define LOGGER_NUM 1
#define TUPLE_NUM 1000000
#define EXTIME 3
#define CACHE_LINE_SIZE 64

#define MAX_OPE 10
#define RRAITO 50
#define YCSB false
#define ZIPF_SKEW 0

#define EPOCH_TIME 40
#define CLOCKS_PER_US 2900

struct returnResult {
    uint64_t local_abort_counts_ = 0;
    uint64_t local_commit_counts_ = 0;
};