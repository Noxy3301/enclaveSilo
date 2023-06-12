#pragma once

#include <cstdint>
#include <sstream>

#include "atomic_tool.h"
#include "procedure.h"
#include "random.h"
#include "tsc.h"

inline static bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold) {
    uint64_t diff = 0;
    diff = stop - start;
    if (diff > threshold) {
        return true;
    } else {
        return false;
    }
}

inline static void waitTime_ns(const uint64_t time) {
    uint64_t start = rdtscp();
    uint64_t end = 0;
    for (;;) {
        end = rdtscp();
        if (end - start > time * CLOCKS_PER_US) break;   // ns換算にしたいけど除算はコストが高そうなので1000倍して調整
    }
}

// common/util.ccのsleepMsをTPCCで使うけど、this_thread::sleep_forは使えないから代用
inline static void sleepMs(size_t ms) {
    uint64_t start = rdtscp();
    uint64_t end = 0;
    for (;;) {
        end = rdtscp();
        if (end - start > ms * CLOCKS_PER_US * 1000) break;   // CHECK: これあっている？
    }
}

template<typename Int>
Int byteswap(Int in) {
  switch (sizeof(Int)) {
    case 1:
      return in;
    case 2:
      return __builtin_bswap16(in);
    case 4:
      return __builtin_bswap32(in);
    case 8:
      return __builtin_bswap64(in);
    default:
      assert(false);
  }
}

template<typename Int>
void assign_as_bigendian(Int value, char *out) {
  Int tmp = byteswap(value);
  ::memcpy(out, &tmp, sizeof(tmp));
}

template<typename Int>
void parse_bigendian(const char *in, Int &out) {
  Int tmp;
  ::memcpy(&tmp, in, sizeof(tmp));
  out = byteswap(tmp);
}

template<typename T>
std::string_view struct_str_view(const T &t) {
  return std::string_view(reinterpret_cast<const char *>(&t), sizeof(t));
}

/**
 * Better strncpy().
 * out buffer will be null-terminated.
 * returned value is written size excluding the last null character.
 */
inline std::size_t copy_cstr(char *out, const char *in, std::size_t out_buf_size) {
  if (out_buf_size == 0) return 0;
  std::size_t i = 0;
  while (i < out_buf_size - 1) {
    if (in[i] == '\0') break;
    out[i] = in[i];
    i++;
  }
  out[i] = '\0';
  return i;
}


/**
 * for debug.
 */
inline std::string str_view_hex(std::string_view sv) {
  std::stringstream ss;
  char buf[3];
  for (auto &&i : sv) {
    ::snprintf(buf, sizeof(buf), "%02x", i);
    ss << buf;
  }
  return ss.str();
}


template<typename T>
inline std::string_view str_view(const T &t) {
  return std::string_view(reinterpret_cast<const char *>(&t), sizeof(t));
}

extern bool chkEpochLoaded();
extern void siloLeaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop);
extern void makeProcedure(std::vector<Procedure> &pro, Xoroshiro128Plus &rnd);