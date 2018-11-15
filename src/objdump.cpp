#include <object.h>
#include <argument_descriptors.h>
#include <cstdio>

static inline const char* GetName(Symbol::SymbolType type) {
    switch (type) {
        case Symbol::kSymbolFunction:
            return "FUNCTION";
        case Symbol::kSymbolVariable:
            return "VARIABLE";
        case Symbol::kSymbolUndefined:
            return "UNDEFINED";
        default:
            return "???";
    }
}

void PrintProcInfo(const Object& obj) {
    std::printf("Processor version: %d.%d.%d\n", obj.proc_version.major, obj.proc_version.minor, obj.proc_version.patch);
}

static inline const char* GetObjectType(Object::ObjectType type) {
    switch (type) {
        case Object::kObjectExecutable:
            return "Executable";
        case Object::kObjectStaticLinkable:
            return "Static Linkable";
        default:
            return "???";
    }
}

void PrintObjectType(const Object& obj) {
    std::printf("Object file type: %s\n", GetObjectType(obj.object_type));
}

void PrintSymbols(const Object& obj) {
    std::printf("Symbols:\n");
    for (const auto&[name, symbol] : obj.defined_symbols) {
        std::printf("%s\t%s\t0x%lx\n", name.c_str(), GetName(symbol.type), symbol.position);
    }
    std::printf("Total %lu symbols.\n", obj.defined_symbols.size());
}

void PrintArgument(int8_t arg_type, const Object& obj, size_t* instruction_pointer) {
    std::fputc(' ', stdout);
    switch (arg_type) {
        case ARG_POINTER:
            std::fputc(ASM_PREFIX_POINTER, stdout);
        case ARG_VALUE: {
                int64_t addr = *reinterpret_cast<const int64_t*>(&obj.bytecode[*instruction_pointer]);
                auto iter = obj.required_symbols.find(*instruction_pointer);
                *instruction_pointer += 8;
                if (iter != obj.required_symbols.end()) {
                    std::printf("%s", iter->second.c_str());
                } else {
                    std::printf("0x%016lx", addr);
                }
            } break;
        case ARG_REGISTER:
            std::printf("%c%hhu", ASM_PREFIX_REGISTER, obj.bytecode[*instruction_pointer]);
            *instruction_pointer += 1;
            break;
        case ARG_REGISTER_POINTER:
            std::printf("%c%hhu", ASM_PREFIX_REGISTER_POINTER, obj.bytecode[*instruction_pointer]);
            *instruction_pointer += 1;
            break;
    }
}

void PrintListing(const Object& obj) {
    std::printf("Listing:\n\n");

    std::unordered_map<int64_t, std::string_view> functions;
    for (const auto&[name, symbol] : obj.defined_symbols) {
        if (symbol.type == Symbol::kSymbolFunction) {
            functions[symbol.position] = name;
        }
    }

    size_t instruction_pointer = 0;
    while (instruction_pointer < obj.bytecode.size()) {
        auto iter = functions.find(instruction_pointer);
        if (iter != functions.end()) {
            std::printf("%*s FUNC %.*s\n", 16, "", static_cast<int>(iter->second.size()), iter->second.data());
        }
        std::printf("%016lx:", instruction_pointer);
        int8_t cur_opcode = obj.bytecode[instruction_pointer++];

#define DEF_CMD(name, opcode, argcnt, ...) case opcode: {                                   \
    std::printf("    %s", #name );                                                          \
    if (argcnt > 0) {                                                                       \
        int8_t argument_descriptor = obj.bytecode[instruction_pointer++];                   \
        for (int i = 0; i < argcnt; ++i) {                                                  \
            PrintArgument(GetArgType(argument_descriptor, i), obj, &instruction_pointer);   \
        }                                                                                   \
    }                                                                                       \
    std::fputc('\n', stdout);                                                               \
} break;
        switch (cur_opcode) {
            #include <instruction_set.h>
            default:
                std::printf("Invalid opcode: 0x%02hhX\n", cur_opcode);
                return;
                break;
        }
#undef DEF_CMD
    }
    std::fputc('\n', stdout);
}

void PrintObject(const Object& obj) {
    PrintProcInfo(obj);
    PrintObjectType(obj);
    PrintSymbols(obj);
    PrintListing(obj);
}

