#pragma once

#include <cstdint>

#include "cache_line_size.h"
#include "consts.h"

// structにしているのはprivateにしたいからってこと？
struct TIDword {
    union {
        uint64_t obj_;
        struct {    //　これで64bitに収まっているの？3word sizeとかになってない？
            bool lock: 1;
            bool latest: 1;
            bool absent: 1;
            uint64_t TID: 29;
            uint64_t epoch: 32;
        };
    };

    TIDword() : epoch(0), TID(0), absent(false), latest(true), lock(false) {};
    
    bool operator == (const TIDword &right) const { return obj_ == right.obj_; }
    bool operator != (const TIDword &right) const { return !operator == (right); }
    bool operator < (const TIDword &right) const { return this->obj_ < right.obj_; }

};

class Tuple {
    public:
        alignas(CACHE_LINE_SIZE) TIDword tidword_;
        char val_[VAL_SIZE];   //引数で宣言しているけどよくわからんから一旦定数で
};