#ifdef PROC_VERSION_MAJOR
#undef PROC_VERSION_MAJOR
#endif

#ifdef PROC_VERSION_MINOR
#undef PROC_VERSION_MINOR
#endif

#ifdef PROC_VERSION_PATCH
#undef PROC_VERSION_PATCH
#endif

#define PROC_VERSION_MAJOR 0
#define PROC_VERSION_MINOR 5
#define PROC_VERSION_PATCH 0

#ifdef MAX_REGISTER
#undef MAX_REGISTER
#endif

#define MAX_REGISTER ((1 << 3) - 1)

#ifndef DEF_CMD
#define DEF_CMD_UNDEFINED
#define DEF_CMD(name, opcode, argcnt, from_stack_cnt, to_stack_cnt, handler, native_codegen)
#endif

#ifndef DEF_ALIAS
#define DEF_ALIAS_UNDEFINED
#define DEF_ALIAS(name, canonical_name)
#endif

DEF_CMD(PUSH,   0x01, 1, 0, 1, {
    LOAD_ARG(0, TO_STACK(0));
}, {
    ASM_PUSH_RAX();
    COMPUTE_ARG(0);
    ASM_MOV_RBX_RAX();
})

DEF_CMD(POP,    0x02, 1, 1, 0, {
    STORE_ARG(0, FROM_STACK(0));
}, {
    switch (ARG_TYPE(0)) {
        case ARG_VALUE:
            // DO NOTHING
            break;
        case ARG_POINTER:
            ASM_MOV_RAX_BY_PTR(TO_DATA_PTR(ARG(0)));
            break;
        case ARG_REGISTER:
            ASM_MOV_RAX_REG(ARG(0));
            break;
        case ARG_REGISTER_POINTER:
            ASM_MOV_REG_RBX(ARG(0));
            CONVERT_RBX_TO_DATA_PTR();
            ASM_MOV_RAX_BY_RBX();
            break;
    }
    ASM_POP_RAX();
})

#define BINARY_OP(name, opcode, operator_, asm_op) DEF_CMD(name, opcode, 0, 2, 1, \
        { TO_STACK(0) = FROM_STACK(0) operator_ FROM_STACK(1); }, {\
            ASM_POP_RBX(); { asm_op ;} \
        })

BINARY_OP(ADD,  0x03, +, { ASM_ADD_RBX_RAX();  })
BINARY_OP(SUB,  0x04, -, { ASM_SUB_RAX_RBX(); ASM_XCHG_RAX_RBX(); })
BINARY_OP(MUL,  0x05, *, { ASM_IMUL_RBX_RAX(); })
// BINARY_OP(DIV,  0x06, /)
// BINARY_OP(MOD,  0x07, %)
DEF_CMD(DIV, 0x06, 0, 2, 1, {
    if (FROM_STACK(1) == 0) {
        ERROR_DIV_ZERO;
    }
    TO_STACK(0) = FROM_STACK(0) / FROM_STACK(1);
}, {
    ASM_MOV_RAX_RBX();
    ASM_POP_RAX();
    ASM_MOV_RAX_RDX();
    ASM_SAR_IMM8_RDX(63);
    ASM_IDIV_RBX();
})

DEF_CMD(MOD, 0x07, 0, 2, 1, {
    if (FROM_STACK(1) == 0) {
        ERROR_DIV_ZERO;
    }
    TO_STACK(0) = FROM_STACK(0) % FROM_STACK(1);
}, {
    ASM_MOV_RAX_RBX();
    ASM_POP_RAX();
    ASM_MOV_RAX_RDX();
    ASM_SAR_IMM8_RDX(63);
    ASM_IDIV_RBX();
    ASM_MOV_RDX_RAX();
})

BINARY_OP(AND,  0x08, &, { ASM_AND_RBX_RAX(); })
BINARY_OP(OR,   0x09, |, { ASM_OR_RBX_RAX(); })
BINARY_OP(XOR,  0x0A, ^, { ASM_XOR_RBX_RAX(); })
BINARY_OP(SHL,  0x0B, <<, { ASM_MOV_BL_CL(); ASM_SHL_CL_RAX(); })
BINARY_OP(SHR,  0x0C, >>, { ASM_MOV_BL_CL(); ASM_SHR_CL_RAX(); })

BINARY_OP(CLT,  0x33, <, { ASM_CMP_RAX_RBX(); ASM_SETL_AL(); ASM_MOVZX_AL_RAX(); })
BINARY_OP(CGT,  0x34, >, { ASM_CMP_RAX_RBX(); ASM_SETG_AL(); ASM_MOVZX_AL_RAX(); })
BINARY_OP(CLE,  0x35, <=, { ASM_CMP_RAX_RBX(); ASM_SETLE_AL(); ASM_MOVZX_AL_RAX(); })
BINARY_OP(CGE,  0x36, >=, { ASM_CMP_RAX_RBX(); ASM_SETGE_AL(); ASM_MOVZX_AL_RAX(); })
BINARY_OP(CEQ,  0x37, ==, { ASM_CMP_RAX_RBX(); ASM_SETE_AL(); ASM_MOVZX_AL_RAX(); })
BINARY_OP(CNE,  0x38, !=, { ASM_CMP_RAX_RBX(); ASM_SETNE_AL(); ASM_MOVZX_AL_RAX(); })

#undef BINARY_OP

#define FP_BIN_OP(name, opcode, operator_, asm_op) DEF_CMD(name, opcode, 0, 2, 1, \
        { AS_DOUBLE(TO_STACK(0)) = AS_DOUBLE(FROM_STACK(0) ) operator_ AS_DOUBLE(FROM_STACK(1)); }, {\
            ASM_MOVSD_BY_RSP_XMM0();    \
            ASM_MOV_RAX_BY_RSP();       \
            { asm_op }                  \
            ASM_MOV_XMM0_RAX();         \
            ASM_ADD_IMM8_RSP(8);        \
        })

FP_BIN_OP(FADD, 0x0D, +, { ASM_ADDSD_BY_RSP_XMM0(); })
FP_BIN_OP(FSUB, 0x0E, -, { ASM_SUBSD_BY_RSP_XMM0(); })
FP_BIN_OP(FMUL, 0x0F, *, { ASM_MULSD_BY_RSP_XMM0(); })
FP_BIN_OP(FDIV, 0x10, /, { ASM_DIVSD_BY_RSP_XMM0(); })

#undef FP_BIN_OP

DEF_CMD(ITD,    0x11, 0, 1, 1, {
    AS_DOUBLE(TO_STACK(0)) = static_cast<double>(FROM_STACK(0));
}, {
    ASM_CVTSI2SD_RAX_XMM0();
    ASM_MOV_XMM0_RAX();
})

DEF_CMD(DTI,    0x12, 0, 1, 1, {
    TO_STACK(0) = static_cast<int64_t>(AS_DOUBLE(FROM_STACK(0)));
}, {
    ASM_MOV_RAX_XMM0();
    ASM_CVTTSD2SI_XMM0_RAX();
})

DEF_CMD(RDINT,  0x13, 0, 0, 1, {
    READ_INT(TO_STACK(0));
}, {
    ASM_PUSH_RAX();
    ASM_SAVE_REGS();
    ASM_CALL_VIA_RAX(READ_INT_CALL);
    ASM_MOV_RAX_RBX();
    ASM_RESTORE_REGS();
    ASM_MOV_RBX_RAX();
})

DEF_CMD(WRINT,  0x14, 0, 1, 0, {
    WRITE_INT(FROM_STACK(0));
}, {
    ASM_MOV_RAX_RDI();
    ASM_SAVE_REGS();
    ASM_CALL_VIA_RAX(WRITE_INT_CALL);
    ASM_RESTORE_REGS();
    ASM_POP_RAX();
})

DEF_CMD(RDDBL,  0x15, 0, 0, 1, {
    READ_DOUBLE(AS_DOUBLE(TO_STACK(0)));
}, {
    ASM_PUSH_RAX();
    ASM_SAVE_REGS();
    ASM_CALL_VIA_RAX(READ_DOUBLE_CALL);
    ASM_RESTORE_REGS();
    ASM_MOV_XMM0_RAX();
})

DEF_CMD(WRDBL,  0x16, 0, 1, 0, {
    WRITE_DOUBLE(AS_DOUBLE(FROM_STACK(0)));
}, {
    ASM_MOV_RAX_XMM0();
    ASM_SAVE_REGS();
    ASM_CALL_VIA_RAX(WRITE_DOUBLE_CALL);
    ASM_RESTORE_REGS();
    ASM_POP_RAX();
})

DEF_CMD(HALT,   0x17, 0, 0, 0, {
    STOP_PROCESSOR
}, {
    ASM_CALL_VIA_RAX(HALT_CALL);
})

DEF_CMD(JMP,    0x18, 1, 0, 0, {
    int64_t addr = 0;
    LOAD_ARG(0, addr);
    JUMP_TO(addr);
}, {
    COMPUTE_ARG(0);
    CONVERT_RBX_TO_CODE_PTR();
    ASM_JMP_RBX();
})

#define COND_JMP(name, opcode, operator_, asm_op) DEF_CMD(name, opcode, 1, 1, 1, {  \
    int64_t addr = 0;                                                               \
    LOAD_ARG(0, addr);                                                              \
    if (FROM_STACK(0) operator_ 0) {                                                \
        JUMP_TO(addr);                                                              \
    }                                                                               \
    TO_STACK(0) = FROM_STACK(0);                                                    \
}, {                                                                                \
    COMPUTE_ARG(0);                                                                 \
    CONVERT_RBX_TO_CODE_PTR();                                                      \
    ASM_CMP_IMM8_RAX(0);                                                            \
    { asm_op }                                                                      \
})

COND_JMP(JEQ,   0x19, ==, { ASM_JNE_REL8(2); ASM_JMP_RBX(); })
COND_JMP(JGT,   0x1A, >, { ASM_JLE_REL8(2); ASM_JMP_RBX(); })
COND_JMP(JLT,   0x1B, <, { ASM_JGE_REL8(2); ASM_JMP_RBX(); })
COND_JMP(JNE,   0x1C, !=, { ASM_JE_REL8(2); ASM_JMP_RBX(); })
COND_JMP(JGE,   0x1D, >=, { ASM_JL_REL8(2); ASM_JMP_RBX(); })
COND_JMP(JLE,   0x1E, <=, { ASM_JG_REL8(2); ASM_JMP_RBX(); })

DEF_ALIAS(JZ, JEQ)
DEF_ALIAS(JP, JGT)
DEF_ALIAS(JN, JLT)
DEF_ALIAS(JNZ, JNE)
DEF_ALIAS(JIF, JNE)

#undef COND_JMP

DEF_CMD(CMP,    0x1F, 0, 2, 3, {
    TO_STACK(0) = FROM_STACK(0);
    TO_STACK(1) = FROM_STACK(1);
    TO_STACK(2) = FROM_STACK(0) - FROM_STACK(1);
}, {
    ASM_MOV_BY_RSP_RBX();
    ASM_PUSH_RAX();
    ASM_SUB_RAX_RBX();
    ASM_MOV_RBX_RAX();
})

DEF_CMD(FISNAN, 0x20, 0, 1, 2, {
    TO_STACK(0) = FROM_STACK(0);
    TO_STACK(1) = std::isnan(AS_DOUBLE(FROM_STACK(0)));
}, {
    ASM_PUSH_RAX();
    ASM_MOV_RAX_XMM0();
    ASM_UCOMISD_XMM0_XMM0();
    ASM_SETNP_AL();
    ASM_MOVZX_AL_RAX();
})

DEF_CMD(FSGN,   0x21, 0, 1, 1, {
    double d = AS_DOUBLE(FROM_STACK(0));
    TO_STACK(0) = (d < -0.0) ? -1 : (d > 0.0 ? 1 : 0);
}, {
    ASM_ZERO_XMM1();
    ASM_MOV_RAX_XMM0();
    ASM_COMISD_XMM1_XMM0();
    ASM_SETA_AL();
    ASM_SETB_BL();
    ASM_SUB_BL_AL();
    ASM_MOVSX_AL_RAX();
})

DEF_CMD(FISINF, 0x22, 0, 1, 2, {
    TO_STACK(0) = FROM_STACK(0);
    TO_STACK(1) = std::isinf(AS_DOUBLE(FROM_STACK(0)));
}, {
    ASM_PUSH_RAX();
    ASM_SHL_RAX();
    ASM_MOV_IMM16_RBX(0x7FF);
    ASM_SHL_IMM8_RBX(53);
    ASM_CMP_RAX_RBX();
    ASM_SETE_AL();
    ASM_MOVZX_AL_RAX();
})

DEF_CMD(CALL,   0x23, 1, 0, 0, {
    int64_t addr = 0;
    LOAD_ARG(0, addr);
    SAVE_ADDR();
    JUMP_TO(addr);
}, {
    COMPUTE_ARG(0);
    CONVERT_RBX_TO_CODE_PTR();
    ASM_MOV_RAX_RCX();
    ASM_CALL_VIA_RAX(FUNC_CALL);
})

DEF_CMD(RET,    0x24, 0, 0, 0, {
    RESTORE_ADDR();
}, {
    ASM_MOV_BY_RBP_RBX();
    ASM_ADD_IMM8_RBP(8);
    ASM_PUSH_RBX();
    ASM_RET();
})

DEF_CMD(NOP,    0x25, 0, 0, 0, {}, { ASM_NOP(); })

DEF_CMD(DUMP,   0x26, 0, 0, 0, {
    PRINT_DUMP();
}, {
    ASM_SAVE_REGS();
    ASM_CALL_VIA_RAX(PRINT_DUMP_CALL);
    ASM_RESTORE_REGS();
})

DEF_CMD(DUP,    0x27, 0, 1, 2, {
    TO_STACK(0) = FROM_STACK(0);
    TO_STACK(1) = FROM_STACK(0);
}, {
    ASM_PUSH_RAX();
})

DEF_CMD(NEG,    0x28, 0, 1, 1, {
    TO_STACK(0) = -FROM_STACK(0);
}, {
    ASM_NEG_RAX();
})

DEF_CMD(FNEG,   0x29, 0, 1, 1, {
    AS_DOUBLE(TO_STACK(0)) = -AS_DOUBLE(FROM_STACK(0));
}, {
    ASM_ZERO_RBX();
    ASM_INC_RBX();
    ASM_SHL_IMM8_RBX(63);
    ASM_XOR_RBX_RAX();
})

DEF_CMD(SWAP,   0x30, 0, 2, 2, {
    TO_STACK(0) = FROM_STACK(1);
    TO_STACK(1) = FROM_STACK(0);
}, {
    ASM_XCHG_RAX_BY_RSP();
})

DEF_CMD(BOOL,   0x31, 0, 1, 1, {
    TO_STACK(0) = static_cast<bool>(FROM_STACK(0));
}, {
    ASM_TEST_RAX_RAX();
    ASM_SETZ_AL();
    ASM_MOVZX_AL_RAX();
})

DEF_CMD(NOT,    0x32, 0, 1, 1, {
    TO_STACK(0) = !(FROM_STACK(0));
}, {
    ASM_TEST_RAX_RAX();
    ASM_SETNZ_AL();
    ASM_MOVZX_AL_RAX();
})

#ifdef DEF_ALIAS_UNDEFINED
#undef DEF_ALIAS_UNDEFINED
#undef DEF_ALIAS
#endif

#ifdef DEF_CMD_UNDEFINED
#undef DEF_CMD_UNDEFINED
#undef DEF_CMD
#endif
