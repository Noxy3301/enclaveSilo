/*
 * Copyright (C) 2011-2021 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _ENCLAVE_H_
#define _ENCLAVE_H_

#include <assert.h>
#include <stdlib.h>

#include "../Include/result.h"
#include "../Include/consts.h"
#include "include/tuple.h"
#include "OCH.h"

#include <vector>

#if defined(__cplusplus)
extern "C" {
#endif

int printf(const char* fmt, ...);

#if defined(__cplusplus)
}
#endif


// NOTE: LinearIndexは消すかも
// template <class T>
// class LinearIndex {
// public:
//     std::vector<Tuple*> table_;
//     int table_size_;

//     LinearIndex() {}

//     void insert_value(T value) {
//         table_.emplace_back(value);
//     }

//     T get(std::string key) {
//         for (int i = 0; i < table_.size(); i++) {
//             // print_String2Hex(table_[i]->key_, false);
//             // cout << " ";
//             // print_String2Hex(key);
//             // cout << "|" << print_hexString(table_[i]->key_) << "|" << print_hexString(key)  << "|" << endl;
//             if (table_[i]->key_ == key) {   // std::cout << "aru" << std::endl;
//                 return table_[i];
//             }
//         } // std::cout << "nai" << std::endl;
//         return nullptr;
//     }

//     // void print_String2Hex(std::string str, bool isFlush = true) {
//     //     // debug用、keyを8x8に戻す
//     //     for (int i = 0; i < 8; i++) {
//     //         std::cout << int(uint8_t(str[i])) << ",";
//     //     }
//     //     if (isFlush) std::cout << std::endl;
//     // }
// };

#endif /* !_ENCLAVE_H_ */
