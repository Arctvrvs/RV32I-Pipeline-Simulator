#include "cpu/stages.h"
#include "cpu/instr.h"
#include "cpu/memory.h"

#include <iostream>

namespace cpu {

// ---------------- WB ----------------

void do_wb(CPU& cpu)
{
    // Publish commit record for cosimulation/debugging.
    cpu.last_commit = {};
    cpu.last_commit.valid = cpu.mem_wb.valid;

    if (!cpu.mem_wb.valid) return;

    cpu.last_commit.instr_id   = cpu.mem_wb.instr_id;
    cpu.last_commit.pc         = cpu.mem_wb.pc;
    cpu.last_commit.raw        = cpu.mem_wb.raw;
    cpu.last_commit.reg_write  = cpu.mem_wb.reg_write;
    cpu.last_commit.rd         = cpu.mem_wb.rd;
    cpu.last_commit.wb_data    = cpu.mem_wb.wb_data;

    cpu.last_commit.mem_is_load     = cpu.mem_wb.mem_is_load;
    cpu.last_commit.mem_is_store    = cpu.mem_wb.mem_is_store;
    cpu.last_commit.mem_addr        = cpu.mem_wb.mem_addr;
    cpu.last_commit.mem_store_data  = cpu.mem_wb.mem_store_data;
    cpu.last_commit.mem_size        = cpu.mem_wb.mem_size;
    cpu.last_commit.mem_signed      = cpu.mem_wb.mem_signed;

    if (cpu.mem_wb.reg_write && cpu.mem_wb.rd != 0) {
        cpu.regs[cpu.mem_wb.rd] = cpu.mem_wb.wb_data;
    }

    // Count every valid instruction reaching WB as "committed"
    cpu.stats.committed_instructions++;
}

// ---------------- MEM ----------------

static inline void uart_write(CPU& cpu, uint32_t value, uint8_t size_bytes) {
    // Write low bytes (little-endian order)
    for (uint8_t i = 0; i < size_bytes; ++i) {
        uint8_t ch = static_cast<uint8_t>((value >> (8u * i)) & 0xFFu);
        cpu.uart_buffer.push_back(static_cast<char>(ch));
        std::cout.put(static_cast<char>(ch));
    }
    std::cout.flush();
}

void do_mem(CPU& cpu, MEM_WB& next_mem_wb)
{
    // Default bubble
    next_mem_wb = {};

    if (!cpu.ex_mem.valid) return;

    uint32_t addr = cpu.ex_mem.alu_result;
    uint32_t loaded = 0;

    const bool is_uart = (addr == CPU::UART_BASE);

    if (cpu.ex_mem.mem_read) {
        // Optional: reads from UART return 0
        if (!is_uart) {
            loaded = load_n(cpu.mem, addr, cpu.ex_mem.mem_size, cpu.ex_mem.mem_signed);
        } else {
            loaded = 0;
        }
    }

    if (cpu.ex_mem.mem_write) {
        if (is_uart) {
            uart_write(cpu, cpu.ex_mem.store_data, cpu.ex_mem.mem_size);
        } else {
            store_n(cpu.mem, addr, cpu.ex_mem.mem_size, cpu.ex_mem.store_data);
        }
    }

    next_mem_wb.valid     = cpu.ex_mem.valid;
    next_mem_wb.instr_id  = cpu.ex_mem.instr_id;
    next_mem_wb.pc        = cpu.ex_mem.pc;
    next_mem_wb.raw       = cpu.ex_mem.raw;

    // Carry memory access info forward for per-commit checking.
    next_mem_wb.mem_is_load      = cpu.ex_mem.mem_read;
    next_mem_wb.mem_is_store     = cpu.ex_mem.mem_write;
    next_mem_wb.mem_addr         = (cpu.ex_mem.mem_read || cpu.ex_mem.mem_write) ? addr : 0u;
    next_mem_wb.mem_store_data   = cpu.ex_mem.mem_write ? cpu.ex_mem.store_data : 0u;
    next_mem_wb.mem_size         = (cpu.ex_mem.mem_read || cpu.ex_mem.mem_write) ? cpu.ex_mem.mem_size : 0u;
    next_mem_wb.mem_signed       = cpu.ex_mem.mem_signed;

    next_mem_wb.rd        = cpu.ex_mem.rd;
    next_mem_wb.reg_write = cpu.ex_mem.reg_write;

    if (cpu.ex_mem.mem_to_reg) {
        next_mem_wb.wb_data = loaded;                // load result
    } else {
        next_mem_wb.wb_data = cpu.ex_mem.alu_result; // ALU / JAL / LUI / AUIPC / CSR read, etc.
    }
}

// ---------------- EX ----------------

static inline uint32_t zext5(uint32_t x) { return x & 0x1Fu; }

void do_ex(CPU& cpu, EX_MEM& next_ex_mem,
           bool& redirect, uint32_t& redirect_target)
{
    // Default bubble + no redirect
    next_ex_mem      = {};
    redirect         = false;
    redirect_target  = 0;

    if (!cpu.id_ex.valid) return;

    const DecodedInstr& d = cpu.id_ex.dinstr;

    uint32_t op1 = d.uses_rs1 ? cpu.id_ex.rs1_val : 0u;
    uint32_t op2 = d.uses_rs2 ? cpu.id_ex.rs2_val : 0u;

    // ---- Forwarding logic ----
    // Apply the older MEM/WB value first, then let the newer EX/MEM result win
    // when both stages target the same source register.
    if (cpu.mem_wb.valid && cpu.mem_wb.reg_write &&
        cpu.mem_wb.rd != 0) {
        if (d.uses_rs1 && cpu.mem_wb.rd == d.rs1) op1 = cpu.mem_wb.wb_data;
        if (d.uses_rs2 && cpu.mem_wb.rd == d.rs2) op2 = cpu.mem_wb.wb_data;
    }

    // EX/MEM forwarding is valid only for results available in EX (not loads).
    if (cpu.ex_mem.valid && cpu.ex_mem.reg_write &&
        cpu.ex_mem.rd != 0 && !cpu.ex_mem.mem_to_reg) {
        if (d.uses_rs1 && cpu.ex_mem.rd == d.rs1) op1 = cpu.ex_mem.alu_result;
        if (d.uses_rs2 && cpu.ex_mem.rd == d.rs2) op2 = cpu.ex_mem.alu_result;
    }

    uint32_t alu_result = 0;

    auto shamt_reg = static_cast<uint32_t>(op2 & 0x1Fu);
    auto shamt_imm = static_cast<uint32_t>(static_cast<uint32_t>(d.imm) & 0x1Fu);

    // ---- Execute ----
    switch (d.kind) {
    // System / CSR
    case InstrKind::ECALL: {
        // Environment call from M-mode => cause 11
        cpu.enter_trap(11u, cpu.id_ex.pc);
        redirect = true;
        redirect_target = cpu.trap_vector();
        if (redirect_target == 0) redirect_target = cpu.mem.prog_max; // deterministic stop if no handler
        break;
    }
    case InstrKind::EBREAK: {
        cpu.enter_trap(3u, cpu.id_ex.pc);
        redirect = true;
        redirect_target = cpu.trap_vector();
        if (redirect_target == 0) redirect_target = cpu.mem.prog_max;
        break;
    }
    case InstrKind::ILLEGAL: {
        cpu.enter_trap(2u, cpu.id_ex.pc);
        redirect = true;
        redirect_target = cpu.trap_vector();
        if (redirect_target == 0) redirect_target = cpu.mem.prog_max;
        break;
    }
    case InstrKind::MRET: {
        uint32_t new_pc = cpu.do_mret();
        redirect = true;
        redirect_target = new_pc;
        break;
    }

    case InstrKind::CSRRW:
    case InstrKind::CSRRS:
    case InstrKind::CSRRC:
    case InstrKind::CSRRWI:
    case InstrKind::CSRRSI:
    case InstrKind::CSRRCI: {
        uint32_t csr = cpu.read_csr(d.csr_addr);
        uint32_t wval = 0;
        bool do_write = true;

        switch (d.kind) {
        case InstrKind::CSRRW:
            wval = op1;
            do_write = true;
            break;
        case InstrKind::CSRRS:
            wval = csr | op1;
            do_write = (d.rs1 != 0);
            break;
        case InstrKind::CSRRC:
            wval = csr & ~op1;
            do_write = (d.rs1 != 0);
            break;
        case InstrKind::CSRRWI:
            wval = zext5(d.csr_zimm);
            do_write = true;
            break;
        case InstrKind::CSRRSI:
            wval = csr | zext5(d.csr_zimm);
            do_write = (zext5(d.csr_zimm) != 0);
            break;
        case InstrKind::CSRRCI:
            wval = csr & ~zext5(d.csr_zimm);
            do_write = (zext5(d.csr_zimm) != 0);
            break;
        default:
            do_write = false;
            wval = csr;
            break;
        }

        if (do_write) {
            cpu.write_csr(d.csr_addr, wval);
        }

        alu_result = csr; // old value to rd
        break;
    }

    // R-type
    case InstrKind::ADD:  alu_result = op1 + op2; break;
    case InstrKind::SUB:  alu_result = op1 - op2; break;
    case InstrKind::SLL:  alu_result = op1 << shamt_reg; break;
    case InstrKind::SLT:  alu_result = (static_cast<int32_t>(op1) < static_cast<int32_t>(op2)) ? 1u : 0u; break;
    case InstrKind::SLTU: alu_result = (op1 < op2) ? 1u : 0u; break;
    case InstrKind::XOR:  alu_result = op1 ^ op2; break;
    case InstrKind::SRL:  alu_result = op1 >> shamt_reg; break;
    case InstrKind::SRA:  alu_result = static_cast<uint32_t>(static_cast<int32_t>(op1) >> shamt_reg); break;
    case InstrKind::OR:   alu_result = op1 | op2; break;
    case InstrKind::AND:  alu_result = op1 & op2; break;

    // I-type ALU
    case InstrKind::ADDI:  alu_result = op1 + static_cast<uint32_t>(d.imm); break;
    case InstrKind::SLLI:  alu_result = op1 << shamt_imm; break;
    case InstrKind::SLTI:  alu_result = (static_cast<int32_t>(op1) < d.imm) ? 1u : 0u; break;
    case InstrKind::SLTIU: alu_result = (op1 < static_cast<uint32_t>(d.imm)) ? 1u : 0u; break;
    case InstrKind::XORI:  alu_result = op1 ^ static_cast<uint32_t>(d.imm); break;
    case InstrKind::SRLI:  alu_result = op1 >> shamt_imm; break;
    case InstrKind::SRAI:  alu_result = static_cast<uint32_t>(static_cast<int32_t>(op1) >> shamt_imm); break;
    case InstrKind::ORI:   alu_result = op1 | static_cast<uint32_t>(d.imm); break;
    case InstrKind::ANDI:  alu_result = op1 & static_cast<uint32_t>(d.imm); break;

    // Loads / Stores: alu_result is effective address
    case InstrKind::LB:
    case InstrKind::LH:
    case InstrKind::LW:
    case InstrKind::LBU:
    case InstrKind::LHU:
    case InstrKind::SB:
    case InstrKind::SH:
    case InstrKind::SW:
        alu_result = op1 + static_cast<uint32_t>(d.imm);
        break;

    // Branches handled below
    case InstrKind::BEQ:
    case InstrKind::BNE:
    case InstrKind::BLT:
    case InstrKind::BGE:
    case InstrKind::BLTU:
    case InstrKind::BGEU:
        break;

    // Jumps
    case InstrKind::JAL:
        alu_result   = cpu.id_ex.pc + 4;
        redirect = true;
        redirect_target = cpu.id_ex.pc + static_cast<uint32_t>(d.imm);
        break;

    case InstrKind::JALR:
        alu_result   = cpu.id_ex.pc + 4;
        redirect = true;
        redirect_target = (op1 + static_cast<uint32_t>(d.imm)) & ~1u;
        break;

    // Upper immediates
    case InstrKind::LUI:
        alu_result = static_cast<uint32_t>(d.imm);
        break;

    case InstrKind::AUIPC:
        alu_result = cpu.id_ex.pc + static_cast<uint32_t>(d.imm);
        break;

    case InstrKind::NOP:
    default:
        break;
    }

    // ---- Branch predictor integration for conditional branches only ----
    if (d.is_branch) {
        cpu.stats.branch_instructions++;

        bool actual_taken = false;
        switch (d.kind) {
        case InstrKind::BEQ:  actual_taken = (op1 == op2); break;
        case InstrKind::BNE:  actual_taken = (op1 != op2); break;
        case InstrKind::BLT:  actual_taken = (static_cast<int32_t>(op1) < static_cast<int32_t>(op2)); break;
        case InstrKind::BGE:  actual_taken = (static_cast<int32_t>(op1) >= static_cast<int32_t>(op2)); break;
        case InstrKind::BLTU: actual_taken = (op1 < op2); break;
        case InstrKind::BGEU: actual_taken = (op1 >= op2); break;
        default: actual_taken = false; break;
        }

        uint32_t fall_through = cpu.id_ex.pc + 4;
        uint32_t target       = cpu.id_ex.pc + static_cast<uint32_t>(d.imm);

        uint32_t bp_index         = cpu.id_ex.bp_index;
        bool     predicted_taken  = cpu.id_ex.predicted_taken;
        uint32_t predicted_target = cpu.id_ex.predicted_target;

        cpu.stats.branch_predictions++;

        bool mispredict = false;
        if (predicted_taken) {
            mispredict = (!actual_taken) || (predicted_target != target);
        } else {
            mispredict = actual_taken;
        }

        if (mispredict) {
            cpu.stats.branch_mispredictions++;
            redirect = true;
            redirect_target = actual_taken ? target : fall_through;
        }

        // Update 2-bit saturating counter
        uint8_t &ctr = cpu.bpred_counter[bp_index];
        if (actual_taken) {
            if (ctr < 3) ++ctr;
        } else {
            if (ctr > 0) --ctr;
        }

        // Update tagged BTB entry (store the architectural target)
        const uint32_t pc_tag = cpu.id_ex.pc >> 10; // 2 (word) + 8 (index)
        cpu.bpred_valid[bp_index]     = true;
        cpu.bpred_is_branch[bp_index] = true;
        cpu.bpred_tag[bp_index]       = pc_tag;
        cpu.bpred_target[bp_index]    = target;
    }

    // ---- Fill EX/MEM ----
    next_ex_mem.valid      = cpu.id_ex.valid;
    next_ex_mem.instr_id   = cpu.id_ex.instr_id;
    next_ex_mem.pc         = cpu.id_ex.pc;
    next_ex_mem.raw        = cpu.id_ex.dinstr.raw;

    next_ex_mem.alu_result = alu_result;
    next_ex_mem.store_data = op2; // store-data value (rs2) for stores

    next_ex_mem.rd         = d.rd;

    // Disable any side effects for trap instructions (they already updated CSRs + redirected PC)
    bool is_trap = (d.kind == InstrKind::ECALL || d.kind == InstrKind::EBREAK || d.kind == InstrKind::ILLEGAL);

    next_ex_mem.mem_read   = (!is_trap) && d.mem_read;
    next_ex_mem.mem_write  = (!is_trap) && d.mem_write;
    next_ex_mem.mem_to_reg = (!is_trap) && d.mem_to_reg;
    next_ex_mem.mem_size   = (!is_trap) ? d.mem_size : 0;
    next_ex_mem.mem_signed = (!is_trap) ? d.mem_signed : false;

    // CSR reads produce alu_result; reg_write is controlled by d.writes_rd.
    // Trap/mret should not write regs.
    bool is_mret = (d.kind == InstrKind::MRET);
    next_ex_mem.reg_write  = (!is_trap && !is_mret) && d.writes_rd;
}

// ---------------- ID ----------------

void do_id(CPU& cpu, ID_EX& next_id_ex, bool& stall)
{
    stall = false;
    next_id_ex = {};

    if (!cpu.if_id.valid) {
        return;
    }

    uint32_t raw = cpu.if_id.instr;
    DecodedInstr d = decode(raw);

    // ---- Load-use hazard detection ----
    if (cpu.id_ex.valid &&
        cpu.id_ex.dinstr.mem_read &&
        cpu.id_ex.dinstr.writes_rd &&
        cpu.id_ex.dinstr.rd != 0) {

        const uint8_t load_rd = cpu.id_ex.dinstr.rd;
        const bool dep_rs1 = d.uses_rs1 && (load_rd == d.rs1);
        const bool dep_rs2 = d.uses_rs2 && (load_rd == d.rs2);

        if (dep_rs1 || dep_rs2) {
            stall = true;
            cpu.stats.stall_cycles++;
            return;
        }
    }

    next_id_ex.valid    = true;
    next_id_ex.instr_id = cpu.if_id.instr_id;
    next_id_ex.pc       = cpu.if_id.pc;
    next_id_ex.dinstr   = d;

    // Note: CSR immediate forms do not use rs1.
    next_id_ex.rs1_val  = (d.uses_rs1 && d.rs1 < 32) ? cpu.regs[d.rs1] : 0;
    next_id_ex.rs2_val  = (d.uses_rs2 && d.rs2 < 32) ? cpu.regs[d.rs2] : 0;

    next_id_ex.bp_index         = cpu.if_id.bp_index;
    next_id_ex.predicted_taken  = cpu.if_id.predicted_taken;
    next_id_ex.predicted_target = cpu.if_id.predicted_target;
}

// ---------------- IF ----------------

void do_if(CPU& cpu, IF_ID& next_if_id,
           bool redirect, uint32_t redirect_target, bool stall)
{
    // On stall, just hold IF/ID and don't advance PC
    if (stall) {
        next_if_id = cpu.if_id;
        return;
    }

    // If the previous cycle signaled a redirect,
    // update PC and inject a bubble into IF/ID.
    if (redirect) {
        cpu.pc = redirect_target;

        next_if_id = {};
        next_if_id.valid = false;
        next_if_id.pc = cpu.pc;
        return;
    }

    // Misaligned fetch trap (optional, but makes behavior defined)
    if ((cpu.pc & 0x3u) != 0u) {
        cpu.enter_trap(0u, cpu.pc); // instruction address misaligned
        uint32_t vec = cpu.trap_vector();
        if (vec == 0) vec = cpu.mem.prog_max;
        cpu.pc = vec;

        next_if_id = {};
        next_if_id.valid = false;
        next_if_id.pc = cpu.pc;
        return;
    }

    // Normal predicted fetch
    uint32_t pc = cpu.pc;
    next_if_id.pc = pc;

    // Check program bounds for fetch validity
    if (pc < cpu.mem.prog_min || pc >= cpu.mem.prog_max) {
        next_if_id.valid = false;
        next_if_id.instr_id = 0;
        next_if_id.instr = 0;
        return;
    }

    // Fetch instruction (from dmem virtual mapping)
    uint32_t instr = load_u32(cpu.mem, pc);

    next_if_id.valid = true;
    next_if_id.instr = instr;
    next_if_id.instr_id = cpu.next_instr_id++;

    // Branch predictor lookup (tagged BTB)
    const uint32_t bp_index = (pc >> 2) & (CPU::BP_ENTRIES - 1);
    const uint32_t pc_tag   = pc >> 10; // 2 (word) + 8 (index)

    bool entry_hit = cpu.bpred_valid[bp_index] &&
                     cpu.bpred_is_branch[bp_index] &&
                     (cpu.bpred_tag[bp_index] == pc_tag);

    bool predicted_taken = entry_hit && (cpu.bpred_counter[bp_index] >= 2);
    uint32_t predicted_target = entry_hit ? cpu.bpred_target[bp_index] : 0u;

    // Safety: only predict for branch opcodes (cheap predecode)
    const bool is_branch_opcode = ((instr & 0x7Fu) == 0b1100011);
    if (!is_branch_opcode) {
        predicted_taken = false;
        predicted_target = 0u;
    }

    next_if_id.bp_index         = static_cast<uint16_t>(bp_index);
    next_if_id.predicted_taken  = predicted_taken;
    next_if_id.predicted_target = predicted_target;

    // Choose the next PC based on prediction
    uint32_t next_pc = pc + 4;
    if (predicted_taken) {
        next_pc = predicted_target;
    }

    cpu.pc = next_pc;
}

} // namespace cpu
