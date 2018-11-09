#pragma once

#include <oosf/types.h>
#include <vector>
#include <unordered_map>

struct Symbol : public Serializable {
    enum SymbolType {
        kSymbolFunction     = 0,
        kSymbolVariable     = 1,
        kSymbolUndefined    = 2,
    };
    int64_t position;
    SymbolType type;
    Symbol(int64_t pos = 0, SymbolType t = kSymbolUndefined);

    static constexpr char kTypeName[] = "vsym";

    template <class T>
    static bool RegisterIn(T* t) {
        return t->template RegisterClass<Symbol>(kTypeName);
    }

    virtual ReadStatus TryRead(InputDataStream*) override;
    virtual void WriteValue(OutputDataStream*) const override;
};

struct Object : public Serializable {
    struct ProcVersion {
        int major = 0, minor = 0, patch = 0;
        bool CompatibleWith(const ProcVersion& target) const;
    };

    ProcVersion proc_version;
    std::vector<int8_t> bytecode;
    std::unordered_map<std::string, Symbol> defined_symbols;
    std::unordered_map<int64_t, std::string> required_symbols;
    int64_t bss_size = 0;

    static constexpr char kTypeName[] = "vobj";

    template <class T>
    static bool RegisterIn(T* t) {
        return Symbol::RegisterIn(t) && t->template RegisterClass<Object>(kTypeName);
    }

    virtual ReadStatus TryRead(InputDataStream*) override;
    virtual void WriteValue(OutputDataStream*) const override;
};
