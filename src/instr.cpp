#include "cpu/instr.h"

namespace cpu
{
    static inline uint32_t get_bits(uint32_t x, int hi, int lo)
    {
        return (x >> lo) & ((1u << (hi - lo + 1)) - 1u);
    }

    static inline int32_t sign_extend(uint32_t x, int bits)
    {
        uint32_t m = 1u << (bits - 1);
        return static_cast<int32_t>((x ^ m) - m);
    }

    DecodedInstr decode(uint32_t raw)
    {
        DecodedInstr d{};
        d.raw = raw;

        // Defaults: treat unknown as ILLEGAL (except raw==0 => NOP)
        d.kind = (raw == 0) ? InstrKind::NOP : InstrKind::ILLEGAL;
        d.rs1 = d.rs2 = d.rd = 0;
        d.imm = 0;

        d.uses_rs1 = d.uses_rs2 = d.writes_rd = false;
        d.mem_read = d.mem_write = d.mem_to_reg = false;
        d.mem_size = 0;
        d.mem_signed = false;
        d.is_branch = d.is_jump = false;
        d.csr_addr = 0;
        d.csr_zimm = 0;

        if (raw == 0) return d;

        const uint32_t opcode = get_bits(raw, 6, 0);
        const uint32_t rd     = get_bits(raw, 11, 7);
        const uint32_t funct3 = get_bits(raw, 14, 12);
        const uint32_t rs1    = get_bits(raw, 19, 15);
        const uint32_t rs2    = get_bits(raw, 24, 20);
        const uint32_t funct7 = get_bits(raw, 31, 25);

        d.rs1 = static_cast<uint8_t>(rs1);
        d.rs2 = static_cast<uint8_t>(rs2);
        d.rd  = static_cast<uint8_t>(rd);

        switch (opcode)
        {
        case 0b0110011: { // R-type
            switch (funct3) {
            case 0b000:
                if (funct7 == 0b0000000) d.kind = InstrKind::ADD;
                else if (funct7 == 0b0100000) d.kind = InstrKind::SUB;
                break;
            case 0b001:
                if (funct7 == 0b0000000) d.kind = InstrKind::SLL;
                break;
            case 0b010:
                if (funct7 == 0b0000000) d.kind = InstrKind::SLT;
                break;
            case 0b011:
                if (funct7 == 0b0000000) d.kind = InstrKind::SLTU;
                break;
            case 0b100:
                if (funct7 == 0b0000000) d.kind = InstrKind::XOR;
                break;
            case 0b101:
                if (funct7 == 0b0000000) d.kind = InstrKind::SRL;
                else if (funct7 == 0b0100000) d.kind = InstrKind::SRA;
                break;
            case 0b110:
                if (funct7 == 0b0000000) d.kind = InstrKind::OR;
                break;
            case 0b111:
                if (funct7 == 0b0000000) d.kind = InstrKind::AND;
                break;
            default:
                break;
            }

            if (d.kind != InstrKind::ILLEGAL) {
                d.uses_rs1 = true;
                d.uses_rs2 = true;
                d.writes_rd = true;
            }
            break;
        }

        case 0b0010011: { // I-type ALU
            const uint32_t imm12 = get_bits(raw, 31, 20);
            d.imm = sign_extend(imm12, 12);

            switch (funct3) {
            case 0b000: d.kind = InstrKind::ADDI; break;
            case 0b010: d.kind = InstrKind::SLTI; break;
            case 0b011: d.kind = InstrKind::SLTIU; break;
            case 0b100: d.kind = InstrKind::XORI; break;
            case 0b110: d.kind = InstrKind::ORI; break;
            case 0b111: d.kind = InstrKind::ANDI; break;

            case 0b001: // SLLI
                if (funct7 == 0b0000000) {
                    d.kind = InstrKind::SLLI;
                    d.imm  = static_cast<int32_t>(get_bits(raw, 24, 20));
                }
                break;

            case 0b101:
                if (funct7 == 0b0000000) {
                    d.kind = InstrKind::SRLI;
                    d.imm  = static_cast<int32_t>(get_bits(raw, 24, 20));
                } else if (funct7 == 0b0100000) {
                    d.kind = InstrKind::SRAI;
                    d.imm  = static_cast<int32_t>(get_bits(raw, 24, 20));
                }
                break;
            default:
                break;
            }

            if (d.kind != InstrKind::ILLEGAL) {
                d.uses_rs1 = true;
                d.writes_rd = true;
            }
            break;
        }

        case 0b0000011: { // LOAD
            d.imm = sign_extend(get_bits(raw, 31, 20), 12);
            switch (funct3) {
            case 0b000: d.kind = InstrKind::LB;  d.mem_size=1; d.mem_signed=true;  break;
            case 0b001: d.kind = InstrKind::LH;  d.mem_size=2; d.mem_signed=true;  break;
            case 0b010: d.kind = InstrKind::LW;  d.mem_size=4; d.mem_signed=true;  break;
            case 0b100: d.kind = InstrKind::LBU; d.mem_size=1; d.mem_signed=false; break;
            case 0b101: d.kind = InstrKind::LHU; d.mem_size=2; d.mem_signed=false; break;
            default:
                break;
            }
            if (d.kind != InstrKind::ILLEGAL) {
                d.uses_rs1 = true;
                d.writes_rd = true;
                d.mem_read = true;
                d.mem_to_reg = true;
            }
            break;
        }

        case 0b0100011: { // STORE
            uint32_t imm_low  = get_bits(raw, 11, 7);
            uint32_t imm_high = get_bits(raw, 31, 25);
            uint32_t imm_raw  = (imm_high << 5) | imm_low;
            d.imm = sign_extend(imm_raw, 12);

            switch (funct3) {
            case 0b000: d.kind = InstrKind::SB; d.mem_size=1; break;
            case 0b001: d.kind = InstrKind::SH; d.mem_size=2; break;
            case 0b010: d.kind = InstrKind::SW; d.mem_size=4; break;
            default:
                break;
            }

            if (d.kind != InstrKind::ILLEGAL) {
                d.uses_rs1 = true;
                d.uses_rs2 = true;
                d.mem_write = true;
            }
            break;
        }

        case 0b1100011: { // BRANCH
            uint32_t imm =
                (get_bits(raw, 31, 31) << 12) |
                (get_bits(raw, 7, 7)   << 11) |
                (get_bits(raw, 30, 25) << 5)  |
                (get_bits(raw, 11, 8)  << 1);
            d.imm = sign_extend(imm, 13);

            switch (funct3) {
            case 0b000: d.kind = InstrKind::BEQ;  break;
            case 0b001: d.kind = InstrKind::BNE;  break;
            case 0b100: d.kind = InstrKind::BLT;  break;
            case 0b101: d.kind = InstrKind::BGE;  break;
            case 0b110: d.kind = InstrKind::BLTU; break;
            case 0b111: d.kind = InstrKind::BGEU; break;
            default:
                break;
            }

            if (d.kind != InstrKind::ILLEGAL) {
                d.uses_rs1 = true;
                d.uses_rs2 = true;
                d.is_branch = true;
            }
            break;
        }

        case 0b1101111: { // JAL
            uint32_t imm =
                (get_bits(raw, 31, 31) << 20) |
                (get_bits(raw, 19, 12) << 12) |
                (get_bits(raw, 20, 20) << 11) |
                (get_bits(raw, 30, 21) << 1);
            d.imm = sign_extend(imm, 21);

            d.kind = InstrKind::JAL;
            d.is_jump = true;
            d.writes_rd = true;
            break;
        }

        case 0b1100111: { // JALR
            if (funct3 == 0b000) {
                d.kind = InstrKind::JALR;
                d.is_jump = true;
                d.uses_rs1 = true;
                d.writes_rd = true;
                d.imm = sign_extend(get_bits(raw, 31, 20), 12);
            }
            break;
        }

        case 0b0110111: { // LUI
            d.kind = InstrKind::LUI;
            d.writes_rd = true;
            d.imm = static_cast<int32_t>(raw & 0xFFFFF000u);
            break;
        }

        case 0b0010111: { // AUIPC
            d.kind = InstrKind::AUIPC;
            d.writes_rd = true;
            d.imm = static_cast<int32_t>(raw & 0xFFFFF000u);
            break;
        }

        case 0b1110011: { // SYSTEM / CSR
            uint32_t imm12 = get_bits(raw, 31, 20);
            d.csr_addr = static_cast<uint16_t>(imm12);

            if (funct3 == 0b000) {
                // Environment / returns
                if (imm12 == 0x000) {
                    d.kind = InstrKind::ECALL;
                } else if (imm12 == 0x001) {
                    d.kind = InstrKind::EBREAK;
                } else if (imm12 == 0x302) {
                    d.kind = InstrKind::MRET;
                } else {
                    // unknown SYSTEM
                    d.kind = InstrKind::ILLEGAL;
                }
            } else {
                // CSR instructions
                switch (funct3) {
                case 0b001: d.kind = InstrKind::CSRRW;  d.uses_rs1 = true;  break;
                case 0b010: d.kind = InstrKind::CSRRS;  d.uses_rs1 = true;  break;
                case 0b011: d.kind = InstrKind::CSRRC;  d.uses_rs1 = true;  break;
                case 0b101: d.kind = InstrKind::CSRRWI; d.csr_zimm = rs1;   break;
                case 0b110: d.kind = InstrKind::CSRRSI; d.csr_zimm = rs1;   break;
                case 0b111: d.kind = InstrKind::CSRRCI; d.csr_zimm = rs1;   break;
                default:
                    d.kind = InstrKind::ILLEGAL;
                    break;
                }

                if (d.kind != InstrKind::ILLEGAL) {
                    d.writes_rd = true; // rd gets old CSR value (even if rd==0, WB will ignore)
                }
            }
            break;
        }

        default:
            break;
        }

        return d;
    }
}
