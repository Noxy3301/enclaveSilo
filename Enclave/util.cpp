#include "include/util.h"

#include "Enclave_t.h"
#include "sgx_thread.h"

bool chkEpochLoaded() {
    uint64_t nowepo = atomicLoadGE();
    // leader_workを実行しているのはthid:0だからforは1から回している？
    for (unsigned int i = 1; i < THREAD_NUM; i++) {
        if (__atomic_load_n(&(ThLocalEpoch[i]), __ATOMIC_ACQUIRE) != nowepo) return false;
    }
    return true;
}

void leaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop) {
    epoch_timer_stop = rdtscp();
    if (chkClkSpan(epoch_timer_start, epoch_timer_stop, EPOCH_TIME * CLOCKS_PER_US * 1000) && chkEpochLoaded()) {
        atomicAddGE();
        epoch_timer_start = epoch_timer_stop;
    }
}

void makeProcedure(std::vector<Procedure> &pro, Xoroshiro128Plus &rnd) {
    pro.clear();
    for (int i = 0; i < MAX_OPE; i++) {
        uint64_t tmpkey, tmpope;
        tmpkey = rnd.next() % TUPLE_NUM;
        if ((rnd.next() % 100) < RRAITO) {
            pro.emplace_back(Ope::READ, tmpkey);
        } else {
            pro.emplace_back(Ope::WRITE, tmpkey);
        }
    }
}

bool unchi() {
    int unchiburi = 0;
}