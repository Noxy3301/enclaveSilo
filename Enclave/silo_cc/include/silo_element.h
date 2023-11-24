#pragma once

#include "../../utils/db_key.h"
#include "../../utils/db_value.h"

#include "../../../Include/structures.h"

class OpElement {
public:
    uint64_t key_;
    Value *value_;
    OpType op_;

    OpElement(const uint64_t &key, Value *value, OpType op)
        : key_(key), value_(value), op_(op) {}
};

class ReadElement : public OpElement {
public:
    using OpElement::OpElement;

    ReadElement(const uint64_t &key, Value *value, 
                const TIDword &tidword, OpType op = OpType::READ)
        : OpElement(key, value, op), tidword_(tidword) {}

    TIDword get_tidword() const {
        return tidword_;
    }

    bool operator<(const ReadElement &right) const {
        return key_ < right.key_;
    }

private:
    TIDword tidword_;
};

class WriteElement : public OpElement {
public:
    using OpElement::OpElement;

    WriteElement(const uint64_t &key, Value *value, 
                 uint64_t new_value_body, OpType op)
        : OpElement(key, value, op), new_value_body_(new_value_body) {}

    uint64_t get_new_value_body() const {
        return new_value_body_;
    }

    bool operator<(const WriteElement &right) const {
        return key_ < right.key_;
    }

private:
    uint64_t new_value_body_;
};
