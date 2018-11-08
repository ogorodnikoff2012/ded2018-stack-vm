#pragma once

#include <oosf/types.h>
#include <vector>
#include <unordered_map>

struct Object : public Serializable {
    std::vector<int8_t> bytecode;
    std::unordered_map<std::string, int64_t> defined_labels;
    std::unordered_map<std::string, int64_t> required_labels;
    int64_t bss_size = 0;

    virtual ReadStatus TryRead(InputDataStream*) override;
    virtual void WriteValue(OutputDataStream*) const override;
};
