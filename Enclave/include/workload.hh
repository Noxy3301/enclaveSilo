#pragma once

#include <map>

#include "tuple.h"

enum class Storage : std::uint32_t;
inline uint32_t get_storage(Storage s) {
  return static_cast<std::uint32_t>(s);
}

enum class TxType : std::uint32_t;
inline uint32_t get_tx_type(TxType t) {
  return static_cast<std::uint32_t>(t);
}

// GLOBAL std::map<uint32_t,std::string> TxTypes;
// inline std::string get_tx_name(uint32_t t) {
//   return TxTypes.at(t);
// }
// inline void set_tx_name(TxType t, std::string name) {
//   TxTypes.emplace(get_tx_type(t), name);
// }

template<size_t N>
struct SimpleKey {
  char data[N]; // not null-terminated.

  char *ptr() { return &data[0]; }

  const char *ptr() const { return &data[0]; }

  [[nodiscard]] std::string view() const {
    return std::string(&data[0], N);
  }

  int compare(const SimpleKey& rhs) const {
    return ::memcmp(data, rhs.data, N);
  }
  bool operator<(const SimpleKey& rhs) const {
    return compare(rhs) < 0;
  }
  bool operator==(const SimpleKey& rhs) const {
    return compare(rhs) == 0;
  }
};

// #define MAX_TABLES 10
// alignas(CACHE_LINE_SIZE) GLOBAL MasstreeWrapper<Tuple> Masstrees[MAX_TABLES];

#if INDEX_PATTERN == 0
extern std::vector<OptCuckoo<Tuple>> Table;
#elif INDEX_PATTERN == 1
extern LinearIndex<Tuple*> Table;
#else
// Masstree Table
#endif