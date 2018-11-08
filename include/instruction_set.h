#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 0

#ifndef DEF_CMD
#define DEF_CMD_UNDEFINED
#define DEF_CMD(name, opcode, argcnt, from_stack_cnt, to_stack_cnt, handler)
#endif

#ifndef DEF_ALIAS
#define DEF_ALIAS_UNDEFINED
#define DEF_ALIAS(name, canonical_name)
#endif

DEF_CMD(PUSH,   0x01, 1, 0, 1, {
    LOAD_ARG(0, TO_STACK(0));
})

DEF_CMD(POP,    0x02, 1, 1, 0, {
    STORE_ARG(0, FROM_STACK(0));
})

#define BINARY_OP(name, opcode, operator_) DEF_CMD(name, opcode, 0, 2, 1, \
        { TO_STACK(0) = FROM_STACK(0) operator_ FROM_STACK(1); })

BINARY_OP(ADD,  0x03, +)
BINARY_OP(SUB,  0x04, -)
BINARY_OP(MUL,  0x05, *)
BINARY_OP(DIV,  0x06, /)
BINARY_OP(MOD,  0x07, %)

BINARY_OP(AND,  0x08, &)
BINARY_OP(OR,   0x09, |)
BINARY_OP(XOR,  0x0A, ^)
BINARY_OP(SHL,  0x0B, <<)
BINARY_OP(SHR,  0x0C, >>)

#undef BINARY_OP

#define FP_BIN_OP(name, opcode, operator_) DEF_CMD(name, opcode, 0, 2, 1, \
        { AS_DOUBLE(TO_STACK(0)) = AS_DOUBLE(FROM_STACK(0) ) operator_ AS_DOUBLE(FROM_STACK(1)); })

FP_BIN_OP(FADD, 0x0D, +)
FP_BIN_OP(FSUB, 0x0E, -)
FP_BIN_OP(FMUL, 0x0F, *)
FP_BIN_OP(FDIV, 0x10, /)

#undef FP_BIN_OP

DEF_CMD(ITD,    0x11, 0, 1, 1, {
    AS_DOUBLE(TO_STACK(0)) = static_cast<double>(FROM_STACK(0));
})

DEF_CMD(DTI,    0x12, 0, 1, 1, {
    TO_STACK(0) = static_cast<int64_t>(AS_DOUBLE(FROM_STACK(0)));
})

DEF_CMD(RDINT,  0x13, 0, 0, 1, {
    TO_STACK(0) = READ_INT();
})

DEF_CMD(WRINT,  0x14, 0, 1, 0, {
    WRITE_INT(FROM_STACK(0));
})

DEF_CMD(RDDBL,  0x15, 0, 0, 1, {
    AS_DOUBLE(TO_STACK(0)) = READ_DOUBLE();
})

DEF_CMD(WRDBL,  0x16, 0, 1, 0, {
    WRITE_DOUBLE(AS_DOUBLE(FROM_STACK(0)));
})

DEF_CMD(HALT,   0x17, 0, 0, 0, {
    STOP_PROCESSOR
})

DEF_CMD(JMP,    0x18, 1, 0, 0, {
    int64_t addr = 0;
    LOAD_ARG_ADDR(0, addr);
    JUMP_TO(addr);
})

#define COND_JMP(name, opcode, operator_) DEF_CMD(name, opcode, 1, 1, 1, {  \
    int64_t addr = 0;                                                       \
    LOAD_ARG_ADDR(0, addr);                                                 \
    if (FROM_STACK(0) operator_ 0) {                                        \
        JUMP_TO(addr);                                                      \
    }                                                                       \
    TO_STACK(0) = FROM_STACK(0);                                            \
})

COND_JMP(JEQ,   0x19, ==)
COND_JMP(JGT,   0x1A, >)
COND_JMP(JLT,   0x1B, <)
COND_JMP(JNE,   0x1C, !=)
COND_JMP(JGE,   0x1D, >=)
COND_JMP(JLE,   0x1E, <=)

DEF_ALIAS(JZ, JEQ)
DEF_ALIAS(JP, JGT)
DEF_ALIAS(JN, JLT)
DEF_ALIAS(JNZ, JNE)

#undef COND_JMP

DEF_CMD(CMP,    0x1F, 0, 2, 3, {
    TO_STACK(0) = FROM_STACK(0);
    TO_STACK(1) = FROM_STACK(1);
    TO_STACK(2) = FROM_STACK(0) - FROM_STACK(1);
})

DEF_CMD(FISNAN, 0x20, 0, 1, 2, {
    TO_STACK(0) = FROM_STACK(0);
    TO_STACK(1) = std::isnan(AS_DOUBLE(FROM_STACK(0)));
})

DEF_CMD(FSGN,   0x21, 0, 1, 1, {
    double d = AS_DOUBLE(FROM_STACK(0));
    TO_STACK(0) = (d < -0.0) ? -1 : (d > 0.0 ? 1 : 0);
})

DEF_CMD(FISINF, 0x22, 0, 1, 2, {
    TO_STACK(0) = FROM_STACK(0);
    TO_STACK(1) = std::isinf(AS_DOUBLE(FROM_STACK(0)));
})

DEF_CMD(CALL,   0x23, 1, 0, 0, {
    int64_t addr = 0;
    LOAD_ARG(0, addr);
    SAVE_ADDR();
    JUMP_TO(addr);
})

DEF_CMD(RET,    0x24, 0, 0, 0, {
    RESTORE_ADDR();
})

DEF_CMD(NOP,    0x25, 0, 0, 0, {})

DEF_CMD(DUMP,   0x26, 0, 0, 0, {
    PRINT_DUMP();
})

#ifdef DEF_ALIAS_UNDEFINED
#undef DEF_ALIAS_UNDEFINED
#undef DEF_ALIAS
#endif

#ifdef DEF_CMD_UNDEFINED
#undef DEF_CMD_UNDEFINED
#undef DEF_CMD
#endif
