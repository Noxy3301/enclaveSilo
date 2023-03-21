#include "include/util.h"

#include "Enclave_t.h"
#include "sgx_thread.h"

#include "include/util.h"
#include "include/ycsb.h"

bool chkEpochLoaded() {
    uint64_t nowepo = atomicLoadGE();
    // leader_workを実行しているのはthid:0だからforは1から回している？
    for (unsigned int i = 1; i < THREAD_NUM; i++) {
        if (__atomic_load_n(&(ThLocalEpoch[i]), __ATOMIC_ACQUIRE) != nowepo) return false;
    }
    return true;
}

// void leaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop) {
//     epoch_timer_stop = rdtscp();
//     if (chkClkSpan(epoch_timer_start, epoch_timer_stop, EPOCH_TIME * CLOCKS_PER_US * 1000) && chkEpochLoaded()) {
//         atomicAddGE();
//         epoch_timer_start = epoch_timer_stop;
//     }
// }

void siloLeaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop) {
    epoch_timer_stop = rdtscp();
    if (chkClkSpan(epoch_timer_start, epoch_timer_stop, EPOCH_TIME * CLOCKS_PER_US * 1000) && chkEpochLoaded()) {
    atomicAddGE();
    epoch_timer_start = epoch_timer_stop;
    }
}

std::mt19937 mt{std::random_device{}()};
void FisherYates(std::vector<int>& v){
    int n = v.size();
    for(int i = n-1; i >= 0; i --){
        std::uniform_int_distribution<int> dist(0, i);
        int j = dist(mt);
        std::swap(v[i], v[j]);
    }
}

void ecall_initDB() {

#if BENCHMARK == 0
    TPCCWorkload<Tuple,void>::makeDB(nullptr);
#elif BENCHMARK == 1
    YcsbWorkload::makeDB<Tuple,void>(nullptr);
#endif

    for (int i = 0; i < THREAD_NUM; i++) {
        ThLocalEpoch[i] = 0;
        CTIDW[i] = ~(uint64_t)0;
    }

    for (int i = 0; i < LOGGER_NUM; i++) {
        ThLocalDurableEpoch[i] = 0;
    }
    DurableEpoch = 0;
}

// void makeProcedure(std::vector<Procedure> &pro, Xoroshiro128Plus &rnd) {
//     pro.clear();
//     for (int i = 0; i < MAX_OPE; i++) {
//         uint64_t tmpkey, tmpope;
//         tmpkey = rnd.next() % TUPLE_NUM;
//         if ((rnd.next() % 100) < RRAITO) {
//             pro.emplace_back(Ope::READ, tmpkey);
//         } else {
//             pro.emplace_back(Ope::WRITE, tmpkey);
//         }
//     }
// }