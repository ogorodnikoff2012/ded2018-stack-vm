#include <jit_compiler.h>
#include <ram.h>
#include <processor.h>
#include <argument_descriptors.h>

#include <sys/mman.h>
#include <cstring>
#include <iostream>

static constexpr int64_t kPageSize = 1 << 12;

static int64_t RoundUp(int64_t value, int64_t divisor) {
    return ((value + divisor - 1) / divisor) * divisor;
}

ProtectedMemoryArena::ProtectedMemoryArena(int64_t size, int prot_flags) {
    size_ = size = RoundUp(size, kPageSize);
    char* buffer = static_cast<char*>(mmap(NULL, size + 2 * kPageSize, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
    pre_canary_ = buffer;
    data_ = buffer + kPageSize;
    post_canary_ = buffer + kPageSize + size;
    mprotect(data_, size, prot_flags);
}

ProtectedMemoryArena::~ProtectedMemoryArena() {
    munmap(pre_canary_, size_ + 2 * kPageSize);
}

void* ProtectedMemoryArena::Begin() const {
    return data_;
}

void* ProtectedMemoryArena::End() const {
    return post_canary_;
}

int64_t ProtectedMemoryArena::Size() const {
    return size_;
}

////////////////////////////////////////////////////////////////////////////////

extern "C" void DoSwitch(void** old_rsp, void** new_rsp);

void ExecutionContext::SwitchTo(ExecutionContext& context) {
    DoSwitch(&rsp, &context.rsp);
}

////////////////////////////////////////////////////////////////////////////////

JITCompiler::JITCompiler()
    : data_(RAM::kChunkSize * RAM::kMaxChunksCnt * sizeof(int64_t)),
    data_stack_(Processor::kDataStackMaxSize * sizeof(int64_t)),
    call_stack_(Processor::kCallStackMaxSize * sizeof(int64_t)) {
}

JITCompiler::~JITCompiler() {
}

const Object::ProcVersion& JITCompiler::GetProcessorVersion() const {
    return version_;
}

static thread_local ExecutionContext supervisor_context, user_context;

void PrepareUserContext(ExecutionContext& user_context, char* user_stack, void* call_stack, void* entry_point) {
    user_stack -= sizeof(void*);
    *((void**)user_stack) = entry_point;
    user_stack -= ExecutionContext::kSize;
    std::memset(user_stack, 0, ExecutionContext::kSize);
    std::memcpy(user_stack, &call_stack, sizeof(void*));
    user_context.rsp = user_stack;
}

static void BadJumpAddressHandler() {
    std::printf("Jump to invalid address\n");
    std::exit(1);
}

template <class T>
static inline bool TryGet(const std::vector<int8_t>& bytecode, int64_t* ip, T* dest) {
    if ((*ip + sizeof(T)) > bytecode.size()) {
        return false;
    }
    *dest = *reinterpret_cast<const T*>(&bytecode[*ip]);
    *ip += sizeof(T);
    return true;
}


bool FillArgs(const std::vector<int8_t>& bytecode, int* arg_types, int64_t* arg_values, int argcnt, int64_t* ip) {
#define TRY_GET(x) if (!TryGet(bytecode, ip, ( x ))) { return false; }
    if (argcnt == 0) {
        return true;
    }

    int8_t arg_descriptor = 0;
    int8_t reg_buffer = 0;
    TRY_GET(&arg_descriptor);

    for (int i = 0; i < argcnt; ++i) {
        arg_types[i] = GetArgType(arg_descriptor, i);

        switch (arg_types[i]) {
            case ARG_VALUE:
                TRY_GET(arg_values + i);
                break;
            case ARG_POINTER:
                TRY_GET(arg_values + i);
                break;
            case ARG_REGISTER:
            case ARG_REGISTER_POINTER:
                TRY_GET(&reg_buffer);
                if (reg_buffer < 0 || reg_buffer >= MAX_REGISTER) {
                    return false;
                }
                arg_values[i] = reg_buffer;
                break;
        }
    }

    return true;
}

#define DEF_CMD(name, code, argcnt, from_stack_cnt, to_stack_cnt, handler, asm_codegen)         \
    case code: {                                                                                \
        int arg_types[argcnt + 1] = {};                                                         \
        int64_t arg_values[argcnt + 1] = {};                                                    \
        if (!FillArgs(obj.bytecode, arg_types, arg_values, argcnt, &instruction_pointer)) {     \
            throw std::runtime_error("Instruction is corrupted! Cannot read arguments.");       \
        }                                                                                       \
        asm_codegen ;                                                                           \
        ASM_NOP();                                                                              \
    } break;

template <class T>
void AppendInstruction(std::vector<int8_t>* native_code, T byte) {
    native_code->push_back(byte);
}

template <class T>
void AppendInstruction(std::vector<int8_t>* native_code, T* ptr) {
    int8_t* bytes = reinterpret_cast<int8_t*>(&ptr);
    for (int i = 0; i < 8; ++i) {
        native_code->push_back(bytes[i]);
    }
}

template <class Head1, class Head2, class... Tail>
void AppendInstruction(std::vector<int8_t>* native_code, Head1&& head1, Head2&& head2, Tail&&... tail) {
    AppendInstruction(native_code, std::forward<Head1>(head1));
    AppendInstruction(native_code, std::forward<Head2>(head2), std::forward<Tail>(tail)...);
}

template <class T>
struct Directly {
    T value;
};

template <class T>
auto MakeDirectly(T value) {
    return Directly<T>{value};
}

template <class T>
void AppendInstruction(std::vector<int8_t>* native_code, Directly<T> value) {
    int8_t* ptr = reinterpret_cast<int8_t*>(&value.value);
    for (size_t i = 0; i < sizeof(T); ++i) {
        native_code->push_back(ptr[i]);
    }
}

void* ToDataPointer(const ProtectedMemoryArena& data, int64_t addr) {
    if (addr < 0 || addr >= (data.Size() >> 3)) {
        throw std::runtime_error("Bad data pointer!");
    }
    return static_cast<int64_t*>(data.Begin()) + addr;
}

static thread_local void* rsp_buffer;

#define STACK_ALIGN_PRE         \
    asm ("mov %%rsp, %0\n"      \
         "and $-0x10, %%rsp\n"  \
         : "=m"(rsp_buffer));

#define STACK_ALIGN_POST        \
    asm ("mov %0, %%rsp"        \
         :                      \
         : "m"(rsp_buffer));

void OverflowCall() {
    STACK_ALIGN_PRE
    std::printf("Pointer out of of bounds! Stopping...");
    user_context.SwitchTo(supervisor_context);
    // UNREACHABLE
    STACK_ALIGN_POST
}

int64_t ReadIntCall() {
    STACK_ALIGN_PRE
    int64_t result;
    std::scanf("%ld", &result);
    STACK_ALIGN_POST
    return result;
}

void WriteIntCall(int64_t x) {
    STACK_ALIGN_PRE
    std::printf("%ld\n", x);
    STACK_ALIGN_POST
}

double ReadDoubleCall() {
    STACK_ALIGN_PRE
    double result;
    std::scanf("%lf", &result);
    STACK_ALIGN_POST
    return result;
}

void WriteDoubleCall(double d) {
    STACK_ALIGN_PRE
    std::printf("%.6lf\n", d);
    STACK_ALIGN_POST
}

void HaltCall() {
    STACK_ALIGN_PRE
    user_context.SwitchTo(supervisor_context);
    STACK_ALIGN_POST
}

extern "C" void FuncCall();

void PrintDumpCall() {
    STACK_ALIGN_PRE
    std::printf("Dump is currently unavailable\n");
    STACK_ALIGN_POST
}

void Log() {
    std::cout << std::endl;
}

template <class Head, class... Rest>
void Log(Head head, Rest... rest) {
    std::cout << std::hex << head << ' ';
    Log(rest...);
}

template <class T>
std::ostream& operator<<(std::ostream& out, const Directly<T>& d) {
    return out << "Directly<" << d.value << ">";
}

#define APPEND_INSTRUCTION(...) { /*Log("AppendInstruction", __VA_ARGS__);*/ AppendInstruction(&native_code, __VA_ARGS__); }

#define ASM_PUSH_RAX()              APPEND_INSTRUCTION(0x50)
#define ASM_MOV_RBX_RAX()           APPEND_INSTRUCTION(0x48, 0x89, 0xd8)
#define ASM_POP_RAX()               APPEND_INSTRUCTION(0x58)
#define ASM_POP_RBX()               APPEND_INSTRUCTION(0x5b)
#define ASM_ADD_RBX_RAX()           APPEND_INSTRUCTION(0x48, 0x01, 0xd8)
#define ASM_SUB_RAX_RBX()           APPEND_INSTRUCTION(0x48, 0x29, 0xc3)
#define ASM_XCHG_RAX_RBX()          APPEND_INSTRUCTION(0x48, 0x93)
#define ASM_IMUL_RBX_RAX()          APPEND_INSTRUCTION(0x48, 0x0f, 0xaf, 0xc3)
#define ASM_MOV_RAX_RBX()           APPEND_INSTRUCTION(0x48, 0x89, 0xc3)
#define ASM_ZERO_RDX()              APPEND_INSTRUCTION(0x48, 0x31, 0xd2)
#define ASM_IDIV_RBX()              APPEND_INSTRUCTION(0x48, 0xf7, 0xfb)
#define ASM_MOV_RDX_RAX()           APPEND_INSTRUCTION(0x48, 0x89, 0xd0)
#define ASM_AND_RBX_RAX()           APPEND_INSTRUCTION(0x48, 0x21, 0xd8)
#define ASM_OR_RBX_RAX()            APPEND_INSTRUCTION(0x48, 0x09, 0xd8)
#define ASM_XOR_RBX_RAX()           APPEND_INSTRUCTION(0x48, 0x31, 0xd8)
#define ASM_MOV_BL_CL()             APPEND_INSTRUCTION(0x88, 0xd9)
#define ASM_SHL_CL_RAX()            APPEND_INSTRUCTION(0x48, 0xd3, 0xe0)
#define ASM_CMP_RAX_RBX()           APPEND_INSTRUCTION(0x48, 0x39, 0xc3)
#define ASM_SETL_AL()               APPEND_INSTRUCTION(0x0f, 0x9c, 0xc0)
#define ASM_MOVZX_AL_RAX()          APPEND_INSTRUCTION(0x48, 0x0f, 0xb6, 0xc0)
#define ASM_SHR_CL_RAX()            APPEND_INSTRUCTION(0x48, 0xd3, 0xe8)
#define ASM_SETG_AL()               APPEND_INSTRUCTION(0x0f, 0x9f, 0xc0)
#define ASM_SETLE_AL()              APPEND_INSTRUCTION(0x0f, 0x9e, 0xc0)
#define ASM_SETGE_AL()              APPEND_INSTRUCTION(0x0f, 0x9d, 0xc0)
#define ASM_SETE_AL()               APPEND_INSTRUCTION(0x0f, 0x94, 0xc0)
#define ASM_SETNE_AL()              APPEND_INSTRUCTION(0x0f, 0x95, 0xc0)
#define ASM_MOVSD_BY_RSP_XMM0()     APPEND_INSTRUCTION(0xf2, 0x0f, 0x10, 0x04, 0x24)
#define ASM_MOV_RAX_BY_RSP()        APPEND_INSTRUCTION(0x48, 0x89, 0x04, 0x24)
#define ASM_ADDSD_BY_RSP_XMM0()     APPEND_INSTRUCTION(0xf2, 0x0f, 0x58, 0x04, 0x24)
#define ASM_MOV_XMM0_RAX()          APPEND_INSTRUCTION(0x66, 0x48, 0x0f, 0x7e, 0xc0)
#define ASM_ADD_IMM8_RSP(x)         APPEND_INSTRUCTION(0x48, 0x83, 0xc4, x)
#define ASM_SUBSD_BY_RSP_XMM0()     APPEND_INSTRUCTION(0xf2, 0x0f, 0x5c, 0x04, 0x24)
#define ASM_MULSD_BY_RSP_XMM0()     APPEND_INSTRUCTION(0xf2, 0x0f, 0x59, 0x04, 0x24)
#define ASM_DIVSD_BY_RSP_XMM0()     APPEND_INSTRUCTION(0xf2, 0x0f, 0x5e, 0x04, 0x24)
#define ASM_CVTSI2SD_RAX_XMM0()     APPEND_INSTRUCTION(0xf2, 0x48, 0x0f, 0x2a, 0xc0)
#define ASM_MOV_RAX_XMM0()          APPEND_INSTRUCTION(0x66, 0x48, 0x0f, 0x6e, 0xc0)
#define ASM_CVTTSD2SI_XMM0_RAX()    APPEND_INSTRUCTION(0xf2, 0x48, 0x0f, 0x2c, 0xc0)
#define ASM_MOV_RAX_RDI()           APPEND_INSTRUCTION(0x48, 0x89, 0xc7)
#define ASM_SAVE_REGS()             APPEND_INSTRUCTION(0x50, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53)
#define ASM_RESTORE_REGS()          APPEND_INSTRUCTION(0x41, 0x5b, 0x41, 0x5a, 0x41, 0x59, 0x41, 0x58, 0x58)
#define ASM_ZERO_RAX()              APPEND_INSTRUCTION(0x48, 0x31, 0xc0)
#define ASM_JMP_RBX()               APPEND_INSTRUCTION(0xff, 0xe3)
#define ASM_CMP_IMM8_RAX(x)         APPEND_INSTRUCTION(0x48, 0x83, 0xf8, x)
#define ASM_JNE_REL8(x)             APPEND_INSTRUCTION(0x75, x)
#define ASM_JLE_REL8(x)             APPEND_INSTRUCTION(0x7e, x)
#define ASM_JGE_REL8(x)             APPEND_INSTRUCTION(0x7d, x)
#define ASM_JE_REL8(x)              APPEND_INSTRUCTION(0x74, x)
#define ASM_JL_REL8(x)              APPEND_INSTRUCTION(0x7c, x)
#define ASM_JG_REL8(x)              APPEND_INSTRUCTION(0x7f, x)
#define ASM_MOV_IMM64_RBX(x)        APPEND_INSTRUCTION(0x48, 0xbb, MakeDirectly(x))
#define ASM_MOV_BY_RBX_RBX()        APPEND_INSTRUCTION(0x48, 0x8b, 0x1b)
#define ASM_MOV_REG_RBX(reg_no)     APPEND_INSTRUCTION(0x4c, 0x89, ENCODE_REG(reg_no, RBX_NO))
#define ASM_MOV_IMM64_RCX(x)        APPEND_INSTRUCTION(0x48, 0xb9, MakeDirectly(x))
#define ASM_CMP_RBX_RCX()           APPEND_INSTRUCTION(0x48, 0x39, 0xd9)
#define ASM_JAE_IMM8(x)             APPEND_INSTRUCTION(0x73, x)
#define ASM_MOV_BY_RCX_PLUS_RBX_TIMES_8_RBX() \
                                    APPEND_INSTRUCTION(0x48, 0x8b, 0x1c, 0xd9)
#define ASM_LEA_BY_RCX_PLUS_RBX_TIMES_8_RBX() \
                                    APPEND_INSTRUCTION(0x48, 0x8d, 0x1c, 0xd9)
#define ASM_MOV_RAX_REG(reg_no)     APPEND_INSTRUCTION(0x49, 0x89, ENCODE_REG(RAX_NO, reg_no))
#define ASM_MOV_REG_RAX(reg_no)     APPEND_INSTRUCTION(0x4c, 0x89, ENCODE_REG(reg_no, RAX_NO))
#define ASM_MOV_RBX_BY_RAX()        APPEND_INSTRUCTION(0x48, 0x89, 0x18)
#define ASM_MOV_RAX_BY_PTR(ptr)     APPEND_INSTRUCTION(0x48, 0xa3, (ptr))
#define ASM_MOV_RAX_BY_RBX()        APPEND_INSTRUCTION(0x48, 0x89, 0x03)
#define ASM_CALL_VIA_RAX(ptr)       APPEND_INSTRUCTION(0x48, 0xb8, ptr, 0xff, 0xd0)
#define ASM_MOV_BY_RSP_RBX()        APPEND_INSTRUCTION(0x48, 0x8b, 0x1c, 0x24)
#define ASM_UCOMISD_XMM0_XMM0()     APPEND_INSTRUCTION(0x66, 0x0f, 0x2e, 0xc0)
#define ASM_SETNP_AL()              APPEND_INSTRUCTION(0x0f, 0x9b, 0xc0)
#define ASM_ZERO_XMM1()             APPEND_INSTRUCTION(0x66, 0x0f, 0xef, 0xc9)
#define ASM_COMISD_XMM1_XMM0()      APPEND_INSTRUCTION(0x66, 0x0f, 0x2f, 0xc1)
#define ASM_SETA_AL()               APPEND_INSTRUCTION(0x0f, 0x97, 0xc0)
#define ASM_SETB_BL()               APPEND_INSTRUCTION(0x0f, 0x92, 0xc3)
#define ASM_SUB_BL_AL()             APPEND_INSTRUCTION(0x28, 0xd8)
#define ASM_MOVSX_AL_RAX()          APPEND_INSTRUCTION(0x48, 0x0f, 0xbe, 0xc0)
#define ASM_SHL_RAX()               APPEND_INSTRUCTION(0x48, 0xd1, 0xe0)
#define ASM_MOV_IMM16_RBX(x)        APPEND_INSTRUCTION(0x48, 0xc7, 0xc3, MakeDirectly(static_cast<int32_t>(x)))
#define ASM_SHL_IMM8_RBX(x)         APPEND_INSTRUCTION(0x48, 0xc1, 0xe3, x)
#define ASM_MOV_BY_RBP_RBX()        APPEND_INSTRUCTION(0x48, 0x8b, 0x5d, 0x00)
#define ASM_ADD_IMM8_RBP(x)         APPEND_INSTRUCTION(0x48, 0x83, 0xc5, x)
#define ASM_MOV_RAX_RCX()           APPEND_INSTRUCTION(0x48, 0x89, 0xc1)
#define ASM_PUSH_RBX()              APPEND_INSTRUCTION(0x53)
#define ASM_RET()                   APPEND_INSTRUCTION(0xc3)
#define ASM_NOP()                   APPEND_INSTRUCTION(0x90)
#define ASM_NEG_RAX()               APPEND_INSTRUCTION(0x48, 0xf7, 0xd8)
#define ASM_ZERO_RBX()              APPEND_INSTRUCTION(0x48, 0x31, 0xdb)
#define ASM_INC_RBX()               APPEND_INSTRUCTION(0x48, 0xff, 0xc3)
#define ASM_XCHG_RAX_BY_RSP()       APPEND_INSTRUCTION(0x48, 0x87, 0x04, 0x24)
#define ASM_TEST_RAX_RAX()          APPEND_INSTRUCTION(0x48, 0x85, 0xc0)
#define ASM_SETZ_AL()               APPEND_INSTRUCTION(0x0f, 0x94, 0xc0)
#define ASM_SETNZ_AL()              APPEND_INSTRUCTION(0x0f, 0x95, 0xc0)
#define ASM_MOV_RAX_RDX()           APPEND_INSTRUCTION(0x48, 0x89, 0xc2)
#define ASM_SAR_IMM8_RDX(x)         APPEND_INSTRUCTION(0x48, 0xc1, 0xfa, x)

#define ARG_TYPE(x)                 arg_types[x]
#define ARG(x)                      arg_values[x]
#define ENCODE_REG(reg1, reg2)      (int)(0xC0 | ((reg1) << 3) | (reg2))
#define RBX_NO                      0x03
#define RAX_NO                      0x00
#define TO_DATA_PTR(addr)           ToDataPointer(data_, addr)

#define CONVERT_RBX_TO_DATA_PTR()   {       \
    ASM_MOV_IMM64_RCX((data_.Size() >> 3)); \
    ASM_CMP_RBX_RCX();                      \
    ASM_JAE_IMM8(12);                       \
    ASM_MOV_IMM64_RBX(OVERFLOW_CALL);       \
    ASM_JMP_RBX();                          \
    ASM_MOV_IMM64_RCX(data_.Begin());       \
    ASM_LEA_BY_RCX_PLUS_RBX_TIMES_8_RBX();  \
}

#define CONVERT_RBX_TO_CODE_PTR() {             \
    ASM_MOV_IMM64_RCX(obj.bytecode.size());     \
    ASM_CMP_RBX_RCX();                          \
    ASM_JAE_IMM8(12);                           \
    ASM_MOV_IMM64_RBX(OVERFLOW_CALL);           \
    ASM_JMP_RBX();                              \
    ASM_MOV_IMM64_RCX(code_addr_table_.data()); \
    ASM_MOV_BY_RCX_PLUS_RBX_TIMES_8_RBX();      \
}

#define COMPUTE_ARG(x)                                                              \
    switch (arg_types[x]) {                                                         \
        case ARG_VALUE:                                                             \
            ASM_MOV_IMM64_RBX(arg_values[x]);                                       \
            break;                                                                  \
        case ARG_POINTER: {                                                         \
            ASM_MOV_IMM64_RBX(TO_DATA_PTR(arg_values[x]));                          \
            ASM_MOV_BY_RBX_RBX();                                                   \
        } break;                                                                    \
        case ARG_REGISTER:                                                          \
            ASM_MOV_REG_RBX(arg_values[x]);                                         \
            break;                                                                  \
        case ARG_REGISTER_POINTER:                                                  \
            ASM_MOV_REG_RBX(arg_values[x]);                                         \
            CONVERT_RBX_TO_DATA_PTR();                                              \
            ASM_MOV_BY_RBX_RBX();                                                   \
            break;                                                                  \
    }

#define OVERFLOW_CALL       (reinterpret_cast<void*>(OverflowCall))
#define READ_INT_CALL       (reinterpret_cast<void*>(ReadIntCall))
#define WRITE_INT_CALL      (reinterpret_cast<void*>(WriteIntCall))
#define READ_DOUBLE_CALL    (reinterpret_cast<void*>(ReadDoubleCall))
#define WRITE_DOUBLE_CALL   (reinterpret_cast<void*>(WriteDoubleCall))
#define HALT_CALL           (reinterpret_cast<void*>(HaltCall))
#define FUNC_CALL           (reinterpret_cast<void*>(FuncCall))
#define PRINT_DUMP_CALL     (reinterpret_cast<void*>(PrintDumpCall))

struct Fixup {
    int64_t instruction_pointer;
    size_t native_code_offset;
};

void JITCompiler::Compile(const Object& obj) {
    int64_t bytecode_size = obj.bytecode.size();
    code_addr_table_.assign(bytecode_size, reinterpret_cast<void*>(BadJumpAddressHandler));
    std::vector<int8_t> native_code;
    native_code.reserve(bytecode_size * 32);

    std::vector<Fixup> fixups;

    int64_t instruction_pointer = 0;
    while (instruction_pointer < bytecode_size) {
        fixups.push_back(Fixup{instruction_pointer, native_code.size()});
        int8_t opcode = obj.bytecode[instruction_pointer++];
        switch (opcode) {
#include <instruction_set.h>
            default:
                throw std::runtime_error("Invalid opcode");
        }
    }

    code_.emplace(native_code.size(), PROT_READ | PROT_WRITE | PROT_EXEC);
    std::copy(native_code.begin(), native_code.end(), static_cast<int8_t*>(code_->Begin()));
    for (auto& fixup : fixups) {
        code_addr_table_[fixup.instruction_pointer] = static_cast<int8_t*>(code_->Begin()) + fixup.native_code_offset;
    }
}

void JITCompiler::Execute() {
    if (!code_) {
        throw std::runtime_error("No bytecode provided!");
    }

    PrepareUserContext(user_context, static_cast<char*>(data_stack_.End()), call_stack_.End(), code_->Begin());
    supervisor_context.SwitchTo(user_context);
}
