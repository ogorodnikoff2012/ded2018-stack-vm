#pragma once

#include <instruction_set.h>
#include <object.h>
#include <sys/mman.h>

class ProtectedMemoryArena {
public:
    explicit ProtectedMemoryArena(int64_t size, int prot_flags = PROT_READ | PROT_WRITE);
    ~ProtectedMemoryArena();
    void* Begin() const;
    void* End() const;
    int64_t Size() const;

private:
    void* pre_canary_;
    void* data_;
    void* post_canary_;
    int64_t size_;
};

class ExecutionContext {
public:
    void SwitchTo(ExecutionContext& context);
    static constexpr int64_t kSize = sizeof(int64_t) * 6; /* Callee-saved registers: RBP, RBX, R12-R15 */
private:
    void* rsp;
    friend void PrepareUserContext(ExecutionContext&, char*, void*, void*);
};

class JITCompiler {
public:
    JITCompiler();
    ~JITCompiler();

    const Object::ProcVersion& GetProcessorVersion() const;
    void Compile(const Object& obj);
    void Execute();

private:
    std::optional<ProtectedMemoryArena> code_;
    ProtectedMemoryArena data_;
    ProtectedMemoryArena data_stack_, call_stack_;
    std::vector<void*> code_addr_table_;

    const Object::ProcVersion version_{PROC_VERSION_MAJOR, PROC_VERSION_MINOR, PROC_VERSION_PATCH};
};
