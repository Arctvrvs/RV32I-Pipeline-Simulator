// File: include/cpu/instr.h
#pragma once
#include <cstdint>

namespace cpu {

    enum class InstrKind
    {
        NOP,

        // System / Privileged (minimal M-mode)
        ECALL, EBREAK, MRET,
        CSRRW, CSRRS, CSRRC,
        CSRRWI, CSRRSI, CSRRCI,

        // Decode failed / unsupported
        ILLEGAL,

        // RV32I: R-type
        ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND,

        // RV32I: I-type ALU
        ADDI, SLLI, SLTI, SLTIU, XORI, SRLI, SRAI, ORI, ANDI,

        // RV32I: Loads
        LB, LH, LW, LBU, LHU,

        // RV32I: Stores
        SB, SH, SW,

        // RV32I: Branches
        BEQ, BNE, BLT, BGE, BLTU, BGEU,

        // RV32I: Jumps
        JAL, JALR,

        // RV32I: Upper immediates
        LUI, AUIPC
    };

    struct DecodedInstr
    {
        InstrKind kind = InstrKind::NOP;
        uint8_t   rs1 = 0;
        uint8_t   rs2 = 0;
        uint8_t   rd  = 0;
        int32_t   imm = 0;
        uint32_t  raw = 0;

        // Operand / side-effect usage (used by hazards + forwarding)
        bool uses_rs1   = false;
        bool uses_rs2   = false;
        bool writes_rd  = false;

        // Memory control
        bool mem_read   = false;
        bool mem_write  = false;
        bool mem_to_reg = false;
        uint8_t mem_size = 0;      // bytes: 1,2,4 (0 = none)
        bool mem_signed = false;   // for loads: sign-extend?

        // Control flow classification
        bool is_branch  = false;
        bool is_jump    = false;

        // CSR fields (SYSTEM opcode)
        uint16_t csr_addr = 0;   // CSR number
        uint32_t csr_zimm = 0;   // immediate for *I forms (0..31)
    };

    DecodedInstr decode(uint32_t raw);

} // namespace cpu
