#include <iostream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <cstdlib>

#include <assembler.h>
#include <argument_descriptors.h>
#include <object.h>

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
#include <instruction_set.h>
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
    if (!::isalpha(name[0]) && name[0] != '_' && name[0] != ',') {
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

#define PERROR(reason) PrintError(line_counter, tokenizer.GetOffset(), std::max(tokenizer.GetLength(), 1UL), line, reason); return;

const char* ParseFunction(Tokenizer* tok, std::unordered_map<std::string, int64_t>* defined_labels, std::vector<int8_t>* bytecode) {
    tok->NextToken();
    if (tok->IsEnd()) {
        return "Expected function name, got EOL";
    }

    std::string func_name(tok->Token());
    if (defined_labels->find(func_name) != defined_labels->end()) {
        return "This name has already been used";
    }
    if (!ValidName(func_name)) {
        return "Bad function name";
    }
    (*defined_labels)[func_name] = bytecode->size();

    tok->NextToken();
    if (!tok->IsEnd()) {
        return "Unexpected token";
    }

    return nullptr;
}

const char* ParseVariable() {
            tokenizer.NextToken();
            if (tokenizer.IsEnd()) {
                PERROR("Expected variable name, got EOL");
            }

            std::string var_name(tokenizer.Token());
            if (defined_labels.find(var_name) != defined_labels.end()) {
                PERROR("This name has already been used");
            }
            if (!ValidName(var_name)) {
                PERROR("Bad variable name");
            }
            defined_labels[var_name] = bss_size;

            long var_size = 1;
            tokenizer.NextToken();
            if (!tokenizer.IsEnd()) {
                auto token = tokenizer.Token();
                if (!ParseLong(&var_size, token) || var_size <= 0) {
                    PERROR("Expected a positive integer");
                }
            }

            bss_size += var_size;

            tokenizer.NextToken();
            if (!tokenizer.IsEnd()) {
                PERROR("Unexpected token");
            }

}

void Assemble(std::ifstream* in, std::ofstream*) {
    std::string line;
    int line_counter = 0;

    std::vector<int8_t> bytecode;
    std::unordered_map<std::string, int64_t> defined_labels;
    std::unordered_map<std::string, int64_t> required_labels;
    int64_t bss_size = 0;

    while (std::getline(*in, line)) {
        ++line_counter;
        Tokenizer tokenizer(line);

        tokenizer.TransformToken(::toupper);
        if (tokenizer.IsEnd()) {
            continue;
        }

        auto cmd = tokenizer.Token();

        if (cmd == "FUNC") {
            const char* error = ParseFunction(&tokenizer, &defined_labels, &bytecode);
            if (error != nullptr) {
                PERROR(error);
            }
        } else if (cmd == "VAR") {
        } else if (::command_table.Contains(cmd)) {
            Command command = ::command_table[cmd];
            bytecode.push_back(command.opcode);

            if (command.args_count > 0) {
                bytecode.push_back(0);
            }
            int8_t& arguments_descriptor = bytecode.back();

            for (int8_t i = 0; i < command.args_count; ++i) {
                tokenizer.NextToken();
                if (tokenizer.IsEnd()) {
                    PERROR("Not enough arguments");
                }
                auto token = tokenizer.Token();
                int8_t arg_type = ARG_VALUE;
                int prefix_length = 0;

                switch (token[0]) {
                    case '*':
                        arg_type = ARG_POINTER;
                        prefix_length = 1;
                        break;
                    case '%':
                        arg_type = ARG_REGISTER;
                        prefix_length = 1;
                        break;
                    case '!':
                        arg_type = ARG_REGISTER_POINTER;
                        prefix_length = 1;
                        break;
                    default:
                        break;
                }

                SetArgType(&arguments_descriptor, i, arg_type);
                token.remove_prefix(prefix_length);
                if (arg_type == ARG_VALUE || arg_type == ARG_POINTER) {
                    bytecode.resize(bytecode.size() + sizeof(int64_t));
                    void* buffer = static_cast<void*>((bytecode.end() - sizeof(int64_t)).base());
                    if (long value = 0; ParseLong(&value, token)) {
                        *static_cast<int64_t*>(buffer) = value;
                    } else if (double value = 0; arg_type == ARG_VALUE && ParseDouble(&value, token)) {
                        *static_cast<double*>(buffer) = value;
                    } else if (ValidName(token)) {
                        required_labels[std::string(token)] = bytecode.size() - sizeof(int64_t);
                    } else {
                        PERROR("Invalid label name");
                    }
                } else {
                    long value = 0;
                    if (ParseLong(&value, token) && value >= 0 && value < (1 << 6)) {
                        bytecode.push_back(value);
                    } else {
                        PERROR("Invalid register");
                    }
                }
            }

            tokenizer.NextToken();
            if (!tokenizer.IsEnd()) {
                PERROR("Too much arguments");
            }
        } else {
//            std::cerr << " # WTF ?!" << std::endl;
            PERROR("Unknown instruction");
        }
    }
}

