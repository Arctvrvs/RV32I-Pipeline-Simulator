#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "cpu/cpu_core.h"
#include "cpu/instr.h"
#include "cpu/memory.h"

namespace {

static inline uint32_t align_pc(uint32_t pc) { return pc & ~0x3u; }

static std::string reg_name(uint8_t r) {
    return "x" + std::to_string(static_cast<int>(r));
}

// Copied (with tiny formatting tweaks) from src/main.cpp for consistency.
static std::string instr_to_asm(const cpu::DecodedInstr& d) {
    using cpu::InstrKind;
    std::ostringstream oss;

    switch (d.kind) {
    case InstrKind::ECALL:  oss << "ecall"; break;
    case InstrKind::EBREAK: oss << "ebreak"; break;
    case InstrKind::MRET:   oss << "mret"; break;

    case InstrKind::CSRRW:
        oss << "csrrw " << reg_name(d.rd) << ", 0x" << std::hex << d.csr_addr << std::dec
            << ", " << reg_name(d.rs1);
        break;
    case InstrKind::CSRRS:
        oss << "csrrs " << reg_name(d.rd) << ", 0x" << std::hex << d.csr_addr << std::dec
            << ", " << reg_name(d.rs1);
        break;
    case InstrKind::CSRRC:
        oss << "csrrc " << reg_name(d.rd) << ", 0x" << std::hex << d.csr_addr << std::dec
            << ", " << reg_name(d.rs1);
        break;
    case InstrKind::CSRRWI:
        oss << "csrrwi " << reg_name(d.rd) << ", 0x" << std::hex << d.csr_addr << std::dec
            << ", " << (d.csr_zimm & 0x1F);
        break;
    case InstrKind::CSRRSI:
        oss << "csrrsi " << reg_name(d.rd) << ", 0x" << std::hex << d.csr_addr << std::dec
            << ", " << (d.csr_zimm & 0x1F);
        break;
    case InstrKind::CSRRCI:
        oss << "csrrci " << reg_name(d.rd) << ", 0x" << std::hex << d.csr_addr << std::dec
            << ", " << (d.csr_zimm & 0x1F);
        break;

    case InstrKind::ILLEGAL:
        oss << "illegal";
        break;

    case InstrKind::ADD:
        oss << "add  " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << reg_name(d.rs2);
        break;
    case InstrKind::SUB:
        oss << "sub  " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << reg_name(d.rs2);
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
        oss << "beq  " << reg_name(d.rs1) << ", " << reg_name(d.rs2) << ", " << d.imm;
        break;
    case InstrKind::BNE:
        oss << "bne  " << reg_name(d.rs1) << ", " << reg_name(d.rs2) << ", " << d.imm;
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
        oss << "jal  " << reg_name(d.rd) << ", " << d.imm;
        break;
    case InstrKind::JALR:
        oss << "jalr " << reg_name(d.rd) << ", " << d.imm << "(" << reg_name(d.rs1) << ")";
        break;

    case InstrKind::LUI:
        oss << "lui  " << reg_name(d.rd) << ", " << d.imm;
        break;
    case InstrKind::AUIPC:
        oss << "auipc " << reg_name(d.rd) << ", " << d.imm;
        break;

    case InstrKind::NOP:
        oss << "nop";
        break;

    default:
        oss << "?";
        break;
    }

    return oss.str();
}

static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                // control char
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                out += buf;
            } else {
                out += c;
            }
        }
    }
    return out;
}

static bool peek_u32(const cpu::CPU& cpu, uint32_t addr, uint32_t& out_raw) {
    uint32_t a = align_pc(addr);
    if (!cpu.mem.is_mapped(a, 4)) return false;
    out_raw = cpu::load_u32(cpu.mem, a);
    return true;
}

static void soft_reset_keep_mem(cpu::CPU& c, uint32_t pc) {
    c.pc = pc;
    c.next_instr_id = 1;
    c.uart_buffer.clear();

    for (auto& r : c.regs) r = 0;

    c.csr_mstatus = 0;
    c.csr_mtvec   = 0;
    c.csr_mepc    = 0;
    c.csr_mcause  = 0;

    c.if_id = {};
    c.id_ex = {};
    c.ex_mem = {};
    c.mem_wb = {};
    c.last_commit = {};

    for (size_t i = 0; i < cpu::CPU::BP_ENTRIES; ++i) {
        c.bpred_counter[i] = 1;
        c.bpred_target[i] = 0;
        c.bpred_tag[i] = 0;
        c.bpred_is_branch[i] = false;
        c.bpred_valid[i] = false;
    }

    c.stats = {};
    c.halted = false;
}

struct Sim {
    cpu::CPU cpu;
    cpu::Memory initial_mem;
    uint32_t entry = 0;
    bool has_program = false;

    std::string state_json;
    std::string disasm_json;
    std::string mem_json;

    Sim() : cpu(64 * 1024), initial_mem(64 * 1024) {}
};

static Sim* as_sim(uint32_t h) {
    return reinterpret_cast<Sim*>(static_cast<uintptr_t>(h));
}

static void append_stage_json(std::ostringstream& oss,
                             const char* name,
                             bool valid,
                             uint64_t instr_id,
                             uint32_t pc,
                             uint32_t raw,
                             const cpu::DecodedInstr* d,
                             bool pred_valid,
                             bool predicted_taken,
                             uint32_t predicted_target,
                             bool mem_r,
                             bool mem_w,
                             uint32_t mem_addr,
                             uint32_t store_data,
                             bool wb_w,
                             uint8_t wb_rd,
                             uint32_t wb_data) {
    oss << "\"" << name << "\":{";
    oss << "\"valid\":" << (valid ? "true" : "false") << ",";
    oss << "\"instr_id\":" << (unsigned long long)instr_id << ",";
    oss << "\"pc\":" << pc << ",";
    oss << "\"raw\":" << raw << ",";
    if (valid && d) {
        std::string asm_s = instr_to_asm(*d);
        oss << "\"asm\":\"" << json_escape(asm_s) << "\",";
        oss << "\"rs1\":" << (int)d->rs1 << ",";
        oss << "\"rs2\":" << (int)d->rs2 << ",";
        oss << "\"rd\":"  << (int)d->rd  << ",";
        oss << "\"imm\":" << (int)d->imm << ",";
        oss << "\"uses_rs1\":" << (d->uses_rs1 ? "true" : "false") << ",";
        oss << "\"uses_rs2\":" << (d->uses_rs2 ? "true" : "false") << ",";
        oss << "\"writes_rd\":" << (d->writes_rd ? "true" : "false") << ",";
        oss << "\"mem_read\":" << (d->mem_read ? "true" : "false") << ",";
        oss << "\"mem_write\":" << (d->mem_write ? "true" : "false") << ",";
        oss << "\"mem_to_reg\":" << (d->mem_to_reg ? "true" : "false") << ",";
        oss << "\"mem_size\":" << (int)d->mem_size << ",";
        oss << "\"mem_signed\":" << (d->mem_signed ? "true" : "false") << ",";
        oss << "\"is_branch\":" << (d->is_branch ? "true" : "false") << ",";
        oss << "\"is_jump\":" << (d->is_jump ? "true" : "false") << ",";
        oss << "\"csr_addr\":" << (int)d->csr_addr << ",";
        oss << "\"csr_zimm\":" << (int)d->csr_zimm << ",";
    } else {
        oss << "\"asm\":\"\",";
        oss << "\"rs1\":0,\"rs2\":0,\"rd\":0,\"imm\":0,";
        oss << "\"uses_rs1\":false,\"uses_rs2\":false,\"writes_rd\":false,";
        oss << "\"mem_read\":false,\"mem_write\":false,\"mem_to_reg\":false,";
        oss << "\"mem_size\":0,\"mem_signed\":false,\"is_branch\":false,\"is_jump\":false,";
        oss << "\"csr_addr\":0,\"csr_zimm\":0,";
    }

    oss << "\"pred_valid\":" << (pred_valid ? "true" : "false") << ",";
    oss << "\"pred_taken\":" << (predicted_taken ? "true" : "false") << ",";
    oss << "\"pred_target\":" << predicted_target << ",";

    oss << "\"mem_r\":" << (mem_r ? "true" : "false") << ",";
    oss << "\"mem_w\":" << (mem_w ? "true" : "false") << ",";
    oss << "\"mem_addr\":" << mem_addr << ",";
    oss << "\"store_data\":" << store_data << ",";

    oss << "\"wb_w\":" << (wb_w ? "true" : "false") << ",";
    oss << "\"wb_rd\":" << (int)wb_rd << ",";
    oss << "\"wb_data\":" << wb_data;

    oss << "}";
}

} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE uint32_t sim_create() {
    auto* s = new Sim();
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s));
}

EMSCRIPTEN_KEEPALIVE void sim_destroy(uint32_t h) {
    delete as_sim(h);
}

// Loads a .hex or ELF32 from Emscripten FS and resets CPU state to entry.
// Returns entry PC on success; 0xFFFFFFFF on failure.
EMSCRIPTEN_KEEPALIVE uint32_t sim_load_program(uint32_t h, const char* path) {
    Sim* s = as_sim(h);
    if (!s || !path) return 0xFFFFFFFFu;

    uint32_t entry = 0;
    if (!cpu::load_program(path, s->cpu.mem, entry, nullptr)) {
        return 0xFFFFFFFFu;
    }

    s->entry = entry;
    s->initial_mem = s->cpu.mem;
    s->has_program = true;
    soft_reset_keep_mem(s->cpu, entry);
    return entry;
}

EMSCRIPTEN_KEEPALIVE void sim_soft_reset(uint32_t h) {
    Sim* s = as_sim(h);
    if (!s) return;
    if (s->has_program) {
        s->cpu.mem = s->initial_mem;
    }
    soft_reset_keep_mem(s->cpu, s->entry);
}

EMSCRIPTEN_KEEPALIVE void sim_set_pc(uint32_t h, uint32_t pc) {
    Sim* s = as_sim(h);
    if (!s) return;
    s->cpu.pc = pc;
    s->cpu.halted = false;
}

EMSCRIPTEN_KEEPALIVE uint32_t sim_get_pc(uint32_t h) {
    Sim* s = as_sim(h);
    return s ? s->cpu.pc : 0;
}

EMSCRIPTEN_KEEPALIVE uint32_t sim_step_cycles(uint32_t h, uint32_t cycles) {
    Sim* s = as_sim(h);
    if (!s) return 0;
    uint32_t done = 0;
    for (; done < cycles; ++done) {
        if (s->cpu.halted) break;
        s->cpu.step();
    }
    return done;
}

// Steps until the next committed instruction reaches WB (or max_cycles/halt).
// Returns cycles executed.
EMSCRIPTEN_KEEPALIVE uint32_t sim_step_commit(uint32_t h, uint32_t max_cycles) {
    Sim* s = as_sim(h);
    if (!s) return 0;
    uint32_t done = 0;
    for (; done < max_cycles; ++done) {
        if (s->cpu.halted) break;
        s->cpu.step();
        if (s->cpu.last_commit.valid) {
            ++done; // include cycle we just executed
            break;
        }
    }
    return done;
}

EMSCRIPTEN_KEEPALIVE const char* sim_get_state_json(uint32_t h) {
    Sim* s = as_sim(h);
    if (!s) return "{}";

    cpu::CPU& cpu = s->cpu;

    // IF: peek current fetch
    uint32_t if_raw = 0;
    bool if_valid = peek_u32(cpu, cpu.pc, if_raw);
    cpu::DecodedInstr if_d = if_valid ? cpu::decode(if_raw) : cpu::DecodedInstr{};

    // ID
    cpu::DecodedInstr id_d = cpu.if_id.valid ? cpu::decode(cpu.if_id.instr) : cpu::DecodedInstr{};

    // EX
    const cpu::DecodedInstr* ex_d_ptr = cpu.id_ex.valid ? &cpu.id_ex.dinstr : nullptr;

    // MEM/WB decode from raw
    cpu::DecodedInstr mem_d = cpu.ex_mem.valid ? cpu::decode(cpu.ex_mem.raw) : cpu::DecodedInstr{};
    cpu::DecodedInstr wb_d  = cpu.mem_wb.valid ? cpu::decode(cpu.mem_wb.raw) : cpu::DecodedInstr{};

    std::ostringstream oss;
    oss << "{";

    // Top-level state
    oss << "\"pc\":" << cpu.pc << ",";
    oss << "\"entry\":" << s->entry << ",";
    oss << "\"halted\":" << (cpu.halted ? "true" : "false") << ",";

    // Stats
    oss << "\"stats\":{"
        << "\"cycles\":" << (unsigned long long)cpu.stats.cycles << ","
        << "\"committed\":" << (unsigned long long)cpu.stats.committed_instructions << ","
        << "\"stall_cycles\":" << (unsigned long long)cpu.stats.stall_cycles << ","
        << "\"branch_instr\":" << (unsigned long long)cpu.stats.branch_instructions << ","
        << "\"branch_pred\":" << (unsigned long long)cpu.stats.branch_predictions << ","
        << "\"branch_misp\":" << (unsigned long long)cpu.stats.branch_mispredictions
        << "},";

    // last commit
    oss << "\"last_commit\":{";
    oss << "\"valid\":" << (cpu.last_commit.valid ? "true" : "false") << ",";
    oss << "\"instr_id\":" << (unsigned long long)cpu.last_commit.instr_id << ",";
    oss << "\"pc\":" << cpu.last_commit.pc << ",";
    oss << "\"raw\":" << cpu.last_commit.raw << ",";
    if (cpu.last_commit.valid) {
        cpu::DecodedInstr cd = cpu::decode(cpu.last_commit.raw);
        oss << "\"asm\":\"" << json_escape(instr_to_asm(cd)) << "\",";
    } else {
        oss << "\"asm\":\"\",";
    }
    oss << "\"reg_write\":" << (cpu.last_commit.reg_write ? "true" : "false") << ",";
    oss << "\"rd\":" << (int)cpu.last_commit.rd << ",";
    oss << "\"wb_data\":" << cpu.last_commit.wb_data << ",";
    oss << "\"mem_is_load\":" << (cpu.last_commit.mem_is_load ? "true" : "false") << ",";
    oss << "\"mem_is_store\":" << (cpu.last_commit.mem_is_store ? "true" : "false") << ",";
    oss << "\"mem_addr\":" << cpu.last_commit.mem_addr << ",";
    oss << "\"mem_store_data\":" << cpu.last_commit.mem_store_data << ",";
    oss << "\"mem_size\":" << (int)cpu.last_commit.mem_size << ",";
    oss << "\"mem_signed\":" << (cpu.last_commit.mem_signed ? "true" : "false");
    oss << "},";

    // Registers
    oss << "\"regs\":[";
    for (int i = 0; i < 32; ++i) {
        if (i) oss << ',';
        oss << cpu.regs[i];
    }
    oss << "],";

    // UART (tail)
    {
        const size_t MAX_UART = 4096;
        std::string tail = cpu.uart_buffer;
        if (tail.size() > MAX_UART) tail = tail.substr(tail.size() - MAX_UART);
        oss << "\"uart\":\"" << json_escape(tail) << "\",";
    }

    // Pipeline stages
    oss << "\"stages\":{";
    append_stage_json(oss,
        "IF",
        if_valid,
        cpu.next_instr_id,
        cpu.pc,
        if_valid ? if_raw : 0u,
        if_valid ? &if_d : nullptr,
        false, false, 0u,
        false, false, 0u, 0u,
        false, 0u, 0u);
    oss << ',';

    append_stage_json(oss,
        "ID",
        cpu.if_id.valid,
        cpu.if_id.instr_id,
        cpu.if_id.pc,
        cpu.if_id.instr,
        cpu.if_id.valid ? &id_d : nullptr,
        cpu.if_id.valid,
        cpu.if_id.predicted_taken,
        cpu.if_id.predicted_target,
        false, false, 0u, 0u,
        false, 0u, 0u);
    oss << ',';

    append_stage_json(oss,
        "EX",
        cpu.id_ex.valid,
        cpu.id_ex.instr_id,
        cpu.id_ex.pc,
        cpu.id_ex.valid ? cpu.id_ex.dinstr.raw : 0u,
        ex_d_ptr,
        cpu.id_ex.valid,
        cpu.id_ex.predicted_taken,
        cpu.id_ex.predicted_target,
        false, false, 0u, 0u,
        false, 0u, 0u);
    oss << ',';

    append_stage_json(oss,
        "MEM",
        cpu.ex_mem.valid,
        cpu.ex_mem.instr_id,
        cpu.ex_mem.pc,
        cpu.ex_mem.raw,
        cpu.ex_mem.valid ? &mem_d : nullptr,
        false, false, 0u,
        cpu.ex_mem.mem_read,
        cpu.ex_mem.mem_write,
        cpu.ex_mem.alu_result,
        cpu.ex_mem.store_data,
        false, 0u, 0u);
    oss << ',';

    append_stage_json(oss,
        "WB",
        cpu.mem_wb.valid,
        cpu.mem_wb.instr_id,
        cpu.mem_wb.pc,
        cpu.mem_wb.raw,
        cpu.mem_wb.valid ? &wb_d : nullptr,
        false, false, 0u,
        cpu.mem_wb.mem_is_load,
        cpu.mem_wb.mem_is_store,
        cpu.mem_wb.mem_addr,
        cpu.mem_wb.mem_store_data,
        cpu.mem_wb.reg_write,
        cpu.mem_wb.rd,
        cpu.mem_wb.wb_data);

    oss << "}"; // stages
    oss << "}"; // root

    s->state_json = oss.str();
    return s->state_json.c_str();
}

// Returns a JSON array of disassembly lines: [{pc, raw, asm, mapped}, ...]
EMSCRIPTEN_KEEPALIVE const char* sim_get_disasm_json(uint32_t h, uint32_t start_pc, uint32_t count) {
    Sim* s = as_sim(h);
    if (!s) return "[]";

    cpu::CPU& cpu = s->cpu;
    std::ostringstream oss;
    oss << "[";

    uint32_t pc = align_pc(start_pc);
    for (uint32_t i = 0; i < count; ++i) {
        if (i) oss << ',';
        uint32_t raw = 0;
        bool mapped = peek_u32(cpu, pc, raw);
        std::string asm_s;
        if (mapped) {
            cpu::DecodedInstr d = cpu::decode(raw);
            asm_s = instr_to_asm(d);
        }
        oss << "{";
        oss << "\"pc\":" << pc << ",";
        oss << "\"raw\":" << (mapped ? raw : 0u) << ",";
        oss << "\"mapped\":" << (mapped ? "true" : "false") << ",";
        oss << "\"asm\":\"" << json_escape(mapped ? asm_s : std::string("<unmapped>")) << "\"";
        oss << "}";
        pc += 4;
    }

    oss << "]";
    s->disasm_json = oss.str();
    return s->disasm_json.c_str();
}

// Returns a JSON table of memory (4 words/row):
// [{addr, w:[w0,w1,w2,w3]}, ...]
EMSCRIPTEN_KEEPALIVE const char* sim_get_mem_json(uint32_t h, uint32_t base_addr, uint32_t rows) {
    Sim* s = as_sim(h);
    if (!s) return "[]";

    cpu::CPU& cpu = s->cpu;
    std::ostringstream oss;
    oss << "[";

    uint32_t addr = base_addr;
    addr &= ~0xFu; // align to 16 bytes

    for (uint32_t r = 0; r < rows; ++r) {
        if (r) oss << ',';
        oss << "{";
        oss << "\"addr\":" << addr << ",";
        oss << "\"w\":[";
        for (int c = 0; c < 4; ++c) {
            if (c) oss << ',';
            uint32_t a = addr + static_cast<uint32_t>(c * 4);
            uint32_t w = 0;
            bool mapped = cpu.mem.is_mapped(a, 4);
            if (mapped) w = cpu::load_u32(cpu.mem, a);
            oss << (mapped ? w : 0u);
        }
        oss << "]";
        oss << "}";
        addr += 16;
    }

    oss << "]";
    s->mem_json = oss.str();
    return s->mem_json.c_str();
}

} // extern "C"
