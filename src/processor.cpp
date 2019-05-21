#include <processor.h>
#include <algorithm>
#include <argument_descriptors.h>
#include <cmath>
#include <map>
#include <string_view>

const Object::ProcVersion& Processor::GetVersion() const {
    return version_;
}

template <class T>
static inline Processor::ExecutionStatus TryGet(const std::vector<int8_t>& bytecode, uint64_t* ip, T* dest) {
    if ((*ip + sizeof(T)) > bytecode.size()) {
        return Processor::kExecStatusIPOutOfRange;
    }
    *dest = *reinterpret_cast<const T*>(&bytecode[*ip]);
    *ip += sizeof(T);
    return Processor::kExecStatusOk;
}


bool Processor::FillArgs(const std::vector<int8_t>& bytecode, int64_t** args, int64_t* arg_stubs, RAM* ram, int argcnt, uint64_t* ip) {
#define TRY_GET(...) if ((status_ = TryGet(__VA_ARGS__)) != kExecStatusOk) { return false; }
    if (argcnt == 0) {
        return true;
    }
    int8_t arg_descriptor = 0;
    if ((status_ = TryGet(bytecode, ip, &arg_descriptor)) != kExecStatusOk) {
        return false;
    }
    for (int i = 0; i < argcnt; ++i) {
        int8_t arg_type = GetArgType(arg_descriptor, i);
        uint8_t reg_buffer = 0;
        bool ram_ok = true;

        switch (arg_type) {
            case ARG_VALUE:
                TRY_GET(bytecode, ip, arg_stubs + i);
                args[i] = arg_stubs + i;
                break;
            case ARG_POINTER:
                TRY_GET(bytecode, ip, arg_stubs + i);
                args[i] = ram->At(arg_stubs[i], &ram_ok);
                if (!ram_ok) {
                    status_ = kExecStatusAddressOutOfRange;
                    return false;
                }
                break;
            case ARG_REGISTER:
                TRY_GET(bytecode, ip, &reg_buffer);
                if (reg_buffer >= registers_.size()) {
                    status_ = kExecStatusRegisterOutOfRange;
                    return false;
                }
                args[i] = &registers_[reg_buffer];
                break;
            case ARG_REGISTER_POINTER:
                TRY_GET(bytecode, ip, &reg_buffer);
                if (reg_buffer >= registers_.size()) {
                    status_ = kExecStatusRegisterOutOfRange;
                    return false;
                }
                args[i] = ram->At(registers_[reg_buffer], &ram_ok);
                if (!ram_ok) {
                    status_ = kExecStatusAddressOutOfRange;
                    return false;
                }
                break;
        }
    }
    return true;
#undef TRY_GET
}

bool Processor::Execute(const std::vector<int8_t>& bytecode, RAM* ram) {

#define FROM_STACK(idx)         from_stack[(idx)]
#define TO_STACK(idx)           to_stack[(idx)]
#define LOAD_ARG(idx, dest)     dest = *args[(idx)]
#define STORE_ARG(idx, src)     *args[(idx)] = src
#define AS_DOUBLE(expr)         (*reinterpret_cast<double*>(&(expr)))
#define READ_INT(dest)          std::scanf("%ld",  &(dest))
#define WRITE_INT(src)          std::printf("%ld\n", (src))
#define READ_DOUBLE(dest)       std::scanf("%lf",  &(dest))
#define WRITE_DOUBLE(src)       std::printf("%lf\n", (src))
#define JUMP_TO(expr)           instruction_pointer_copy = (expr)
#define STOP_PROCESSOR          { status_ = kExecStatusOk; return true; }
#define PRINT_DUMP()            Dump()

#define SAVE_ADDR()                                 \
if (call_stack_.size() == kCallStackMaxSize) {      \
    status_ = kExecStatusCallStackOverflow;         \
    return false;                                   \
}                                                   \
call_stack_.push_back(instruction_pointer_copy);

#define RESTORE_ADDR()                              \
if (call_stack_.empty()) {                          \
    status_ = kExecStatusEmptyCallStack;            \
    return false;                                   \
}                                                   \
instruction_pointer_copy = call_stack_.back();      \
call_stack_.pop_back();

#define ERROR_DIV_ZERO { status_ = kExecStatusDivZero; return false; }


#define DEF_CMD(name, code, argcnt, from_stack_cnt, to_stack_cnt, handler)                  \
case code: {                                                                                \
    if (data_stack_.size() < from_stack_cnt) {                                              \
        status_ = kExecStatusEmptyDataStack;                                                \
        return false;                                                                       \
    }                                                                                       \
    if (data_stack_.size() - from_stack_cnt + to_stack_cnt > kDataStackMaxSize) {           \
        status_ = kExecStatusDataStackOverflow;                                             \
        return false;                                                                       \
    }                                                                                       \
    int64_t* args[argcnt + 1] = {};                                                         \
    int64_t arg_stubs[argcnt + 1] = {};                                                     \
    int64_t from_stack[from_stack_cnt + 1] = {};                                            \
    int64_t to_stack[to_stack_cnt + 1] = {};                                                \
    if (!FillArgs(bytecode, args, arg_stubs, ram, argcnt, &instruction_pointer_copy)) {     \
        return false;                                                                       \
    }                                                                                       \
    std::copy_n(data_stack_.end() - from_stack_cnt, from_stack_cnt, from_stack);            \
    { handler; }                                                                            \
    data_stack_.resize(data_stack_.size() - from_stack_cnt + to_stack_cnt);                 \
    std::copy_n(to_stack, to_stack_cnt, data_stack_.end() - to_stack_cnt);                  \
    break;                                                                                  \
}


    while (true) {
        uint64_t instruction_pointer_copy = instruction_pointer_;

        int8_t opcode = bytecode[instruction_pointer_copy++];
        switch (opcode) {
#include <instruction_set.h>
            default:
                status_ = kExecStatusInvalidOpcode;
                return false;
        }
        instruction_pointer_ = instruction_pointer_copy;
    }
#undef DEF_CMD
}

static constexpr int kRegsInRow = 4;

static void PrintTable(const int64_t* data, int size, int items_in_row) {
    for (int i = 0; i < size; ++i) {
        if (i % items_in_row == 0) {
            std::printf("\n%04X: ", i);
        }
        std::printf("0x%016lX ", data[i]);
    }
}

static inline const char* GetProcessorStatusDescription(Processor::ExecutionStatus status) {
    switch (status) {
        case Processor::kExecStatusOk:
            return "OK";
        case Processor::kExecStatusInvalidOpcode:
            return "Invalid opcode";
        case Processor::kExecStatusIPOutOfRange:
            return "IP out of range";
        case Processor::kExecStatusEmptyCallStack:
            return "Empty call stack";
        case Processor::kExecStatusEmptyDataStack:
            return "Empty data stack";
        case Processor::kExecStatusCallStackOverflow:
            return "Call stack overflow";
        case Processor::kExecStatusDataStackOverflow:
            return "Data stack overflow";
        case Processor::kExecStatusAddressOutOfRange:
            return "Address out of range";
        case Processor::kExecStatusRegisterOutOfRange:
            return "Invalid register number";
        case Processor::kExecStatusDivZero:
            return "Division by zero";
        default:
            return "???";

    }
}

void Processor::Dump() const {
    std::printf("Processor status: 0x%02x (%s)\n", status_, GetProcessorStatusDescription(status_));
    std::printf("Regiters:");
    PrintTable(registers_.data(), registers_.size(), kRegsInRow);
    std::printf("\n");
}

void PrintCallStackLine(const std::map<int64_t, std::string_view>& functions, int64_t addr, int depth) {
    std::printf("\n%04d: 0x%016lX", depth, addr);
    auto func_table_iter = functions.upper_bound(addr);
    if (func_table_iter != functions.begin()) {
        --func_table_iter;
        std::printf(" <%.*s+0x%lX>", static_cast<int>(func_table_iter->second.length()),
                    func_table_iter->second.data(), addr - func_table_iter->first);
    }
}

void Processor::PrintStackTrace(const Object& obj) const {
    std::printf("Stack trace:");
    std::map<int64_t, std::string_view> functions;
    for (const auto& [name, symbol] : obj.defined_symbols) {
        if (symbol.type == Symbol::kSymbolFunction) {
            functions[symbol.position] = name;
        }
    }

    int pointer_index = 0;
    PrintCallStackLine(functions, instruction_pointer_, pointer_index++);
    for (auto iter = call_stack_.rbegin(); iter != call_stack_.rend(); ++iter) {
        PrintCallStackLine(functions, *iter, pointer_index++);
    }
    std::printf("\n");
}
