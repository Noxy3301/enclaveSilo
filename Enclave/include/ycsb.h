#pragma once

#include <thread>
#include <vector>

#include "../../Include/result.h"
#include "heap_object.hh"
#include "procedure.h"
#include "random.h"
#include "tuple_body.hh"
#include "util.h"
#include "workload.hh"
#include "zipf.h"

#include "atomic_wrapper.h"
#include "transaction.h"

#include <iostream> // debug

enum class Storage : std::uint32_t {
    YCSB = 0,
    Size,
};

struct YCSB {
    uint64_t id_;
    char val_[VAL_SIZE];

    static void CreateKey(uint64_t id, char *out) {
        assign_as_bigendian(id, &out[0]);
    }

    void createKey(char *out) const { return CreateKey(id_, out); }

    [[nodiscard]] std::string_view view() const {
        return struct_str_view(*this);
    }
};

inline static void makeProcedure(std::vector<Procedure> &pro,
                                 Xoroshiro128Plus &rnd, FastZipf &zipf) {
    pro.clear();
    for (int i = 0; i < MAX_OPE; i++) {
        uint64_t tmpkey = zipf() % TUPLE_NUM; // decide access destination key.
        // decide operation type
        if ((rnd.next() % 100) < RRAITO) {
            pro.emplace_back(Ope::READ, tmpkey);
        } else {
            pro.emplace_back(Ope::WRITE, tmpkey);
        }
    }
}

class YcsbWorkload {
  public:
    Xoroshiro128Plus rnd_;
    FastZipf zipf_;

    YcsbWorkload() {
        rnd_.init();
        FastZipf zipf(&rnd_, ZIPF_SKEW, TUPLE_NUM);
        zipf_ = zipf;
    }

    template <typename TxExecutor, typename TransactionStatus>
    void run(TxExecutor &tx) {
        makeProcedure(tx.pro_set_, rnd_, zipf_);
    RETRY:
        if (tx.isLeader()) {
            tx.leaderWork();
        }
        tx.durableEpochWork(tx.epoch_timer_start, tx.epoch_timer_stop, quit);

        if (loadAcquire(tx.quit_))
            return;

        tx.begin();
        SimpleKey<8> key[tx.pro_set_.size()];
        HeapObject obj[tx.pro_set_.size()];
        uint64_t i = 0;
        for (auto &pro : tx.pro_set_) {
            YCSB::CreateKey(pro.key_, key[i].ptr());
            uint64_t k;
            parse_bigendian(key[i].view().data(), k);
            if (pro.ope_ == Ope::READ) {
                TupleBody *body;
                tx.read(Storage::YCSB, key[i].view(), &body);
                if (tx.status_ != TransactionStatus::aborted) {
                    YCSB &t = body->get_value().cast_to<YCSB>();
                }
            } else if (pro.ope_ == Ope::WRITE) {
                obj[i].template allocate<YCSB>();
                YCSB &t = obj[i].ref();
                tx.write(Storage::YCSB, key[i].view(), TupleBody(key[i].view(), std::move(obj[i])));
            } else if (pro.ope_ == Ope::READ_MODIFY_WRITE) {
                TupleBody *body;
                tx.read(Storage::YCSB, key[i].view(), &body);
                if (tx.status_ != TransactionStatus::aborted) {
                    YCSB &old_tuple = body->get_value().cast_to<YCSB>();
                    obj[i].template allocate<YCSB>();
                    YCSB &new_tuple = obj[i].ref();
                    memcpy(new_tuple.val_, old_tuple.val_, VAL_SIZE);
                    tx.write(Storage::YCSB, key[i].view(), TupleBody(key[i].view(), std::move(obj[i])));
                }
            } else {
                printf("okasiizo!");
                // ERR;
            }

            if (tx.status_ == TransactionStatus::aborted) {
                tx.abort();
                ++tx.result_->local_abort_counts_;
                goto RETRY;
            }

            i++;
        }

        if (!tx.commit()) {
            tx.abort();
            ++tx.result_->local_abort_counts_;
            goto RETRY;
        }
        storeRelease(tx.result_->local_commit_counts_,
                     loadAcquire(tx.result_->local_commit_counts_) + 1);

        return;
    }

    static uint32_t getTableNum() { return (uint32_t)Storage::Size; }

    template <typename Tuple, typename Param> static void makeDB(Param *p) {
        for (auto i = 0; i <= TUPLE_NUM; ++i) {
            SimpleKey<8> key;
            YCSB::CreateKey(i, key.ptr());
            HeapObject obj;
            obj.allocate<YCSB>();
            YCSB& ycsb_tuple = obj.ref();
            ycsb_tuple.id_ = i;
            Tuple* tmp = new Tuple();
            tmp->init(0, TupleBody(key.view(), std::move(obj)), p);
            Table[get_storage(Storage::YCSB)].put(key.view(), tmp, 1);
        }
    }

    // static void displayWorkloadParameter() {
    //   cout << "#FLAGS_ycsb_max_ope:\t" << FLAGS_ycsb_max_ope << endl;
    //   cout << "#FLAGS_ycsb_rmw:\t" << FLAGS_ycsb_rmw << endl;
    //   cout << "#FLAGS_ycsb_rratio:\t" << FLAGS_ycsb_rratio << endl;
    //   cout << "#FLAGS_ycsb_tuple_num:\t" << FLAGS_ycsb_tuple_num << endl;
    //   cout << "#FLAGS_ycsb_zipf_skew:\t" << FLAGS_ycsb_zipf_skew << endl;
    // }
};
