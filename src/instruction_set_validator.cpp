#include <iostream>
#include <iomanip>
#include <set>
#include <map>
#include <vector>
#include <string>

bool test_for_collisions() {
    std::set<std::string> names;
    std::map<int, std::string> opcodes;
#define DEF_CMD(name, opcode, ...) \
    if (names.find(#name) != names.end()) { \
        std::cerr << "NAME COLLISION: " << #name << std::endl; \
        return false; \
    } \
    names.insert(#name); \
    if (opcodes.find(opcode) != opcodes.end()) { \
        std::cerr << "OPCODE COLLISION: " << (opcode) << "; OLD NAME: " << opcodes[opcode] \
                  << "; NEW NAME: " << #name << std::endl; \
        return false; \
    } \
    opcodes[opcode] = #name;

#include <instruction_set.h>
#undef DEF_CMD
    return true;
}

bool just_write_em_all() {
    std::cerr << "Commands:\n";
#define DEF_CMD(name, opcode, argcnt, from_stack_cnt, to_stack_cnt, ...) \
    std::cerr << std::setw(2) << opcode << ' ' << #name << ":  \t" << argcnt << " args, " << from_stack_cnt << " pops, " << to_stack_cnt << " pushes\n";
#include <instruction_set.h>
#undef DEF_CMD

    std::cerr << "\nAliases:\n";
#define DEF_ALIAS(name, canonical) std::cerr << #name << " = " << #canonical << std::endl;
#include <instruction_set.h>
#undef DEF_ALIAS
    return true;
}

int main() {
#define RUN_TEST(test_name) std::cout << "=== " #test_name " ===" << std::endl; \
    ++total_tests; \
    if (test_name()) { \
        ++ok_tests; \
        std::cout << "OK" << std::endl; \
    } else { \
        std::cout << "Failed" << std::endl; \
    }

    int total_tests = 0;
    int ok_tests = 0;

    RUN_TEST(test_for_collisions);
    RUN_TEST(just_write_em_all);

    std::cout << "=== SUMMARY ===" << std::endl;
    std::cout << ok_tests << " of " << total_tests << " passed" << std::endl;

    return 0;
}
