#pragma once

#include "../../utils/db_key.h"
#include "../../utils/db_value.h"

#include "../../../Include/structures.h"

class Procedure {
    public:
        OpType ope_;
        uint64_t key_;
        uint64_t value_;

        Procedure(OpType ope, uint64_t key, uint64_t value)
            : ope_(ope), key_(key), value_(value) {}

        // bool operator<(const Procedure &right) const {
        //     if (this->key_ == right.key_ && this->ope_ == Ope::WRITE && right.ope_ == Ope::READ) {
        //         return true;
        //     } else if (this->key_ == right.key_ && this->ope_ == Ope::WRITE && right.ope_ == Ope::WRITE) {
        //         return true;
        //     }
        //     // キーが同値なら先に write ope を実行したい．read -> write よりも write -> read.
        //     // キーが同値で自分が read でここまで来たら，下記の式によって絶対に falseとなり，自分 (read) が昇順で後ろ回しになるので ok
      
        //     return this->key_ < right.key_;
        // }
};