#pragma once

#include <memory>

#include "workload.hh"
#include "tuple.h"

enum class OpType : std::uint8_t {
  NONE,
  READ,
  SCAN,
  INSERT,
  DELETE,
  UPDATE,
  RMW,
};

template<typename T>
class OpElement {
public:
  Storage storage_;
  std::string key_;
  T *rcdptr_;
  OpType op_;

  OpElement() : key_(0), rcdptr_(nullptr) {}

  OpElement(std::string key) : key_(key) {}

  OpElement(std::string key, T *rcdptr) : key_(key), rcdptr_(rcdptr) {}

  OpElement(Storage s, std::string key, T *rcdptr)
    : storage_(s), key_(key), rcdptr_(rcdptr) {}

  OpElement(Storage s, std::string key, T *rcdptr, OpType op)
    : storage_(s), key_(key), rcdptr_(rcdptr), op_(op) {}
};


// silo_op_element

template<typename T>
class ReadElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;
  // TupleBody body_;

  ReadElement(Storage s, std::string key, T *rcdptr, int val, Tidword tidword)
          : OpElement<T>::OpElement(s, key, rcdptr) {
    tidword_.obj_ = tidword.obj_;
    this->val_ = val;
    // memcpy(this->val_, val, VAL_SIZE);
  }

  // ReadElement(Storage s, std::string key, T* rcdptr, TupleBody&& body, Tidword tidword)
  //         : OpElement<T>::OpElement(s, key, rcdptr), body_(std::move(body)) {
  //   tidword_.obj_ = tidword.obj_;
  // }

  bool operator<(const ReadElement &right) const {
    if (this->storage_ != right.storage_) return this->storage_ < right.storage_;
    return this->key_ < right.key_;
  }

  Tidword get_tidword() {
    return tidword_;
  }

private:
  Tidword tidword_;
  int val_;
};

template<typename T>
class WriteElement : public OpElement<T> {
public:
  using OpElement<T>::OpElement;
  // TupleBody body_;

  // WriteElement(Storage s, std::string key, T *rcdptr, std::string val, OpType op)
  //         : OpElement<T>::OpElement(s, key, rcdptr, op) {
    // static_assert(std::string("").size() == 0, "Expected behavior was broken.");
    // if (val.size() != 0) {
    //   val_ptr_ = std::make_unique<char[]>(val.size());
    //   memcpy(val_ptr_.get(), val.data(), val.size());
    //   val_length_ = val.size();
    // } else {
    //   val_length_ = 0;
    // }
    // else : fast approach for benchmark
  // }

  WriteElement(Storage s, std::string key, T* rcdptr, int val, OpType op)
          : OpElement<T>::OpElement(s, key, rcdptr, op) {
            this->val_ = val;
  }

  // WriteElement(Storage s, std::string key, T* rcdptr, TupleBody&& body, OpType op)
  //         : OpElement<T>::OpElement(s, key, rcdptr, op), body_(std::move(body)) {
  // }

  bool operator<(const WriteElement &right) const {
    if (this->storage_ != right.storage_) return this->storage_ < right.storage_;
    return this->key_ < right.key_;
  }

  int get_val() {
    return val_;
  }

  // char *get_val_ptr() {
  //   return val_ptr_.get();
  // }

  // std::size_t get_val_length() {
  //   return val_length_;
  // }

private:
  int val_;
  // std::unique_ptr<char[]> val_ptr_; // NOLINT
  // std::size_t val_length_{};
};
