#pragma once

#include <instruction_set.h>
#include <array>
#include <vector>
#include <object.h>
#include <ram.h>

class Processor {
public:
    enum ExecutionStatus {
        kExecStatusOk,
        kExecStatusAddressOutOfRange,
        kExecStatusIPOutOfRange,
        kExecStatusRegisterOutOfRange,
        kExecStatusDataStackOverflow,
        kExecStatusCallStackOverflow,
        kExecStatusEmptyDataStack,
        kExecStatusEmptyCallStack,
        kExecStatusInvalidOpcode,
        kExecStatusDivZero
    };
    static constexpr int kDataStackMaxSize = 4096;
    static constexpr int kCallStackMaxSize = 4096;

    const Object::ProcVersion& GetVersion() const;
    bool Execute(const std::vector<int8_t>& bytecode, RAM* ram);
    void Dump() const;
    void PrintStackTrace(const Object& obj) const;

private:
    inline bool FillArgs(const std::vector<int8_t>& bytecode, int64_t** args, int64_t* arg_stubs, RAM* ram, int argcnt, uint64_t* ip);

    std::array<int64_t, (MAX_REGISTER) + 1> registers_;
    std::vector<int64_t> data_stack_;
    std::vector<int64_t> call_stack_;
    uint64_t instruction_pointer_ = 0;
    ExecutionStatus status_ = kExecStatusOk;
    const Object::ProcVersion version_{PROC_VERSION_MAJOR, PROC_VERSION_MINOR, PROC_VERSION_PATCH};
};
