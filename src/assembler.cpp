#include <iostream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <cstdlib>

#include "assembler.h"
#include "argument_descriptors.h"
#include "oosf/output_data_stream.h"
#include "object.h"

struct Command {
    int8_t opcode;
    int8_t args_count;
};

namespace {

class CommandTable {
public:
    CommandTable() : command_table_() {
#define DEF_CMD(name, opcode, argcnt, ...) command_table_[( #name )] = {( opcode ), ( argcnt )};
#define DEF_ALIAS(name, canonical) command_table_[( #name )] = command_table_[( #canonical )];
#include "instruction_set.h"
#undef DEF_CMD
#undef DEF_ALIAS
    }

    template <class T>
    inline decltype(auto) operator[](const T& key) {
        return command_table_[key];
    }

    template <class T>
    inline bool Contains(const T& key) const {
        return command_table_.find(key) != command_table_.end();
    }

private:
    std::unordered_map<std::string_view, Command> command_table_;
};

static CommandTable command_table;

}

class Tokenizer {
public:
    Tokenizer(std::string& data) : data_(data), token_begin(data.begin()), token_end(data.begin()) {
        NextToken();
    }

    bool IsEnd() const {
        return token_begin == data_.end() || *token_begin == '#';
    }

    void NextToken() {
        if (IsEnd()) {
            return;
        }
        token_begin = std::find_if_not(token_end, data_.end(), ::isspace);
        token_end = std::find_if(token_begin, data_.end(), ::isspace);
    }

    std::string_view Token() const {
        return std::string_view(token_begin.base(), token_end - token_begin);
    }

    template <class UnaryOp>
    void TransformToken(UnaryOp op) {
        std::transform(token_begin, token_end, token_begin, op);
    }

    size_t GetOffset() const {
        return token_begin - data_.begin();
    }

    size_t GetLength() const {
        return token_end - token_begin;
    }

private:
    std::string& data_;
    std::string::iterator token_begin, token_end;
};

void PrintError(int line_counter, int pos_in_line, int token_length, const std::string& line, const char* msg) {
    std::cerr << "ERROR in line " << line_counter << ": " << msg << std::endl
              << line << std::endl;
    for (int i = 0; i < pos_in_line; ++i) {
        std::cerr.put(' ');
    }
    for (int i = 0; i < token_length; ++i) {
        std::cerr.put('^');
    }
    std::cerr << std::endl;
}

template <class String>
bool ValidName(const String& name) {
    if (name.empty()) {
        return false;
    }
    if (!::isalpha(name[0]) && name[0] != '_' && name[0] != '.') {
        return false;
    }

    for (size_t i = 1; i < name.length(); ++i) {
        if (!::isalnum(name[i]) && name[i] != '_') {
            return false;
        }
    }

    return true;
}

static inline bool ParseLong(long* value, const std::string_view& token) {
    char *result_end = nullptr;
    *value = std::strtol(token.data(), &result_end, 10);
    return result_end == token.data() + token.length();
}

static inline bool ParseDouble(double* value, const std::string_view& token) {
    char *result_end = nullptr;
    *value = std::strtod(token.data(), &result_end);
    return result_end == token.data() + token.length();
}

const char* TryAddSymbol(Object* object, const std::string_view& token, Symbol symbol) {
    std::string name(token);
    if (object->defined_symbols.find(name) != object->defined_symbols.end()) {
        return "This name has already been used";
    }
    if (!ValidName(name)) {
        return "Bad name";
    }
    object->defined_symbols[name] = symbol;
    return nullptr;
}

const char* ParseFunction(Tokenizer* tok, Object* object) {
    tok->NextToken();
    if (tok->IsEnd()) {
        return "Expected function name, got EOL";
    }

    const char* result = TryAddSymbol(object, tok->Token(), Symbol(object->bytecode.size(), Symbol::kSymbolFunction));

    if (result != nullptr) {
        return result;
    }

    tok->NextToken();
    if (!tok->IsEnd()) {
        return "Unexpected token";
    }

    return nullptr;
}

const char* ParseVariable(Tokenizer* tok, Object* object) {
    tok->NextToken();
    if (tok->IsEnd()) {
        return "Expected variable name, got EOL";
    }

    const char* result = TryAddSymbol(object, tok->Token(), Symbol(object->bss_size, Symbol::kSymbolVariable));

    if (result != nullptr) {
        return result;
    }

    long var_size = 1;
    tok->NextToken();
    if (!tok->IsEnd()) {
        auto token = tok->Token();
        if (!ParseLong(&var_size, token) || var_size <= 0) {
            return "Expected a positive integer";
        }
    }

    object->bss_size += var_size;

    tok->NextToken();
    if (!tok->IsEnd()) {
        return "Unexpected token";
    }

    return nullptr;
}

const char* ParseSymbol(Tokenizer* tok, Object* object) {
    tok->NextToken();
    if (tok->IsEnd()) {
        return "Expected symbol name, got EOL";
    }

    const std::string symbol_name(tok->Token());
    const char* result = TryAddSymbol(object, tok->Token(), Symbol(0, Symbol::kSymbolUndefined));

    if (result != nullptr) {
        return result;
    }

    int64_t value = 0;
    tok->NextToken();
    if (tok->IsEnd()) {
        return "Expected value, got EOL";
    }

    auto token = tok->Token();
    if (!ParseLong(&value, token) && !ParseDouble(reinterpret_cast<double*>(&value), token)) {
        return "Expected a number";
    }

    object->defined_symbols[symbol_name].position = value;

    tok->NextToken();
    if (!tok->IsEnd()) {
        return "Unexpected token";
    }

    return nullptr;
}

const char* ParseArgument(Tokenizer* tok, Object* object, int8_t* arg_descr, int8_t index) {
    tok->NextToken();
    if (tok->IsEnd()) {
        return "Not enough arguments";
    }
    auto token = tok->Token();
    int8_t arg_type = ARG_VALUE;
    int prefix_length = 0;

    switch (token[0]) {
        case ASM_PREFIX_POINTER:
            arg_type = ARG_POINTER;
            prefix_length = 1;
            break;
        case ASM_PREFIX_REGISTER:
            arg_type = ARG_REGISTER;
            prefix_length = 1;
            break;
        case ASM_PREFIX_REGISTER_POINTER:
            arg_type = ARG_REGISTER_POINTER;
            prefix_length = 1;
            break;
        default:
            break;
    }

    SetArgType(arg_descr, index, arg_type);
    token.remove_prefix(prefix_length);
    if (arg_type == ARG_VALUE || arg_type == ARG_POINTER) {
        object->bytecode.resize(object->bytecode.size() + sizeof(int64_t));

        void* buffer = static_cast<void*>(object->bytecode.data() + (object->bytecode.size() - sizeof(int64_t)));
        if (long value = 0; ParseLong(&value, token)) {
            *static_cast<int64_t*>(buffer) = value;
        } else if (double value = 0; arg_type == ARG_VALUE && ParseDouble(&value, token)) {
            *static_cast<double*>(buffer) = value;
        } else if (ValidName(token)) {
            object->required_symbols[object->bytecode.size() - sizeof(int64_t)] = token;
        } else {
            return "Invalid label name";
        }
    } else {
        long value = 0;
        if (ParseLong(&value, token) && value >= 0 && value <= MAX_REGISTER) {
            object->bytecode.push_back(value);
        } else {
            return "Invalid register";
        }
    }

    return nullptr;
}

const char* ParseCommand(Tokenizer* tok, Object* object) {
    Command command = ::command_table[tok->Token()];
    object->bytecode.push_back(command.opcode);

    if (command.args_count > 0) {
        object->bytecode.push_back(0);
    }
    int8_t& arguments_descriptor = object->bytecode.back();

    for (int8_t i = 0; i < command.args_count; ++i) {
        const char* error = ParseArgument(tok, object, &arguments_descriptor, i);
        if (error != nullptr) {
            return error;
        }
    }

    tok->NextToken();
    if (!tok->IsEnd()) {
        return "Too much arguments";
    }

    return nullptr;
}

void Assemble(std::ifstream* in, std::ofstream* out) {
#define PERROR(reason) PrintError(line_counter, tokenizer.GetOffset(), std::max(tokenizer.GetLength(), 1UL), line, reason); return;

    std::string line;
    int line_counter = 0;

    Object object;
    object.proc_version.major = PROC_VERSION_MAJOR;
    object.proc_version.minor = PROC_VERSION_MINOR;
    object.proc_version.patch = PROC_VERSION_PATCH;
    object.object_type = Object::kObjectStaticLinkable;

    while (std::getline(*in, line)) {
        ++line_counter;
        Tokenizer tokenizer(line);

        tokenizer.TransformToken(::toupper);
        if (tokenizer.IsEnd()) {
            continue;
        }

        auto cmd = tokenizer.Token();
        const char* error = nullptr;
        if (cmd == "FUNC") {
            error = ParseFunction(&tokenizer, &object);
        } else if (cmd == "VAR") {
            error = ParseVariable(&tokenizer, &object);
        } else if (cmd == "SYMBOL") {
            error = ParseSymbol(&tokenizer, &object);
        } else if (::command_table.Contains(cmd)) {
            error = ParseCommand(&tokenizer, &object);
        } else {
            error = "Unknown instruction";
        }

        if (error != nullptr) {
            PERROR(error);
        }
    }

    OutputDataStream dstream(out, object.defined_symbols.size() + object.required_symbols.size() + 1);
    Object::RegisterIn(&dstream);
    dstream.Write(object);
}

