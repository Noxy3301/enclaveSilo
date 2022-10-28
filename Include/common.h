#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "tuple.h"
#include "cache_line_size.h"
#include "int64byte.h"

#ifdef GLOBAL_VALUE_DEFINE
#define GLOBAL
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte GlobalEpoch(1);
#else
#define GLOBAL extern
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte GlobalEpoch;
#endif

alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *ThLocalEpoch;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *CTIDW;

// alignas(CACHE_LINE_SIZE) GLOBAL Tuple *Table;

GLOBAL std::vector<Tuple> Table(TUPLE_NUM);

// common_log
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte *ThLocalDurableEpoch;
alignas(CACHE_LINE_SIZE) GLOBAL uint64_t_64byte DurableEpoch;