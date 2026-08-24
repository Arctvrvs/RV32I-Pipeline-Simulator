# include <iostream>
# include <string>
# include <vector>
# include <sstream>
# include <iomanip>
# include <fstream>

# include "cpu/cpu_core.h"
# include "cpu/memory.h"
# include "cpu/instr.h"

using namespace std;

// ---------------- DualOut: log to console + file ----------------

struct DualOut {
    std::ostream& console;
    std::ofstream file;

    DualOut(std::ostream& c, const std::string& path)
        : console(c), file(path) {}

    template<typename T>
    DualOut& operator<<(const T& v) {
        console << v;
        if (file.is_open()) file << v;
        return *this;
    }

    // For manipulators like std::endl
    using StreamManip = std::ostream& (*)(std::ostream&);
    DualOut& operator<<(StreamManip m) {
        console << m;
        if (file.is_open()) file << m;
        return *this;
    }

    // For manipulators like std::hex, std::dec
    using IOSManip = std::ios_base& (*)(std::ios_base&);
    DualOut& operator<<(IOSManip m) {
        console << m;
        if (file.is_open()) file << m;
        return *this;
    }
};

// Global pointer used by helpers
static DualOut* g_log = nullptr;

// Convenience macro so we can write LOG << ...
#define LOG (*g_log)

namespace {

    // Simple xN register naming (could be swapped to ABI names)
    string reg_name(uint8_t r) {
        return "x" + to_string(static_cast<int>(r));
    }

    // Turn a DecodedInstr into a human-readable asm string
    string instr_to_asm(const cpu::DecodedInstr& d) {
        using cpu::InstrKind;
        ostringstream oss;

        switch (d.kind) {
        case InstrKind::ECALL:  oss << "ecall"; break;
        case InstrKind::EBREAK: oss << "ebreak"; break;
        case InstrKind::MRET:   oss << "mret"; break;

        case InstrKind::CSRRW:
            oss << "csrrw " << reg_name(d.rd) << ", 0x" << hex << d.csr_addr << dec
                << ", " << reg_name(d.rs1);
            break;
        case InstrKind::CSRRS:
            oss << "csrrs " << reg_name(d.rd) << ", 0x" << hex << d.csr_addr << dec
                << ", " << reg_name(d.rs1);
            break;
        case InstrKind::CSRRC:
            oss << "csrrc " << reg_name(d.rd) << ", 0x" << hex << d.csr_addr << dec
                << ", " << reg_name(d.rs1);
            break;
        case InstrKind::CSRRWI:
            oss << "csrrwi " << reg_name(d.rd) << ", 0x" << hex << d.csr_addr << dec
                << ", " << (d.csr_zimm & 0x1F);
            break;
        case InstrKind::CSRRSI:
            oss << "csrrsi " << reg_name(d.rd) << ", 0x" << hex << d.csr_addr << dec
                << ", " << (d.csr_zimm & 0x1F);
            break;
        case InstrKind::CSRRCI:
            oss << "csrrci " << reg_name(d.rd) << ", 0x" << hex << d.csr_addr << dec
                << ", " << (d.csr_zimm & 0x1F);
            break;

        case InstrKind::ILLEGAL:
            oss << "illegal";
            break;

        case InstrKind::ADD:
            oss << "add  " << reg_name(d.rd)
                << ", "    << reg_name(d.rs1)
                << ", "    << reg_name(d.rs2);
            break;
        case InstrKind::SUB:
            oss << "sub  " << reg_name(d.rd)
                << ", "    << reg_name(d.rs1)
                << ", "    << reg_name(d.rs2);
            break;
        case InstrKind::SLL:
            oss << "sll  " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << reg_name(d.rs2);
            break;
        case InstrKind::SLT:
            oss << "slt  " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << reg_name(d.rs2);
            break;
        case InstrKind::SLTU:
            oss << "sltu " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << reg_name(d.rs2);
            break;
        case InstrKind::XOR:
            oss << "xor  " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << reg_name(d.rs2);
            break;
        case InstrKind::SRL:
            oss << "srl  " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << reg_name(d.rs2);
            break;
        case InstrKind::SRA:
            oss << "sra  " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << reg_name(d.rs2);
            break;
        case InstrKind::OR:
            oss << "or   " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << reg_name(d.rs2);
            break;
        case InstrKind::AND:
            oss << "and  " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << reg_name(d.rs2);
            break;

        case InstrKind::ADDI:
            oss << "addi " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << d.imm;
            break;
        case InstrKind::SLLI:
            oss << "slli " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << (d.imm & 0x1F);
            break;
        case InstrKind::SLTI:
            oss << "slti " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << d.imm;
            break;
        case InstrKind::SLTIU:
            oss << "sltiu " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << d.imm;
            break;
        case InstrKind::XORI:
            oss << "xori " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << d.imm;
            break;
        case InstrKind::SRLI:
            oss << "srli " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << (d.imm & 0x1F);
            break;
        case InstrKind::SRAI:
            oss << "srai " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << (d.imm & 0x1F);
            break;
        case InstrKind::ORI:
            oss << "ori  " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << d.imm;
            break;
        case InstrKind::ANDI:
            oss << "andi " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << d.imm;
            break;

        case InstrKind::LB:
            oss << "lb   " << reg_name(d.rd) << ", " << d.imm << "(" << reg_name(d.rs1) << ")";
            break;
        case InstrKind::LH:
            oss << "lh   " << reg_name(d.rd) << ", " << d.imm << "(" << reg_name(d.rs1) << ")";
            break;
        case InstrKind::LW:
            oss << "lw   " << reg_name(d.rd) << ", " << d.imm << "(" << reg_name(d.rs1) << ")";
            break;
        case InstrKind::LBU:
            oss << "lbu  " << reg_name(d.rd) << ", " << d.imm << "(" << reg_name(d.rs1) << ")";
            break;
        case InstrKind::LHU:
            oss << "lhu  " << reg_name(d.rd) << ", " << d.imm << "(" << reg_name(d.rs1) << ")";
            break;

        case InstrKind::SB:
            oss << "sb   " << reg_name(d.rs2) << ", " << d.imm << "(" << reg_name(d.rs1) << ")";
            break;
        case InstrKind::SH:
            oss << "sh   " << reg_name(d.rs2) << ", " << d.imm << "(" << reg_name(d.rs1) << ")";
            break;
        case InstrKind::SW:
            oss << "sw   " << reg_name(d.rs2) << ", " << d.imm << "(" << reg_name(d.rs1) << ")";
            break;

        case InstrKind::BEQ:
            oss << "beq  " << reg_name(d.rs1)
                << ", "    << reg_name(d.rs2)
                << ", "    << d.imm;
            break;
        case InstrKind::BNE:
            oss << "bne  " << reg_name(d.rs1)
                << ", "    << reg_name(d.rs2)
                << ", "    << d.imm;
            break;
        case InstrKind::BLT:
            oss << "blt  " << reg_name(d.rs1) << ", " << reg_name(d.rs2) << ", " << d.imm;
            break;
        case InstrKind::BGE:
            oss << "bge  " << reg_name(d.rs1) << ", " << reg_name(d.rs2) << ", " << d.imm;
            break;
        case InstrKind::BLTU:
            oss << "bltu " << reg_name(d.rs1) << ", " << reg_name(d.rs2) << ", " << d.imm;
            break;
        case InstrKind::BGEU:
            oss << "bgeu " << reg_name(d.rs1) << ", " << reg_name(d.rs2) << ", " << d.imm;
            break;
        case InstrKind::JAL:
            oss << "jal  " << reg_name(d.rd)
                << ", "    << d.imm;
            break;
        case InstrKind::JALR:
            oss << "jalr " << reg_name(d.rd)
                << ", "    << reg_name(d.rs1)
                << ", "    << d.imm;
            break;
        case InstrKind::LUI:
            oss << "lui  " << reg_name(d.rd)
                << ", "    << d.imm;
            break;
        case InstrKind::AUIPC:
            oss << "auipc " << reg_name(d.rd)
                << ", " << d.imm;
            break;
        case InstrKind::NOP:
        default:
            oss << "nop";
            break;
        }
        return oss.str();
    }

    // Per-cycle pipeline dump (detailed)
    void dump_pipeline(const cpu::CPU& cpu) {
        LOG << "Cycle " << cpu.stats.cycles << "\n";

        // IF: just show the next PC that will be fetched
        LOG << "  IF : pc=0x"
            << std::hex << std::setw(8) << std::setfill('0') << cpu.pc
            << std::dec << "\n";

        // ID stage: IF/ID latch
        LOG << "  ID : ";
        if (cpu.if_id.valid) {
            auto d = cpu::decode(cpu.if_id.instr);
            LOG << "pc=0x" << std::hex << std::setw(8) << std::setfill('0') << cpu.if_id.pc
                << " instr=0x" << std::setw(8) << cpu.if_id.instr
                << std::dec << "  " << instr_to_asm(d) << "\n";
        } else {
            LOG << "(bubble)\n";
        }

        // EX stage: ID/EX latch
        LOG << "  EX : ";
        if (cpu.id_ex.valid) {
            const auto& d = cpu.id_ex.dinstr;
            LOG << "pc=0x" << std::hex << std::setw(8) << std::setfill('0') << cpu.id_ex.pc
                << std::dec << "  " << instr_to_asm(d) << "\n";
        } else {
            LOG << "(bubble)\n";
        }

        // MEM stage: EX/MEM latch
        LOG << "  MEM: ";
        if (cpu.ex_mem.valid) {
            LOG << "alu=0x" << std::hex << std::setw(8) << std::setfill('0')
                << cpu.ex_mem.alu_result << std::dec
                << " rd=" << static_cast<int>(cpu.ex_mem.rd)
                << " mem_read=" << cpu.ex_mem.mem_read
                << " mem_write=" << cpu.ex_mem.mem_write << "\n";
        } else {
            LOG << "(bubble)\n";
        }

        // WB stage: MEM/WB latch
        LOG << "  WB : ";
        if (cpu.mem_wb.valid) {
            LOG << "rd=" << static_cast<int>(cpu.mem_wb.rd)
                << " data=0x" << std::hex << std::setw(8) << std::setfill('0')
                << cpu.mem_wb.wb_data << std::dec << "\n";
        } else {
            LOG << "(bubble)\n";
        }

        LOG << "\n";
    }

    // Compact one-line pipeline summary
    std::string stage_str(const char* name, bool valid, uint32_t pc, uint32_t instr) {
        std::ostringstream oss;
        oss << name << ":";
        if (!valid) {
            oss << "bubble";
        } else {
            auto d = cpu::decode(instr);
            oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << pc
                << std::dec << " " << instr_to_asm(d);
        }
        return oss.str();
    }

    void dump_pipeline_compact(const cpu::CPU& cpu) {
    LOG << "Cycle " << cpu.stats.cycles << " | "
        // IF: just show the fetch PC
        << "IF:0x" << std::hex << std::setw(8) << std::setfill('0') << cpu.pc << std::dec
        // ID: use helper to print pc + asm if valid
        << " | " << stage_str("ID",  cpu.if_id.valid,  cpu.if_id.pc,  cpu.if_id.instr)
        // EX: we *do* have a decoded instr in ID_EX
        << " | EX:";

    if (cpu.id_ex.valid) {
        LOG << instr_to_asm(cpu.id_ex.dinstr);
    } else {
        LOG << "bubble";
    }

    // MEM: EX_MEM has no decoded instr, so show ALU + rd instead
    LOG << " | MEM:";
    if (cpu.ex_mem.valid) {
        LOG << "alu=0x"
            << std::hex << std::setw(8) << std::setfill('0') << cpu.ex_mem.alu_result
            << std::dec << " rd=" << static_cast<int>(cpu.ex_mem.rd);
    } else {
        LOG << "bubble";
    }

    // WB: show dest register if valid
    LOG << " | WB:";
    if (cpu.mem_wb.valid) {
        LOG << "x" << static_cast<int>(cpu.mem_wb.rd);
    } else {
        LOG << "bubble";
    }

    LOG << "\n";
    }


    // Reference model that executes one architectural instruction at a time.
    // Used for per-commit cosimulation against the pipelined core.
    struct RefModel {
        cpu::Memory mem;
        uint32_t regs[32] = {0};
        uint32_t pc = 0;
        uint64_t steps = 0;

        // Minimal CSR state (M-mode only)
        uint32_t csr_mstatus = 0;
        uint32_t csr_mtvec   = 0;
        uint32_t csr_mepc    = 0;
        uint32_t csr_mcause  = 0;

        std::string uart_buffer;

        explicit RefModel(const cpu::Memory& initial_mem, uint32_t entry) : mem(initial_mem) {
            pc = entry;
            for (auto &r : regs) r = 0;
            csr_mstatus = 0;
            csr_mtvec = 0;
            csr_mepc = 0;
            csr_mcause = 0;
            uart_buffer.clear();
        }

        uint32_t read_csr(uint16_t addr) const {
            switch (addr) {
            case cpu::CPU::CSR_MSTATUS: return csr_mstatus;
            case cpu::CPU::CSR_MTVEC:   return csr_mtvec;
            case cpu::CPU::CSR_MEPC:    return csr_mepc;
            case cpu::CPU::CSR_MCAUSE:  return csr_mcause;
            default: return 0;
            }
        }

        void write_csr(uint16_t addr, uint32_t value) {
            switch (addr) {
            case cpu::CPU::CSR_MSTATUS: csr_mstatus = value; break;
            case cpu::CPU::CSR_MTVEC:   csr_mtvec   = value; break;
            case cpu::CPU::CSR_MEPC:    csr_mepc    = value; break;
            case cpu::CPU::CSR_MCAUSE:  csr_mcause  = value; break;
            default: break;
            }
        }

        uint32_t trap_vector() const {
            return csr_mtvec & ~0x3u;
        }

        void enter_trap(uint32_t cause, uint32_t epc) {
            constexpr uint32_t MIE  = (1u << 3);
            constexpr uint32_t MPIE = (1u << 7);
            constexpr uint32_t MPP_SHIFT = 11;
            constexpr uint32_t MPP_MASK  = (3u << MPP_SHIFT);

            uint32_t mie = (csr_mstatus & MIE) ? 1u : 0u;
            csr_mstatus = (csr_mstatus & ~MPIE) | (mie ? MPIE : 0u);
            csr_mstatus &= ~MIE;
            csr_mstatus = (csr_mstatus & ~MPP_MASK) | (3u << MPP_SHIFT);

            csr_mepc = epc;
            csr_mcause = cause;
        }

        uint32_t do_mret() {
            constexpr uint32_t MIE  = (1u << 3);
            constexpr uint32_t MPIE = (1u << 7);
            constexpr uint32_t MPP_SHIFT = 11;
            constexpr uint32_t MPP_MASK  = (3u << MPP_SHIFT);

            uint32_t mpie = (csr_mstatus & MPIE) ? 1u : 0u;
            csr_mstatus = (csr_mstatus & ~MIE) | (mpie ? MIE : 0u);
            csr_mstatus |= MPIE;
            csr_mstatus &= ~MPP_MASK;
            return csr_mepc;
        }

        bool step_one() {
            // Misaligned fetch trap
            if ((pc & 0x3u) != 0u) {
                enter_trap(0u, pc);
                uint32_t vec = trap_vector();
                pc = (vec != 0) ? vec : mem.prog_max;
                regs[0] = 0;
                steps++;
                return true;
            }

            // Fell off program (best-effort)
            if (pc < mem.prog_min || pc >= mem.prog_max) {
                return false;
            }

            uint32_t raw = cpu::load_u32(mem, pc);
            cpu::DecodedInstr d = cpu::decode(raw);

            uint32_t next_pc = pc + 4;
            uint32_t rs1 = (d.uses_rs1 && d.rs1 < 32) ? regs[d.rs1] : 0u;
            uint32_t rs2 = (d.uses_rs2 && d.rs2 < 32) ? regs[d.rs2] : 0u;

            auto zimm = static_cast<uint32_t>(d.csr_zimm & 0x1Fu);

            switch (d.kind) {
            case cpu::InstrKind::ECALL:
                enter_trap(11u, pc);
                next_pc = (trap_vector() != 0) ? trap_vector() : mem.prog_max;
                break;
            case cpu::InstrKind::EBREAK:
                enter_trap(3u, pc);
                next_pc = (trap_vector() != 0) ? trap_vector() : mem.prog_max;
                break;
            case cpu::InstrKind::ILLEGAL:
                enter_trap(2u, pc);
                next_pc = (trap_vector() != 0) ? trap_vector() : mem.prog_max;
                break;
            case cpu::InstrKind::MRET:
                next_pc = do_mret();
                break;

            case cpu::InstrKind::CSRRW:
            case cpu::InstrKind::CSRRS:
            case cpu::InstrKind::CSRRC:
            case cpu::InstrKind::CSRRWI:
            case cpu::InstrKind::CSRRSI:
            case cpu::InstrKind::CSRRCI: {
                uint32_t csr = read_csr(d.csr_addr);
                uint32_t wval = 0;
                bool do_write = true;
                switch (d.kind) {
                case cpu::InstrKind::CSRRW:
                    wval = rs1;
                    do_write = true;
                    break;
                case cpu::InstrKind::CSRRS:
                    wval = csr | rs1;
                    do_write = (d.rs1 != 0);
                    break;
                case cpu::InstrKind::CSRRC:
                    wval = csr & ~rs1;
                    do_write = (d.rs1 != 0);
                    break;
                case cpu::InstrKind::CSRRWI:
                    wval = zimm;
                    do_write = true;
                    break;
                case cpu::InstrKind::CSRRSI:
                    wval = csr | zimm;
                    do_write = (zimm != 0);
                    break;
                case cpu::InstrKind::CSRRCI:
                    wval = csr & ~zimm;
                    do_write = (zimm != 0);
                    break;
                default:
                    do_write = false;
                    wval = csr;
                    break;
                }
                if (do_write) write_csr(d.csr_addr, wval);
                if (d.rd) regs[d.rd] = csr;
                break;
            }

            case cpu::InstrKind::ADD:  if (d.rd) regs[d.rd] = rs1 + rs2; break;
            case cpu::InstrKind::SUB:  if (d.rd) regs[d.rd] = rs1 - rs2; break;
            case cpu::InstrKind::SLL:  if (d.rd) regs[d.rd] = rs1 << (rs2 & 0x1Fu); break;
            case cpu::InstrKind::SLT:  if (d.rd) regs[d.rd] = (static_cast<int32_t>(rs1) < static_cast<int32_t>(rs2)) ? 1u : 0u; break;
            case cpu::InstrKind::SLTU: if (d.rd) regs[d.rd] = (rs1 < rs2) ? 1u : 0u; break;
            case cpu::InstrKind::XOR:  if (d.rd) regs[d.rd] = rs1 ^ rs2; break;
            case cpu::InstrKind::SRL:  if (d.rd) regs[d.rd] = rs1 >> (rs2 & 0x1Fu); break;
            case cpu::InstrKind::SRA:  if (d.rd) regs[d.rd] = static_cast<uint32_t>(static_cast<int32_t>(rs1) >> (rs2 & 0x1Fu)); break;
            case cpu::InstrKind::OR:   if (d.rd) regs[d.rd] = rs1 | rs2; break;
            case cpu::InstrKind::AND:  if (d.rd) regs[d.rd] = rs1 & rs2; break;

            case cpu::InstrKind::ADDI:  if (d.rd) regs[d.rd] = rs1 + static_cast<uint32_t>(d.imm); break;
            case cpu::InstrKind::SLLI:  if (d.rd) regs[d.rd] = rs1 << (static_cast<uint32_t>(d.imm) & 0x1Fu); break;
            case cpu::InstrKind::SLTI:  if (d.rd) regs[d.rd] = (static_cast<int32_t>(rs1) < d.imm) ? 1u : 0u; break;
            case cpu::InstrKind::SLTIU: if (d.rd) regs[d.rd] = (rs1 < static_cast<uint32_t>(d.imm)) ? 1u : 0u; break;
            case cpu::InstrKind::XORI:  if (d.rd) regs[d.rd] = rs1 ^ static_cast<uint32_t>(d.imm); break;
            case cpu::InstrKind::SRLI:  if (d.rd) regs[d.rd] = rs1 >> (static_cast<uint32_t>(d.imm) & 0x1Fu); break;
            case cpu::InstrKind::SRAI:  if (d.rd) regs[d.rd] = static_cast<uint32_t>(static_cast<int32_t>(rs1) >> (static_cast<uint32_t>(d.imm) & 0x1Fu)); break;
            case cpu::InstrKind::ORI:   if (d.rd) regs[d.rd] = rs1 | static_cast<uint32_t>(d.imm); break;
            case cpu::InstrKind::ANDI:  if (d.rd) regs[d.rd] = rs1 & static_cast<uint32_t>(d.imm); break;

            case cpu::InstrKind::LB:
            case cpu::InstrKind::LH:
            case cpu::InstrKind::LW:
            case cpu::InstrKind::LBU:
            case cpu::InstrKind::LHU: {
                uint32_t addr = rs1 + static_cast<uint32_t>(d.imm);
                uint32_t val = (addr == cpu::CPU::UART_BASE) ? 0u : cpu::load_n(mem, addr, d.mem_size, d.mem_signed);
                if (d.rd) regs[d.rd] = val;
                break;
            }

            case cpu::InstrKind::SB:
            case cpu::InstrKind::SH:
            case cpu::InstrKind::SW: {
                uint32_t addr = rs1 + static_cast<uint32_t>(d.imm);
                if (addr == cpu::CPU::UART_BASE) {
                    // Append bytes to reference UART buffer (no stdout printing in reference)
                    for (uint8_t i = 0; i < d.mem_size; ++i) {
                        uart_buffer.push_back(static_cast<char>((rs2 >> (8u * i)) & 0xFFu));
                    }
                } else {
                    cpu::store_n(mem, addr, d.mem_size, rs2);
                }
                break;
            }

            case cpu::InstrKind::BEQ:
                if (rs1 == rs2) next_pc = pc + static_cast<uint32_t>(d.imm);
                break;
            case cpu::InstrKind::BNE:
                if (rs1 != rs2) next_pc = pc + static_cast<uint32_t>(d.imm);
                break;
            case cpu::InstrKind::BLT:
                if (static_cast<int32_t>(rs1) < static_cast<int32_t>(rs2)) next_pc = pc + static_cast<uint32_t>(d.imm);
                break;
            case cpu::InstrKind::BGE:
                if (static_cast<int32_t>(rs1) >= static_cast<int32_t>(rs2)) next_pc = pc + static_cast<uint32_t>(d.imm);
                break;
            case cpu::InstrKind::BLTU:
                if (rs1 < rs2) next_pc = pc + static_cast<uint32_t>(d.imm);
                break;
            case cpu::InstrKind::BGEU:
                if (rs1 >= rs2) next_pc = pc + static_cast<uint32_t>(d.imm);
                break;

            case cpu::InstrKind::JAL:
                if (d.rd) regs[d.rd] = pc + 4;
                next_pc = pc + static_cast<uint32_t>(d.imm);
                break;
            case cpu::InstrKind::JALR:
                if (d.rd) regs[d.rd] = pc + 4;
                next_pc = (rs1 + static_cast<uint32_t>(d.imm)) & ~1u;
                break;

            case cpu::InstrKind::LUI:
                if (d.rd) regs[d.rd] = static_cast<uint32_t>(d.imm);
                break;
            case cpu::InstrKind::AUIPC:
                if (d.rd) regs[d.rd] = pc + static_cast<uint32_t>(d.imm);
                break;

            case cpu::InstrKind::NOP:
            default:
                break;
            }

            pc = next_pc;
            regs[0] = 0;
            steps++;
            return true;
        }
    };

    // Per-commit cosimulation check. Returns true if still consistent.
    bool cosim_check_commit(const cpu::CPU& cpu, RefModel& ref) {
        const auto& c = cpu.last_commit;
        if (!c.valid) return true; // no commit this cycle

        // The committed instruction's PC should match the reference PC.
        if (ref.pc != c.pc) {
            auto d = cpu::decode(c.raw);
            LOG << "COSIM FAIL @ cycle " << cpu.stats.cycles
                << " (commit id=" << c.instr_id << ")\n";
            LOG << "  PC mismatch: pipeline committed pc=0x" << std::hex << c.pc
                << " but reference pc=0x" << ref.pc << std::dec << "\n";
            LOG << "  Committed: instr=0x" << std::hex << c.raw << std::dec
                << "  " << instr_to_asm(d) << "\n";
            return false;
        }

        // Step the reference by one architectural instruction.
        if (!ref.step_one()) {
            auto d = cpu::decode(c.raw);
            LOG << "COSIM FAIL @ cycle " << cpu.stats.cycles
                << " (commit id=" << c.instr_id << ")\n";
            LOG << "  Reference fell off program while pipeline still commits.\n";
            LOG << "  Committed: pc=0x" << std::hex << c.pc
                << " instr=0x" << c.raw << std::dec
                << "  " << instr_to_asm(d) << "\n";
            return false;
        }

        // Compare architectural registers.
        for (int i = 0; i < 32; ++i) {
            if (cpu.regs[i] != ref.regs[i]) {
                auto d = cpu::decode(c.raw);
                LOG << "COSIM FAIL @ cycle " << cpu.stats.cycles
                    << " (commit id=" << c.instr_id << ")\n";
                LOG << "  Committed: pc=0x" << std::hex << c.pc
                    << " instr=0x" << c.raw << std::dec
                    << "  " << instr_to_asm(d) << "\n";
                LOG << "  Reg mismatch x" << i
                    << ": pipeline=0x" << std::hex << cpu.regs[i]
                    << " ref=0x" << ref.regs[i] << std::dec << "\n";

                // Provide a small context dump.
                LOG << "  Context (x0-x7):\n";
                for (int r = 0; r < 8; ++r) {
                    LOG << "    x" << r
                        << " pipe=0x" << std::hex << cpu.regs[r]
                        << " ref=0x" << ref.regs[r] << std::dec << "\n";
                }
                return false;
            }
        }

        // Compare minimal CSRs (helps catch trap/CSR issues early).
        if (cpu.csr_mstatus != ref.csr_mstatus ||
            cpu.csr_mtvec   != ref.csr_mtvec   ||
            cpu.csr_mepc    != ref.csr_mepc    ||
            cpu.csr_mcause  != ref.csr_mcause) {
            auto d = cpu::decode(c.raw);
            LOG << "COSIM FAIL @ cycle " << cpu.stats.cycles
                << " (commit id=" << c.instr_id << ")\n";
            LOG << "  Committed: pc=0x" << std::hex << c.pc
                << " instr=0x" << c.raw << std::dec
                << "  " << instr_to_asm(d) << "\n";
            LOG << "  CSR mismatch:\n";
            LOG << "    mstatus pipe=0x" << std::hex << cpu.csr_mstatus << " ref=0x" << ref.csr_mstatus << std::dec << "\n";
            LOG << "    mtvec   pipe=0x" << std::hex << cpu.csr_mtvec   << " ref=0x" << ref.csr_mtvec   << std::dec << "\n";
            LOG << "    mepc    pipe=0x" << std::hex << cpu.csr_mepc    << " ref=0x" << ref.csr_mepc    << std::dec << "\n";
            LOG << "    mcause  pipe=0x" << std::hex << cpu.csr_mcause  << " ref=0x" << ref.csr_mcause  << std::dec << "\n";
            return false;
        }

                // For stores, also check the bytes written to memory (fast targeted check).
        if (c.mem_is_store) {
            // Skip UART MMIO stores (no backing memory, side-effect is console output).
            if (c.mem_addr == cpu::CPU::UART_BASE) {
                return true;
            }
            const uint8_t sz = c.mem_size ? c.mem_size : 4;
            uint32_t p = cpu::load_n(cpu.mem, c.mem_addr, sz, false);
            uint32_t r = cpu::load_n(ref.mem, c.mem_addr, sz, false);

            uint32_t mask = (sz == 1) ? 0xFFu : (sz == 2) ? 0xFFFFu : 0xFFFFFFFFu;
            p &= mask;
            r &= mask;

            if (p != r) {
                auto d = cpu::decode(c.raw);
                LOG << "COSIM FAIL @ cycle " << cpu.stats.cycles
                    << " (commit id=" << c.instr_id << ")\n";
                LOG << "  Committed: pc=0x" << std::hex << c.pc
                    << " instr=0x" << c.raw << std::dec
                    << "  " << instr_to_asm(d) << "\n";
                LOG << "  Store mem mismatch @ addr=0x" << std::hex << c.mem_addr << std::dec
                    << " (size=" << static_cast<int>(sz) << "): pipeline=0x" << std::hex << p
                    << " ref=0x" << r << std::dec << "\n";
                LOG << "  Store data (pipeline view): 0x" << std::hex << c.mem_store_data
                    << std::dec << "\n";
                return false;
            }
        }
        return true;
    }

    bool check_memory(const cpu::Memory& pipe_mem,
                      const cpu::Memory& ref_mem)
{
    std::size_t size = std::min(pipe_mem.dmem.size(), ref_mem.dmem.size());
    std::size_t mismatches = 0;
    const std::size_t max_report = 32; // limit printed lines

    for (std::size_t idx = 0; idx < size; ++idx) {
        uint8_t p = pipe_mem.dmem[idx];
        uint8_t r = ref_mem.dmem[idx];
        if (p != r) {
            if (mismatches < max_report) {
                uint32_t addr = static_cast<uint32_t>(idx);
                LOG << "Mem mismatch at byte addr 0x" << std::hex << addr << std::dec
                    << ": pipeline=0x" << std::hex << static_cast<unsigned>(p)
                    << " ref=0x" << static_cast<unsigned>(r) << std::dec << "\n";
            }
                ++mismatches;
            }
        }

        if (pipe_mem.dmem.size() != ref_mem.dmem.size()) {
            LOG << "Warning: pipeline dmem size (" << pipe_mem.dmem.size()
                << ") != reference dmem size (" << ref_mem.dmem.size() << ")\n";
        }

        if (mismatches == 0) {
            LOG << "Data memory check: PASS (all bytes match)\n";
        } else {
            LOG << "Data memory check: FAIL (" << mismatches
                << " mismatching byte(s); first " << max_report
                << " shown above)\n";
        }
        return mismatches == 0 && pipe_mem.dmem.size() == ref_mem.dmem.size();
    }

} // anonymous namespace

// ---------------- Debugger ----------------

struct Debugger {
    cpu::CPU& cpu;
    uint64_t max_cycles;
    uint32_t breakpoint_pc = 0;
    bool     has_breakpoint = false;

    int      last_written_rd = -1; // highlight last WB dest

    explicit Debugger(cpu::CPU& c, uint64_t max_c = 100000)
        : cpu(c), max_cycles(max_c) {}

    void print_help() {
        LOG << "Commands:\n"
            << "  h, help             Show this help\n"
            << "  s [n]               Step n cycles (default 1)\n"
            << "  si [n]              Step n committed instructions (default 1)\n"
            << "  c                   Continue until halt or breakpoint\n"
            << "  b <pc>              Set breakpoint at byte PC (hex or dec)\n"
            << "  bc                  Clear breakpoint\n"
            << "  regs                Show registers\n"
            << "  pc                  Show current PC\n"
            << "  p, pipeline         Show pipeline state for this cycle\n"
            << "  x <addr> [n]        Examine n data words at addr (default n=8)\n"
            << "  q                   Quit debugger\n";
    }

    void print_regs() {
        LOG << "Registers:\n";
        for (int i = 0; i < 32; ++i) {
            if (i % 4 == 0) LOG << "  ";
            LOG << "x" << setw(2) << setfill(' ') << i << "="
                << "0x" << hex << setw(8) << setfill('0') << cpu.regs[i] << "  "
                << dec;
            if (i % 4 == 3) LOG << "\n";
        }
        LOG << "\n";
    }

    void print_pc() {
        LOG << "PC=0x" << hex << setw(8) << setfill('0') << cpu.pc << dec << "\n";
    }

    void examine_mem(uint32_t addr, int n) {
        LOG << "Data memory from addr 0x"
            << hex << setw(8) << setfill('0') << addr << dec << ":\n";
        for (int i = 0; i < n; ++i) {
            uint32_t a = addr + static_cast<uint32_t>(i) * 4u;
            if (!cpu.mem.is_mapped(a, 4)) {
                LOG << "  0x" << hex << setw(8) << setfill('0') << a
                    << dec << " : <unmapped>\n";
                continue;
            }
            uint32_t val = cpu::load_u32(cpu.mem, a);
            LOG << "  0x" << hex << setw(8) << setfill('0') << a
                << " : 0x" << setw(8) << val << dec << "\n";
        }
        LOG << "\n";
    }

bool check_breakpoint() {
        if (has_breakpoint && cpu.pc == breakpoint_pc) {
            LOG << "** Breakpoint hit at PC=0x"
                << hex << setw(8) << setfill('0') << cpu.pc << dec << "\n";
            return true;
        }
        return false;
    }

    void step_cycles(uint64_t n) {
        for (uint64_t i = 0; i < n; ++i) {
            if (cpu.halted || cpu.stats.cycles >= max_cycles) break;

            // record last-written register for this cycle (WB stage)
            last_written_rd = cpu.mem_wb.valid ? cpu.mem_wb.rd : -1;

            dump_pipeline(cpu);
            cpu.step();
            if (check_breakpoint()) break;
        }
    }

    void step_instructions(uint64_t n) {
        uint64_t target = cpu.stats.committed_instructions + n;
        while (!cpu.halted &&
               cpu.stats.cycles < max_cycles &&
               cpu.stats.committed_instructions < target) {

            last_written_rd = cpu.mem_wb.valid ? cpu.mem_wb.rd : -1;

            dump_pipeline(cpu);
            cpu.step();
            if (check_breakpoint()) break;
        }
    }

    void render_ui() {
        // ANSI clear screen + move cursor home
        std::cout << "\033[2J\033[H";

        const char* red   = "\033[31m";
        const char* green = "\033[32m";
        const char* reset = "\033[0m";

        // Header
        LOG << "== RV32I CPU Simulator ==\n";
        LOG << "Cycles: " << cpu.stats.cycles
            << "   Committed: " << cpu.stats.committed_instructions
            << "   Stalls: " << cpu.stats.stall_cycles
            << "   Halted: "
            << (cpu.halted ? red : green)
            << (cpu.halted ? "yes" : "no")
            << reset << "\n";
        LOG << "PC: 0x" << std::hex << std::setw(8) << std::setfill('0')
            << cpu.pc << std::dec << "\n\n";

        // Registers (left)
        LOG << "[Registers]\n";
        for (int i = 0; i < 32; ++i) {
            if (i % 4 == 0) LOG << "  ";

            bool is_last = (i == last_written_rd);

            if (is_last) LOG << "*";
            else         LOG << " ";

            LOG << "x" << std::setw(2) << std::setfill(' ') << i
                << "=0x" << std::hex << std::setw(8) << std::setfill('0')
                << cpu.regs[i] << std::dec << "  ";
            if (i % 4 == 3) LOG << "\n";
        }
        LOG << "\n";

        // Pipeline
        LOG << "[Pipeline - compact]\n";
        dump_pipeline_compact(cpu);
        LOG << "\n";

        LOG << "[Pipeline - detailed]\n";
        dump_pipeline(cpu);

        // Branch predictor stats
        LOG << "[Branch predictor]\n";
        LOG << "  Branches:    " << cpu.stats.branch_instructions   << "\n";
        LOG << "  Predictions: " << cpu.stats.branch_predictions    << "\n";
        LOG << "  Mispredicts: " << cpu.stats.branch_mispredictions << "\n";
        double mis_rate = cpu.stats.branch_predictions == 0
            ? 0.0
            : static_cast<double>(cpu.stats.branch_mispredictions) /
              static_cast<double>(cpu.stats.branch_predictions);
        LOG << "  Miss rate:   " << mis_rate << "\n\n";

        // Simple data memory window at base address 0
        uint32_t base_addr = 0;
        LOG << "[Data memory @ 0x" << std::hex << std::setw(8) << std::setfill('0')
            << base_addr << std::dec << "]\n";

        for (int row = 0; row < 8; ++row) {
            uint32_t addr = base_addr + row * 16; // 16 bytes per row
            LOG << "  0x" << std::hex << std::setw(8) << std::setfill('0') << addr << ":";
                        for (int w = 0; w < 4; ++w) {         // 4 words per row
                uint32_t a = addr + static_cast<uint32_t>(w) * 4u;
                if (a < cpu.mem.dmem.size()) {
                    uint32_t val = cpu::load_u32(cpu.mem, a);
                    LOG << "  " << std::setw(8) << val;
                } else {
                    LOG << "  ........";
                }
            }
            LOG << std::dec << "\n";

        }
        LOG << "\n";

        LOG << "[Commands] s/si=step  c=continue  q=quit  (press Enter after command)\n";
    }

    void run_ui_mode() {
        // redraw on every cycle, waiting for commands
        while (true) {
            render_ui();

            std::string line;
            LOG << "(ui) " << std::flush;
            if (!std::getline(std::cin, line)) break;
            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;
            if (cmd.empty()) continue;

            if (cmd == "q") {
                break;
            } else if (cmd == "s") {
                uint64_t n = 1;
                iss >> n;
                step_cycles(n);
            } else if (cmd == "si") {
                uint64_t n = 1;
                iss >> n;
                step_instructions(n);
            } else if (cmd == "c") {
                // continue until halt/breakpoint, but refresh occasionally
                while (!cpu.halted && cpu.stats.cycles < max_cycles) {
                    last_written_rd = cpu.mem_wb.valid ? cpu.mem_wb.rd : -1;
                    cpu.step();
                    if (check_breakpoint()) break;
                    // small auto-refresh every N cycles
                    if (cpu.stats.cycles % 10 == 0) {
                        render_ui();
                    }
                }
            } else {
                LOG << "Unknown cmd in UI mode. Use s/si/c/q.\n";
            }

            if (cpu.halted) {
                render_ui();
                LOG << "** CPU halted after " << cpu.stats.cycles << " cycles\n";
            }
        }
    }

    void run() {
        print_help();
        std::string line;
        while (true) {
            LOG << "(cpu-sim) " << std::flush;
            if (!std::getline(std::cin, line)) break;
            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;
            if (cmd.empty()) continue;

            if (cmd == "h" || cmd == "help") {
                print_help();
            } else if (cmd == "q") {
                break;
            } else if (cmd == "regs") {
                print_regs();
            } else if (cmd == "pc") {
                print_pc();
            } else if (cmd == "p" || cmd == "pipeline") {
                dump_pipeline(cpu);
            } else if (cmd == "s") {
                uint64_t n = 1;
                iss >> n;
                step_cycles(n);
            } else if (cmd == "si") {
                uint64_t n = 1;
                iss >> n;
                step_instructions(n);
            } else if (cmd == "c") {
                while (!cpu.halted && cpu.stats.cycles < max_cycles) {
                    dump_pipeline(cpu);
                    cpu.step();
                    if (check_breakpoint()) break;
                }
            } else if (cmd == "b") {
                std::string pc_str;
                if (!(iss >> pc_str)) {
                    LOG << "Usage: b <pc>\n";
                    continue;
                }
                uint32_t pc = 0;
                if (pc_str.rfind("0x", 0) == 0 || pc_str.rfind("0X", 0) == 0) {
                    pc = static_cast<uint32_t>(std::stoul(pc_str, nullptr, 16));
                } else {
                    pc = static_cast<uint32_t>(std::stoul(pc_str, nullptr, 10));
                }
                breakpoint_pc = pc;
                has_breakpoint = true;
                LOG << "Breakpoint set at PC=0x"
                    << hex << setw(8) << setfill('0') << pc << dec << "\n";
            } else if (cmd == "bc") {
                has_breakpoint = false;
                LOG << "Breakpoint cleared.\n";
            } else if (cmd == "x") {
                std::string addr_str;
                int n = 8;
                if (!(iss >> addr_str)) {
                    LOG << "Usage: x <addr> [n]\n";
                    continue;
                }
                iss >> n;
                uint32_t addr = 0;
                if (addr_str.rfind("0x", 0) == 0 || addr_str.rfind("0X", 0) == 0) {
                    addr = static_cast<uint32_t>(std::stoul(addr_str, nullptr, 16));
                } else {
                    addr = static_cast<uint32_t>(std::stoul(addr_str, nullptr, 10));
                }
                examine_mem(addr, n <= 0 ? 8 : n);
            } else {
                LOG << "Unknown command: " << cmd << " (type 'help')\n";
            }

            if (cpu.halted) {
                LOG << "** CPU halted after " << cpu.stats.cycles << " cycles\n";
            }
        }
    }
};

int main (int argc, char **argv){
    // Mirror CLI output to a local, ignored log file for debugging.
    DualOut logger(std::cout, "console_output.txt");
    g_log = &logger;

    if (argc < 2){
        LOG << "Usage: rv32-sim [--trace] [--gdb] [--ui] program.(hex|elf) [max_cycles]\n";
        return 1;
    }

    bool trace    = false;
    bool gdb_mode = false;
    bool ui_mode  = false;
    vector<string> positional;

    // Parse flags + positional args
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--trace" || arg == "-t") {
            trace = true;
        } else if (arg == "--gdb" || arg == "--interactive") {
            gdb_mode = true;
        } else if (arg == "--ui") {
            ui_mode = true;
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.empty()) {
        LOG << "Usage: rv32-sim [--trace] [--gdb] [--ui] program.(hex|elf) [max_cycles]\n";
        return 1;
    }

    string   program_path = positional[0];
    uint64_t max_cycles   = 100000;
    if (positional.size() >= 2) {
        max_cycles = stoull(positional[1]);
    }

    cpu::CPU cpu;
    uint32_t entry = 0;
    if (!cpu::load_program(program_path, cpu.mem, entry, nullptr)){
        LOG << "Failed to load program: " << program_path << "\n";
        return 1;
    }

    // ---- UI mode (new) ----
    if (ui_mode) {
        cpu.pc = entry;
        Debugger dbg(cpu, max_cycles);
        dbg.run_ui_mode();
        return 0;   // skip normal batch run + stats comparison
    }

    // ---- Interactive debugger (gdb-like) ----
    if (gdb_mode) {
        cpu.pc = entry;
        Debugger dbg(cpu, max_cycles);
        dbg.run();
        return 0;   // skip normal batch run + stats comparison
    }
    // ----------------------------------------

    // Reference model for per-commit cosimulation
    RefModel ref(cpu.mem, entry);

    // Run pipelined model (batch mode)
    cpu.pc = entry;
    bool cosim_ok = true;
    while (!cpu.halted && cpu.stats.cycles < max_cycles){
        if (trace) {
            dump_pipeline(cpu);
        }
        cpu.step();

        if (!cosim_check_commit(cpu, ref)) {
            cosim_ok = false;
            break;
        }
    }

    double cpi = cpu.stats.committed_instructions == 0
            ? 0.0
            : static_cast<double>(cpu.stats.cycles) /
              static_cast<double>(cpu.stats.committed_instructions);
    LOG << "Cycles:       " << cpu.stats.cycles << "\n";
    LOG << "Committed:    " << cpu.stats.committed_instructions << "\n";
    LOG << "Stall cycles: " << cpu.stats.stall_cycles << "\n";
    LOG << "CPI:          " << cpi << "\n";
    LOG << "x10 (a0) = 0x" << std::hex << cpu.regs[10] << std::dec << "\n";

    LOG << "Branches:     " << cpu.stats.branch_instructions   << "\n";
    LOG << "Predictions:  " << cpu.stats.branch_predictions    << "\n";
    LOG << "Mispredicts:  " << cpu.stats.branch_mispredictions << "\n";

    double mis_rate = cpu.stats.branch_predictions == 0
        ? 0.0
        : static_cast<double>(cpu.stats.branch_mispredictions) /
          static_cast<double>(cpu.stats.branch_predictions);

    LOG << "Mispred rate: " << mis_rate << "\n";

    // Final checks / summary
    if (cosim_ok) {
        LOG << "Per-commit cosim: PASS\n";
    } else {
        LOG << "Per-commit cosim: FAIL (see first mismatch above)\n";
    }

    bool registers_ok = true;
    for (int i = 0; i < 32; ++i) {
        if (cpu.regs[i] != ref.regs[i]) {
            LOG << "Final reg mismatch in x" << i
                << ": pipeline=0x" << std::hex << cpu.regs[i]
                << " ref=0x" << ref.regs[i] << std::dec << "\n";
            registers_ok = false;
        }
    }
    if (registers_ok) {
        LOG << "Final register check: PASS\n";
    } else {
        LOG << "Final register check: FAIL\n";
    }

    const bool memory_ok = check_memory(cpu.mem, ref.mem);

    return (cosim_ok && registers_ok && memory_ok) ? 0 : 2;
}
