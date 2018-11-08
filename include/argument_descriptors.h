#pragma once
#include <cstdint>

#define ARG_VALUE               0
#define ARG_POINTER             1
#define ARG_REGISTER            2
#define ARG_REGISTER_POINTER    3

static inline void SetArgType(int8_t* descriptor, int index, int8_t type) {
    *descriptor &= ~(3 << (index << 1));
    *descriptor |= type << (index << 1);
}

static inline int8_t GetArgType(int8_t descriptor, int index) {
    return (descriptor >> (index << 1)) & 3;
}
