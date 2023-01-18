#pragma once

#include <cstdint>
#include <vector>

#include "tuple.h"

#include "../../Include/consts.h"

extern std::vector<Tuple> Table;
extern std::vector<uint64_t> ThLocalEpoch;
extern std::vector<uint64_t> CTIDW;
extern std::vector<uint64_t> ThLocalDurableEpoch;
extern uint64_t DurableEpoch;
extern uint64_t GlobalEpoch;
extern std::vector<returnResult> results;

// std::vector<Result> results(THREAD_NUM); //TODO: mainの方でもsiloresultを定義しているので重複回避目的で名前変更しているけどここら辺どうやって扱うか考えておく

extern bool start;
extern bool quit;