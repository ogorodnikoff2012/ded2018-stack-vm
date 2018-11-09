#include <linker.h>
#include <oosf/input_data_stream.h>
#include <algorithm>
#include <sstream>

bool TryLoadObject(std::vector<Object>* obj, const char* filename) {
    std::FILE* file = std::fopen(filename, "rb");
    if (file == nullptr) {
        return false;
    }
    InputDataStream dstream(file);
    Object::RegisterIn(&dstream);

    obj->emplace_back();
    ReadStatus result = dstream.TryRead(&obj->back());
    std::fclose(file);

    if (result != kStatusOk) {
        obj->pop_back();
        return false;
    }
    return true;
}

bool TryLink(const std::vector<Object>& objects, Object* executable, std::string* error) {
    for (size_t i = 0; i < objects.size(); ++i) {
        if (!objects[i].proc_version.CompatibleWith(executable->proc_version)) {
            std::stringstream ss;
            ss << "Object file #" << i << " is not compatible with current processor version (current: "
               << executable->proc_version.major << '.' << executable->proc_version.minor << '.' << executable->proc_version.patch
               << ", found: "
               << objects[i].proc_version.major << '.' << objects[i].proc_version.minor << '.' << objects[i].proc_version.patch
               << ")";

            *error = ss.str();
            return false;
        }
    }

    std::vector<int64_t> offsets(objects.size(), 0);
    std::vector<int64_t> bss_offsets(objects.size(), 0);

    size_t total_bytecode_size = 0;
    for (size_t i = 0; i < objects.size(); ++i) {
        offsets[i] = total_bytecode_size;
        total_bytecode_size += objects[i].bytecode.size();
        bss_offsets[i] = executable->bss_size;
        executable->bss_size += objects[i].bss_size;

        for (const auto&[name, symbol] : objects[i].defined_symbols) {
            if (name[0] != '.') {
                if (executable->defined_symbols.find(name) != executable->defined_symbols.end()) {
                    *error = "Redefinition of symbol " + name;
                    return false;
                }
                int64_t symbol_pos = symbol.position;
                if (symbol.type == Symbol::kSymbolFunction) {
                    symbol_pos += offsets[i];
                } else if (symbol.type == Symbol::kSymbolVariable) {
                    symbol_pos += bss_offsets[i];
                }

                executable->defined_symbols[name] = Symbol(symbol_pos, symbol.type);
            }
        }

        executable->bss_size += objects[i].bss_size;
    }
    executable->defined_symbols["__bss_size"] = Symbol(executable->bss_size, Symbol::kSymbolVariable);

    executable->bytecode.resize(total_bytecode_size);
    for (size_t i = 0; i < objects.size(); ++i) {
        std::copy(objects[i].bytecode.begin(), objects[i].bytecode.end(), executable->bytecode.begin() + offsets[i]);
        for (const auto& [pos, name]: objects[i].required_symbols) {
            int64_t real_pos = pos + offsets[i];
            int64_t real_value = 0;
            auto iter = objects[i].defined_symbols.find(name);
            if (iter != objects[i].defined_symbols.end()) {
                real_value = iter->second.position;
                switch (iter->second.type) {
                    case Symbol::kSymbolFunction:
                        real_value += offsets[i];
                        break;
                    case Symbol::kSymbolVariable:
                        real_value += bss_offsets[i];
                        break;
                    default:
                        break;
                }
            } else if ((iter = executable->defined_symbols.find(name)) != executable->defined_symbols.end()) {
                real_value = iter->second.position;
            } else {
                *error = "Undefined symbol " + name;
                return false;
            }
            *reinterpret_cast<int64_t*>(&executable->bytecode[real_pos]) = real_value;
        }
    }

    return true;
}
