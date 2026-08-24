// gui_main.cpp

#include "imgui.h"
#include "../external/implot/implot.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "../external/glfw/include/GLFW/glfw3.h"

#include <cstdio>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <map>

// CPU core:
#include "cpu/cpu_core.h"
#include "cpu/memory.h"
#include "cpu/instr.h"

// ---------- Helpers (shared style w/ CLI) ----------

static std::string reg_name(uint8_t r) {
    return "x" + std::to_string(static_cast<int>(r));
}

static const char* instr_kind_name(cpu::InstrKind k) {
    using cpu::InstrKind;
    switch (k) {
    case InstrKind::NOP:   return "NOP";
    case InstrKind::ILLEGAL: return "ILLEGAL";
    case InstrKind::ECALL: return "ECALL";
    case InstrKind::EBREAK:return "EBREAK";
    case InstrKind::MRET:  return "MRET";
    case InstrKind::CSRRW: return "CSRRW";
    case InstrKind::CSRRS: return "CSRRS";
    case InstrKind::CSRRC: return "CSRRC";
    case InstrKind::CSRRWI:return "CSRRWI";
    case InstrKind::CSRRSI:return "CSRRSI";
    case InstrKind::CSRRCI:return "CSRRCI";
    case InstrKind::ADD:   return "ADD";
    case InstrKind::SUB:   return "SUB";
    case InstrKind::SLL:   return "SLL";
    case InstrKind::SLT:   return "SLT";
    case InstrKind::SLTU:  return "SLTU";
    case InstrKind::XOR:   return "XOR";
    case InstrKind::SRL:   return "SRL";
    case InstrKind::SRA:   return "SRA";
    case InstrKind::OR:    return "OR";
    case InstrKind::AND:   return "AND";
    case InstrKind::ADDI:  return "ADDI";
    case InstrKind::SLLI:  return "SLLI";
    case InstrKind::SLTI:  return "SLTI";
    case InstrKind::SLTIU: return "SLTIU";
    case InstrKind::XORI:  return "XORI";
    case InstrKind::SRLI:  return "SRLI";
    case InstrKind::SRAI:  return "SRAI";
    case InstrKind::ORI:   return "ORI";
    case InstrKind::ANDI:  return "ANDI";
    case InstrKind::LB:    return "LB";
    case InstrKind::LH:    return "LH";
    case InstrKind::LW:    return "LW";
    case InstrKind::LBU:   return "LBU";
    case InstrKind::LHU:   return "LHU";
    case InstrKind::SB:    return "SB";
    case InstrKind::SH:    return "SH";
    case InstrKind::SW:    return "SW";
    case InstrKind::BEQ:   return "BEQ";
    case InstrKind::BNE:   return "BNE";
    case InstrKind::BLT:   return "BLT";
    case InstrKind::BGE:   return "BGE";
    case InstrKind::BLTU:  return "BLTU";
    case InstrKind::BGEU:  return "BGEU";
    case InstrKind::JAL:   return "JAL";
    case InstrKind::JALR:  return "JALR";
    case InstrKind::LUI:   return "LUI";
    case InstrKind::AUIPC: return "AUIPC";
    default:               return "?";
    }
}

// Full pretty-print, matching the CLI's behavior.
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
        oss << "jalr " << reg_name(d.rd) << ", " << reg_name(d.rs1) << ", " << d.imm;
        break;
    case InstrKind::LUI:
        oss << "lui  " << reg_name(d.rd) << ", " << d.imm;
        break;
    case InstrKind::AUIPC:
        oss << "auipc " << reg_name(d.rd) << ", " << d.imm;
        break;

    case InstrKind::NOP:
    default:
        oss << "nop";
        break;
    }
    return oss.str();
}

// ---------- CPI history for live ImPlot graph ----------

struct CpiHistory {
    std::vector<double> x;   // cycles
    std::vector<double> y;   // CPI
    std::size_t max_points = 1024;

    void add(uint64_t cycles, uint64_t committed) {
        if (committed == 0) {
            // no meaningful CPI yet
            return;
        }

        double cx  = static_cast<double>(cycles);
        double cpi = static_cast<double>(cycles) /
                     static_cast<double>(committed);

        if (!x.empty() && x.back() == cx) {
            // same cycle, just update last point
            y.back() = cpi;
            return;
        }

        x.push_back(cx);
        y.push_back(cpi);

        if (x.size() > max_points) {
            x.erase(x.begin());
            y.erase(y.begin());
        }
    }

    void clear() {
        x.clear();
        y.clear();
    }
};

// ---------- Watch list ----------

enum class WatchKind {
    Reg,
    Mem
};

struct WatchItem {
    WatchKind kind;
    int       reg;    // if Reg
    uint32_t  addr;   // if Mem
};

struct WatchState {
    char input_buf[32] = {0};
    std::vector<WatchItem> items;
};

// ---------- Pipeline timeline history ----------

struct PipelineSample {
    uint64_t cycle;
    uint32_t if_pc;
    uint32_t id_pc;
    uint32_t ex_pc;
    uint32_t mem_pc;
    uint32_t wb_pc;
};

// ---------- Perf stats (instruction categories) ----------

struct PerfStats {
    uint64_t alu   = 0;   // add/sub/addi/lui, etc.
    uint64_t mem   = 0;   // lw/sw
    uint64_t branch = 0;  // beq/bne/jal/jalr
    uint64_t other = 0;   // anything else
};

// Forward decl (definition is later in the file).
static void categorize_for_perf(const cpu::DecodedInstr& d, PerfStats& perf);

// ---------- GUI state (memory, breakpoints, highlight, disasm, watch, heatmap, perf) ----------

struct GuiState {
    uint32_t mem_base       = 0;      // base byte address for memory window
    uint32_t break_pc       = 0;      // breakpoint PC (byte address)
    bool     break_enabled  = false;
    bool     breakpoint_hit = false;

    float    reg_highlight[32] = {0.0f}; // fade-out highlight strength [0..1]
    uint32_t last_mem_addr     = 0;
    float    last_mem_highlight = 0.0f;  // fade-out for last memory access

    uint32_t disasm_base_pc = 0;      // base PC to show in disasm view

    // One-page dashboard controls
    bool     follow_pc         = true;  // disasm follows current PC
    bool     auto_follow_instr = true;  // highlight tracks last committed instruction
    uint64_t follow_instr_id   = 0;     // instruction id currently highlighted
    int      selected_stage    = 1;     // 0=IF,1=ID,2=EX,3=MEM,4=WB
    bool     auto_follow_mem   = true;  // memory view follows last load/store

    // Per-frame event detection (derived from stats deltas)
    uint64_t prev_cycles       = 0;
    uint64_t prev_committed    = 0;
    uint64_t prev_stall_cycles = 0;
    uint64_t prev_mispredicts  = 0;
    bool     last_cycle_stall      = false;
    bool     last_cycle_commit     = false;
    bool     last_cycle_mispredict = false;

    // Convenience inputs (hex)
    uint32_t view_pc_input = 0; // "PC box" for jumping the disassembly/memory view

    // Optional ELF symbols: address -> name
    std::unordered_map<uint32_t, std::string> symbols;

    WatchState watch;

    // Heatmap usage counts
    uint64_t reg_use_count[32] = {0};
    std::unordered_map<uint32_t, uint64_t> mem_use_count;

    // Pipeline timeline
    std::vector<PipelineSample> pipeline_hist;
    uint64_t last_hist_cycle = 0;
    std::size_t timeline_max_samples = 64;

    // Perf categories
    PerfStats perf;
};

// ---------------- Dashboard helpers ----------------

static uint32_t clamp_disasm_base(const cpu::CPU& cpu, uint32_t base_pc) {
    const uint32_t prog_min = cpu.mem.prog_min;
    const uint32_t prog_max = cpu.mem.prog_max;
    if (prog_max <= prog_min) return prog_min;

    base_pc &= ~0x3u;
    if (base_pc < prog_min) base_pc = prog_min;
    if (base_pc >= prog_max) base_pc = prog_max - 4u;
    return base_pc;
}

static void reset_gui_state(GuiState& gui, const cpu::CPU& cpu,
                            const std::unordered_map<uint32_t, std::string>& symbols,
                            uint32_t entry)
{
    gui = GuiState{};
    gui.symbols = symbols;
    gui.mem_base = cpu.mem.prog_min;
    gui.disasm_base_pc = (entry != 0) ? (entry & ~0x3u) : cpu.mem.prog_min;
    gui.view_pc_input = gui.disasm_base_pc;
    gui.follow_pc = true;
    gui.auto_follow_instr = true;
    gui.auto_follow_mem = true;
    gui.selected_stage = 1; // ID
}

static void update_events(GuiState& gui, const cpu::CPU& cpu) {
    gui.last_cycle_stall      = (cpu.stats.stall_cycles > gui.prev_stall_cycles);
    gui.last_cycle_mispredict = (cpu.stats.branch_mispredictions > gui.prev_mispredicts);
    gui.last_cycle_commit     = (cpu.stats.committed_instructions > gui.prev_committed);

    gui.prev_cycles       = cpu.stats.cycles;
    gui.prev_committed    = cpu.stats.committed_instructions;
    gui.prev_stall_cycles = cpu.stats.stall_cycles;
    gui.prev_mispredicts  = cpu.stats.branch_mispredictions;
}

static bool step_until_commit(cpu::CPU& cpu, int safety_max_cycles = 50000) {
    uint64_t before = cpu.stats.committed_instructions;
    for (int i = 0; i < safety_max_cycles && !cpu.halted; ++i) {
        cpu.step();
        if (cpu.stats.committed_instructions != before) return true;
    }
    return false;
}

static bool decode_at_pc(cpu::CPU& cpu, uint32_t pc, uint32_t& raw_out, cpu::DecodedInstr& dec_out) {
    pc &= ~0x3u;
    if (!cpu.mem.is_mapped(pc, 4)) return false;
    raw_out = cpu::load_u32(cpu.mem, pc);
    dec_out = cpu::decode(raw_out);
    return true;
}

static const char* imm_format_from_opcode(uint32_t raw) {
    uint32_t opcode = raw & 0x7Fu;
    switch (opcode) {
    case 0x33: return "R";         // OP
    case 0x13: return "I";         // OP-IMM
    case 0x03: return "I(load)";   // LOAD
    case 0x67: return "I(jalr)";   // JALR
    case 0x23: return "S";         // STORE
    case 0x63: return "B";         // BRANCH
    case 0x6F: return "J";         // JAL
    case 0x37: return "U(lui)";    // LUI
    case 0x17: return "U(auipc)";  // AUIPC
    case 0x73: return "SYSTEM";    // SYSTEM
    default:   return "?";
    }
}

static bool branch_taken(cpu::InstrKind k, uint32_t op1, uint32_t op2) {
    using cpu::InstrKind;
    switch (k) {
    case InstrKind::BEQ:  return op1 == op2;
    case InstrKind::BNE:  return op1 != op2;
    case InstrKind::BLT:  return (int32_t)op1 < (int32_t)op2;
    case InstrKind::BGE:  return (int32_t)op1 >= (int32_t)op2;
    case InstrKind::BLTU: return op1 < op2;
    case InstrKind::BGEU: return op1 >= op2;
    default: return false;
    }
}

static void update_after_stepping(cpu::CPU& cpu, GuiState& gui, CpiHistory& cpi_hist) {
    // Fade register highlights
    for (int i = 0; i < 32; ++i) {
        gui.reg_highlight[i] *= 0.9f;
        if (gui.reg_highlight[i] < 0.01f) gui.reg_highlight[i] = 0.0f;
    }

    // Highlight last-written register
    if (cpu.mem_wb.valid && cpu.mem_wb.rd < 32 && cpu.mem_wb.rd != 0 && cpu.mem_wb.reg_write) {
        gui.reg_highlight[cpu.mem_wb.rd] = 1.0f;
    }

    // Track last memory access
    if (cpu.last_commit.valid && (cpu.last_commit.mem_is_load || cpu.last_commit.mem_is_store)) {
        gui.last_mem_addr      = cpu.last_commit.mem_addr;
        gui.last_mem_highlight = 1.0f;
        gui.mem_use_count[gui.last_mem_addr]++;
        if (gui.auto_follow_mem) {
            gui.mem_base = gui.last_mem_addr & ~0xFu;
        }
    } else if (cpu.ex_mem.valid && (cpu.ex_mem.mem_read || cpu.ex_mem.mem_write)) {
        gui.last_mem_addr      = cpu.ex_mem.alu_result;
        gui.last_mem_highlight = 1.0f;
        gui.mem_use_count[gui.last_mem_addr]++;
        if (gui.auto_follow_mem) {
            gui.mem_base = gui.last_mem_addr & ~0xFu;
        }
    }

    gui.last_mem_highlight *= 0.9f;
    if (gui.last_mem_highlight < 0.01f) gui.last_mem_highlight = 0.0f;

    // CPI sample
    cpi_hist.add(cpu.stats.cycles, cpu.stats.committed_instructions);

    // Perf / heatmap approximation
    if (cpu.id_ex.valid) {
        const auto& d = cpu.id_ex.dinstr;
        if (d.rs1 < 32) gui.reg_use_count[d.rs1]++;
        if (d.rs2 < 32) gui.reg_use_count[d.rs2]++;
        if (d.rd  < 32) gui.reg_use_count[d.rd]++;
        categorize_for_perf(d, gui.perf);
    }

    // Auto-follow the instruction highlight
    if (gui.auto_follow_instr) {
        if (cpu.last_commit.valid) gui.follow_instr_id = cpu.last_commit.instr_id;
        else if (cpu.id_ex.valid)  gui.follow_instr_id = cpu.id_ex.instr_id;
        else if (cpu.if_id.valid)  gui.follow_instr_id = cpu.if_id.instr_id;
    }

    // Disassembly follow PC
    if (gui.follow_pc) {
        // Keep current PC roughly centered.
        uint32_t base = (cpu.pc >= 10u * 4u) ? (cpu.pc - 10u * 4u) : cpu.mem.prog_min;
        gui.disasm_base_pc = clamp_disasm_base(cpu, base);
        gui.view_pc_input = cpu.pc;
    }

    update_events(gui, cpu);
}

// ---------------- Inline (single-page) UI blocks ----------------

static void DrawNowExecuting(cpu::CPU& cpu, GuiState& gui) {
    ImGui::SeparatorText("Now executing");

    // Next fetch
    uint32_t raw = 0;
    cpu::DecodedInstr d{};
    bool ok = decode_at_pc(cpu, cpu.pc, raw, d);
    std::string next_asm = ok ? instr_to_asm(d) : std::string("<unmapped>");

    ImGui::Text("Cycle %llu | Committed %llu | CPI %.3f | PC 0x%08X", 
                (unsigned long long)cpu.stats.cycles,
                (unsigned long long)cpu.stats.committed_instructions,
                (cpu.stats.committed_instructions == 0) ? 0.0 : (double)cpu.stats.cycles / (double)cpu.stats.committed_instructions,
                cpu.pc);

    ImGui::Text("Next IF:  raw 0x%08X  %s", raw, next_asm.c_str());

    // Last commit summary
    if (cpu.last_commit.valid) {
        const auto& c = cpu.last_commit;
        cpu::DecodedInstr cd = cpu::decode(c.raw);
        std::string casm = instr_to_asm(cd);

        ImGui::Text("Last commit:  id %llu  pc 0x%08X  %s", 
                    (unsigned long long)c.instr_id,
                    c.pc,
                    casm.c_str());

        if (c.reg_write && c.rd != 0) {
            ImGui::SameLine();
            ImGui::Text(" | %s <= 0x%08X", reg_name(c.rd).c_str(), c.wb_data);
        }
        if (c.mem_is_store) {
            ImGui::Text("Store: [0x%08X] <= 0x%08X (%uB)", c.mem_addr, c.mem_store_data, (unsigned)c.mem_size);
        } else if (c.mem_is_load) {
            ImGui::Text("Load : [0x%08X] -> %s (size=%uB, %s)", c.mem_addr,
                        (c.reg_write && c.rd != 0) ? reg_name(c.rd).c_str() : "(no rd)",
                        (unsigned)c.mem_size,
                        c.mem_signed ? "signed" : "unsigned");
        }
    } else {
        ImGui::TextDisabled("Last commit: (none yet)");
    }

    // Per-cycle events
    ImGui::Text("Events last cycle:");
    ImGui::SameLine();
    if (gui.last_cycle_stall) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "STALL");
        ImGui::SameLine();
    }
    if (gui.last_cycle_mispredict) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "MISPREDICT/REDIRECT");
        ImGui::SameLine();
    }
    if (gui.last_cycle_commit) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "COMMIT");
    }
    if (!gui.last_cycle_stall && !gui.last_cycle_mispredict && !gui.last_cycle_commit) {
        ImGui::TextDisabled("(none)");
    }
}

static void DrawPipelineDiagram(cpu::CPU& cpu, GuiState& gui) {
    ImGui::SeparatorText("Pipeline");

    // Small controls
    ImGui::Checkbox("Auto-follow instruction", &gui.auto_follow_instr);
    ImGui::SameLine();
    ImGui::Text("Highlight id: %llu", (unsigned long long)gui.follow_instr_id);
    ImGui::SameLine();
    if (ImGui::Button("Follow EX")) {
        gui.auto_follow_instr = false;
        if (cpu.id_ex.valid) gui.follow_instr_id = cpu.id_ex.instr_id;
    }
    ImGui::SameLine();
    if (ImGui::Button("Follow last commit")) {
        gui.auto_follow_instr = false;
        if (cpu.last_commit.valid) gui.follow_instr_id = cpu.last_commit.instr_id;
    }

    const float box_h = 92.0f;
    const float gap = 10.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float box_w = (avail.x - 4.0f * gap) / 5.0f;
    if (box_w < 110.0f) box_w = 110.0f;

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    struct StageBox {
        const char* name;
        bool valid;
        uint64_t instr_id;
        uint32_t pc;
        uint32_t raw;
        cpu::DecodedInstr d;
        bool have_dec;
        // stage-specific extras
        bool predicted_taken;
        uint32_t predicted_target;
        // mem/wb extras
        uint32_t alu_result;
        bool mem_r;
        bool mem_w;
        uint32_t store_data;
        uint8_t rd;
        bool reg_write;
        uint32_t wb_data;
    };

    StageBox s[5]{};

    // IF: not latched; approximate as "next fetch"
    s[0].name = "IF";
    s[0].valid = cpu.mem.is_mapped(cpu.pc & ~0x3u, 4);
    s[0].instr_id = cpu.next_instr_id;
    s[0].pc = cpu.pc;
    s[0].have_dec = decode_at_pc(cpu, cpu.pc, s[0].raw, s[0].d);

    // ID
    s[1].name = "ID";
    s[1].valid = cpu.if_id.valid;
    s[1].instr_id = cpu.if_id.instr_id;
    s[1].pc = cpu.if_id.pc;
    s[1].raw = cpu.if_id.instr;
    s[1].d = cpu::decode(cpu.if_id.instr);
    s[1].have_dec = cpu.if_id.valid;
    s[1].predicted_taken = cpu.if_id.predicted_taken;
    s[1].predicted_target = cpu.if_id.predicted_target;

    // EX
    s[2].name = "EX";
    s[2].valid = cpu.id_ex.valid;
    s[2].instr_id = cpu.id_ex.instr_id;
    s[2].pc = cpu.id_ex.pc;
    s[2].raw = cpu.id_ex.dinstr.raw;
    s[2].d = cpu.id_ex.dinstr;
    s[2].have_dec = cpu.id_ex.valid;
    s[2].predicted_taken = cpu.id_ex.predicted_taken;
    s[2].predicted_target = cpu.id_ex.predicted_target;

    // MEM
    s[3].name = "MEM";
    s[3].valid = cpu.ex_mem.valid;
    s[3].instr_id = cpu.ex_mem.instr_id;
    s[3].pc = cpu.ex_mem.pc;
    s[3].raw = cpu.ex_mem.raw;
    s[3].d = cpu::decode(cpu.ex_mem.raw);
    s[3].have_dec = cpu.ex_mem.valid;
    s[3].alu_result = cpu.ex_mem.alu_result;
    s[3].mem_r = cpu.ex_mem.mem_read;
    s[3].mem_w = cpu.ex_mem.mem_write;
    s[3].store_data = cpu.ex_mem.store_data;
    s[3].rd = cpu.ex_mem.rd;
    s[3].reg_write = cpu.ex_mem.reg_write;

    // WB
    s[4].name = "WB";
    s[4].valid = cpu.mem_wb.valid;
    s[4].instr_id = cpu.mem_wb.instr_id;
    s[4].pc = cpu.mem_wb.pc;
    s[4].raw = cpu.mem_wb.raw;
    s[4].d = cpu::decode(cpu.mem_wb.raw);
    s[4].have_dec = cpu.mem_wb.valid;
    s[4].rd = cpu.mem_wb.rd;
    s[4].reg_write = cpu.mem_wb.reg_write;
    s[4].wb_data = cpu.mem_wb.wb_data;

    auto draw_stage = [&](int idx) {
        StageBox& st = s[idx];
        ImVec2 a(p0.x + idx * (box_w + gap), p0.y);
        ImVec2 b(a.x + box_w, a.y + box_h);

        bool highlight = st.valid && (st.instr_id != 0) && (st.instr_id == gui.follow_instr_id);
        ImU32 border = highlight ? IM_COL32(255, 220, 90, 255) : IM_COL32(90, 90, 100, 255);
        ImU32 bg     = st.valid ? IM_COL32(30, 30, 36, 255) : IM_COL32(22, 22, 26, 255);
        float rounding = 8.0f;
        dl->AddRectFilled(a, b, bg, rounding);
        dl->AddRect(a, b, border, rounding, 0, highlight ? 3.0f : 1.5f);

        // Stage header
        ImVec2 t(a.x + 8, a.y + 6);
        dl->AddText(t, IM_COL32(220, 220, 230, 255), st.name);

        // Body
        float y = a.y + 26.0f;
        if (st.valid && st.have_dec) {
            char buf1[96];
            std::snprintf(buf1, sizeof(buf1), "id %llu", (unsigned long long)st.instr_id);
            dl->AddText(ImVec2(a.x + 8, y), IM_COL32(180, 180, 190, 255), buf1);
            y += 16.0f;
            char buf2[96];
            std::snprintf(buf2, sizeof(buf2), "pc 0x%08X", st.pc);
            dl->AddText(ImVec2(a.x + 8, y), IM_COL32(180, 180, 190, 255), buf2);
            y += 16.0f;

            std::string asm_s = instr_to_asm(st.d);
            if (asm_s.size() > 24) asm_s = asm_s.substr(0, 24) + "…";
            dl->AddText(ImVec2(a.x + 8, y), IM_COL32(220, 220, 230, 255), asm_s.c_str());
            y += 16.0f;

            // Tiny stage-specific hint line
            if (idx == 1 || idx == 2) {
                if (st.d.is_branch) {
                    char bb[128];
                    std::snprintf(bb, sizeof(bb), "pred %s → 0x%08X", st.predicted_taken ? "T" : "NT", st.predicted_target);
                    dl->AddText(ImVec2(a.x + 8, y), IM_COL32(160, 190, 255, 255), bb);
                }
            } else if (idx == 3) {
                if (st.mem_r || st.mem_w) {
                    char mm[128];
                    std::snprintf(mm, sizeof(mm), "%s 0x%08X", st.mem_r ? "load" : "store", st.alu_result);
                    dl->AddText(ImVec2(a.x + 8, y), IM_COL32(200, 170, 255, 255), mm);
                }
            } else if (idx == 4) {
                if (st.reg_write && st.rd != 0) {
                    char ww[128];
                    std::snprintf(ww, sizeof(ww), "%s<=0x%08X", reg_name(st.rd).c_str(), st.wb_data);
                    dl->AddText(ImVec2(a.x + 8, y), IM_COL32(140, 255, 170, 255), ww);
                }
            }
        } else {
            dl->AddText(ImVec2(a.x + 8, a.y + 34), IM_COL32(120, 120, 130, 255), "(bubble)");
        }

        // Click target
        ImGui::SetCursorScreenPos(a);
        ImGui::InvisibleButton((std::string("stage_") + st.name).c_str(), ImVec2(box_w, box_h));
        if (ImGui::IsItemClicked()) {
            gui.selected_stage = idx;
            if (st.valid) {
                gui.auto_follow_instr = false;
                gui.follow_instr_id = st.instr_id;
            }
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && st.valid && st.have_dec) {
            ImGui::BeginTooltip();
            ImGui::Text("%s stage", st.name);
            ImGui::Separator();
            ImGui::Text("id: %llu", (unsigned long long)st.instr_id);
            ImGui::Text("pc: 0x%08X", st.pc);
            ImGui::Text("raw: 0x%08X", st.raw);
            ImGui::Text("asm: %s", instr_to_asm(st.d).c_str());
            ImGui::Separator();
            ImGui::Text("kind: %s", instr_kind_name(st.d.kind));
            ImGui::Text("rs1=%d  rs2=%d  rd=%d", st.d.rs1, st.d.rs2, st.d.rd);
            ImGui::Text("imm=%d  fmt=%s", (int)st.d.imm, imm_format_from_opcode(st.raw));
            if (idx == 1 || idx == 2) {
                if (st.d.is_branch) {
                    ImGui::Text("pred: %s  target: 0x%08X", st.predicted_taken ? "taken" : "not-taken", st.predicted_target);
                }
            }
            if (idx == 3) {
                ImGui::Text("alu/addr: 0x%08X", st.alu_result);
                ImGui::Text("mem_read=%d mem_write=%d", st.mem_r ? 1 : 0, st.mem_w ? 1 : 0);
                if (st.mem_w) ImGui::Text("store_data: 0x%08X", st.store_data);
            }
            if (idx == 4) {
                ImGui::Text("reg_write=%d", st.reg_write ? 1 : 0);
                if (st.reg_write) ImGui::Text("wb: %s <= 0x%08X", reg_name(st.rd).c_str(), st.wb_data);
            }
            ImGui::EndTooltip();
        }
    };

    // Draw arrows between stages
    for (int i = 0; i < 4; ++i) {
        ImVec2 a(p0.x + i * (box_w + gap) + box_w, p0.y + box_h * 0.5f);
        ImVec2 b(p0.x + (i + 1) * (box_w + gap), p0.y + box_h * 0.5f);
        dl->AddLine(a, b, IM_COL32(120, 120, 130, 255), 2.0f);
        // Arrow head
        dl->AddTriangleFilled(ImVec2(b.x - 8, b.y - 5), ImVec2(b.x - 8, b.y + 5), ImVec2(b.x, b.y), IM_COL32(120, 120, 130, 255));
    }

    // Draw boxes
    for (int i = 0; i < 5; ++i) draw_stage(i);

    // Reserve space
    ImGui::Dummy(ImVec2(0, box_h + 8.0f));
}

static void DrawInstructionLens(cpu::CPU& cpu, GuiState& gui) {
    ImGui::SeparatorText("Instruction lens");

    // Stage selector
    const char* stages[] = {"IF", "ID", "EX", "MEM", "WB"};
    ImGui::Text("Selected stage:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::Combo("##stage_sel", &gui.selected_stage, stages, IM_ARRAYSIZE(stages));

    // Extract selected-stage data
    bool valid = false;
    uint64_t instr_id = 0;
    uint32_t pc = 0;
    uint32_t raw = 0;
    cpu::DecodedInstr d{};
    uint32_t rs1_val = 0, rs2_val = 0;
    bool have_rs_vals = false;
    bool pred_taken = false;
    uint32_t pred_target = 0;

    switch (gui.selected_stage) {
    case 0: // IF
        valid = decode_at_pc(cpu, cpu.pc, raw, d);
        instr_id = cpu.next_instr_id;
        pc = cpu.pc;
        break;
    case 1: // ID
        valid = cpu.if_id.valid;
        instr_id = cpu.if_id.instr_id;
        pc = cpu.if_id.pc;
        raw = cpu.if_id.instr;
        d = cpu::decode(raw);
        pred_taken = cpu.if_id.predicted_taken;
        pred_target = cpu.if_id.predicted_target;
        break;
    case 2: // EX
        valid = cpu.id_ex.valid;
        instr_id = cpu.id_ex.instr_id;
        pc = cpu.id_ex.pc;
        raw = cpu.id_ex.dinstr.raw;
        d = cpu.id_ex.dinstr;
        rs1_val = cpu.id_ex.rs1_val;
        rs2_val = cpu.id_ex.rs2_val;
        have_rs_vals = true;
        pred_taken = cpu.id_ex.predicted_taken;
        pred_target = cpu.id_ex.predicted_target;
        break;
    case 3: // MEM
        valid = cpu.ex_mem.valid;
        instr_id = cpu.ex_mem.instr_id;
        pc = cpu.ex_mem.pc;
        raw = cpu.ex_mem.raw;
        d = cpu::decode(raw);
        break;
    case 4: // WB
        valid = cpu.mem_wb.valid;
        instr_id = cpu.mem_wb.instr_id;
        pc = cpu.mem_wb.pc;
        raw = cpu.mem_wb.raw;
        d = cpu::decode(raw);
        break;
    default:
        break;
    }

    if (!valid) {
        ImGui::TextDisabled("(bubble / no valid instruction in this stage)");
        return;
    }

    // Summary
    ImGui::Text("id %llu | pc 0x%08X | raw 0x%08X", (unsigned long long)instr_id, pc, raw);
    ImGui::Text("asm: %s", instr_to_asm(d).c_str());

    // Decode details
    ImGui::Separator();
    ImGui::Text("Decode");
    uint32_t opcode = raw & 0x7Fu;
    uint32_t funct3 = (raw >> 12) & 0x7u;
    uint32_t funct7 = (raw >> 25) & 0x7Fu;

    if (ImGui::BeginTable("lens_decode", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("kind");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%s", instr_kind_name(d.kind));
        ImGui::TableSetColumnIndex(2); ImGui::Text("format");
        ImGui::TableSetColumnIndex(3); ImGui::Text("%s", imm_format_from_opcode(raw));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("opcode");
        ImGui::TableSetColumnIndex(1); ImGui::Text("0x%02X", (unsigned)opcode);
        ImGui::TableSetColumnIndex(2); ImGui::Text("funct3");
        ImGui::TableSetColumnIndex(3); ImGui::Text("0x%X", (unsigned)funct3);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("funct7");
        ImGui::TableSetColumnIndex(1); ImGui::Text("0x%02X", (unsigned)funct7);
        ImGui::TableSetColumnIndex(2); ImGui::Text("imm");
        ImGui::TableSetColumnIndex(3); ImGui::Text("%d", (int)d.imm);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("rs1");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%s = 0x%08X", reg_name(d.rs1).c_str(), cpu.regs[d.rs1]);
        ImGui::TableSetColumnIndex(2); ImGui::Text("rs2");
        ImGui::TableSetColumnIndex(3); ImGui::Text("%s = 0x%08X", reg_name(d.rs2).c_str(), cpu.regs[d.rs2]);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("rd");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%s", reg_name(d.rd).c_str());
        ImGui::TableSetColumnIndex(2); ImGui::Text("writes_rd");
        ImGui::TableSetColumnIndex(3); ImGui::Text("%s", d.writes_rd ? "yes" : "no");

        ImGui::EndTable();
    }

    // Control / side effects
    ImGui::Separator();
    ImGui::Text("Effects");
    ImGui::BulletText("mem_read=%d  mem_write=%d  mem_size=%u  mem_signed=%d  mem_to_reg=%d",
                      d.mem_read ? 1 : 0, d.mem_write ? 1 : 0, (unsigned)d.mem_size, d.mem_signed ? 1 : 0, d.mem_to_reg ? 1 : 0);
    ImGui::BulletText("is_branch=%d  is_jump=%d", d.is_branch ? 1 : 0, d.is_jump ? 1 : 0);

    // Dataflow illustration (most meaningful in EX)
    if (gui.selected_stage == 2 && have_rs_vals) {
        ImGui::Separator();
        ImGui::Text("EX-stage dataflow (using latched operands)");
        ImGui::BulletText("op1 (rs1) = 0x%08X", rs1_val);
        ImGui::BulletText("op2 (rs2) = 0x%08X", rs2_val);

        if (d.mem_read || d.mem_write) {
            uint32_t ea = rs1_val + (uint32_t)d.imm;
            ImGui::BulletText("effective addr = op1 + imm = 0x%08X", ea);
        }

        if (d.is_branch) {
            bool actual = branch_taken(d.kind, rs1_val, rs2_val);
            uint32_t fall_through = pc + 4u;
            uint32_t target = pc + (uint32_t)d.imm;
            bool mispredict = false;
            if (pred_taken) mispredict = (!actual) || (pred_target != target);
            else            mispredict = actual;

            ImGui::BulletText("predicted: %s  target 0x%08X", pred_taken ? "taken" : "not-taken", pred_target);
            ImGui::BulletText("actual   : %s  target 0x%08X  fall-through 0x%08X", actual ? "taken" : "not-taken", target, fall_through);
            if (mispredict) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "MISPREDICT → redirect to 0x%08X", actual ? target : fall_through);
            } else {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Prediction correct");
            }
        }
    }
}

static void DrawDisasmInline(cpu::CPU& cpu, GuiState& gui, float height) {
    ImGui::Text("Disassembly");
    ImGui::SameLine();
    ImGui::Checkbox("Follow PC", &gui.follow_pc);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputScalar("View PC##view_pc", ImGuiDataType_U32,
                       &gui.view_pc_input, nullptr, nullptr, "%08X",
                       ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    if (ImGui::Button("Go")) {
        gui.follow_pc = false;
        gui.disasm_base_pc = clamp_disasm_base(cpu, gui.view_pc_input);
    }

    ImGui::SameLine();
    ImGui::Text(" | Breakpoint:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    ImGui::InputScalar("##bp_pc", ImGuiDataType_U32, &gui.break_pc, nullptr, nullptr, "%08X",
                       ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::Checkbox("Enable##bp_en", &gui.break_enabled);
    ImGui::SameLine();
    if (ImGui::Button("Clear BP")) {
        gui.break_enabled = false;
        gui.breakpoint_hit = false;
    }
    if (gui.breakpoint_hit) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "HIT");
    }

    ImGui::BeginChild("disasm_child", ImVec2(0, height), false, ImGuiWindowFlags_HorizontalScrollbar);

    const uint32_t prog_min = cpu.mem.prog_min;
    const uint32_t prog_max = cpu.mem.prog_max;
    if (prog_max <= prog_min) {
        ImGui::TextDisabled("(no program loaded)");
        ImGui::EndChild();
        return;
    }

    uint32_t pc = clamp_disasm_base(cpu, gui.disasm_base_pc);
    const int rows = 70;
    for (int i = 0; i < rows; ++i) {
        if (pc < prog_min || pc >= prog_max) break;

        uint32_t raw = cpu::load_u32(cpu.mem, pc);
        cpu::DecodedInstr d = cpu::decode(raw);
        std::string asm_str = instr_to_asm(d);

        // Optional symbol label
        auto it = gui.symbols.find(pc);
        if (it != gui.symbols.end()) {
            ImGui::TextDisabled("%s:", it->second.c_str());
        }

        bool is_cur = (pc == cpu.pc);
        bool is_bp  = gui.break_enabled && (pc == gui.break_pc);

        std::ostringstream line;
        line << (is_bp ? "● " : "  ");
        line << "0x" << std::hex << std::setw(8) << std::setfill('0') << pc
             << "  0x" << std::setw(8) << raw
             << std::dec << "  " << asm_str;

        if (is_cur) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));

        bool selected = is_cur;
        if (ImGui::Selectable(line.str().c_str(), selected)) {
            gui.break_pc = pc;
            gui.break_enabled = true;
            gui.breakpoint_hit = false;
        }

        if (is_cur) ImGui::PopStyleColor();

        pc += 4;
    }

    ImGui::EndChild();
}

static void DrawRegistersAndWatchesInline(cpu::CPU& cpu, GuiState& gui, float height) {
    ImGui::Text("Registers & Watches");
    ImGui::BeginChild("regs_child", ImVec2(0, height), false);

    auto draw_reg_line = [&](int r, const char* label_override = nullptr) {
        float h = gui.reg_highlight[r];
        if (h > 0.0f) {
            ImU32 col = ImGui::GetColorU32(ImVec4(1.0f - 0.5f * h, 1.0f, 1.0f - h, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, col);
        }
        if (label_override) {
            ImGui::Text("%-4s %s = 0x%08X", label_override, reg_name(r).c_str(), cpu.regs[r]);
        } else {
            ImGui::Text("%s = 0x%08X", reg_name(r).c_str(), cpu.regs[r]);
        }
        if (h > 0.0f) ImGui::PopStyleColor();
    };

    // Common set
    ImGui::Text("PC  = 0x%08X", cpu.pc);
    draw_reg_line(1,  "ra");
    draw_reg_line(2,  "sp");
    draw_reg_line(3,  "gp");
    draw_reg_line(4,  "tp");
    for (int r = 10; r <= 17; ++r) {
        char lab[8];
        std::snprintf(lab, sizeof(lab), "a%d", r - 10);
        draw_reg_line(r, lab);
    }

    if (ImGui::CollapsingHeader("All x0-x31", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("all_regs", 2, ImGuiTableFlags_SizingFixedFit)) {
            for (int i = 0; i < 32; ++i) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("x%-2d", i);
                ImGui::TableSetColumnIndex(1);
                float h = gui.reg_highlight[i];
                if (h > 0.0f) {
                    ImU32 col = ImGui::GetColorU32(ImVec4(1.0f - 0.5f * h, 1.0f, 1.0f - h, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, col);
                    ImGui::Text("0x%08X", cpu.regs[i]);
                    ImGui::PopStyleColor();
                } else {
                    ImGui::Text("0x%08X", cpu.regs[i]);
                }
            }
            ImGui::EndTable();
        }
    }

    ImGui::Separator();
    ImGui::Text("Watches (enter: x5 or 0x1000)");
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputText("##watch_input", gui.watch.input_buf, sizeof(gui.watch.input_buf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string s(gui.watch.input_buf);
        s.erase(0, s.find_first_not_of(" \t\r\n"));
        s.erase(s.find_last_not_of(" \t\r\n") + 1);
        if (!s.empty()) {
            WatchItem item{};
            if (s[0] == 'x' || s[0] == 'X') {
                int reg_num = std::stoi(s.substr(1), nullptr, 10);
                if (reg_num >= 0 && reg_num < 32) {
                    item.kind = WatchKind::Reg;
                    item.reg  = reg_num;
                    gui.watch.items.push_back(item);
                }
            } else {
                uint32_t addr = 0;
                if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) addr = (uint32_t)std::stoul(s, nullptr, 16);
                else addr = (uint32_t)std::stoul(s, nullptr, 10);
                item.kind = WatchKind::Mem;
                item.addr = addr;
                gui.watch.items.push_back(item);
            }
        }
        gui.watch.input_buf[0] = '\0';
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear##watch_clear")) {
        gui.watch.items.clear();
    }

    for (std::size_t i = 0; i < gui.watch.items.size(); ++i) {
        const auto& w = gui.watch.items[i];
        if (w.kind == WatchKind::Reg) {
            uint32_t val = cpu.regs[w.reg];
            ImGui::Text("%s = 0x%08X", reg_name((uint8_t)w.reg).c_str(), val);
        } else {
            uint32_t addr = w.addr;
            bool in_range = cpu.mem.is_mapped(addr, 4);
            uint32_t val = in_range ? cpu::load_u32(cpu.mem, addr & ~0x3u) : 0u;
            ImGui::Text("[0x%08X] = %s0x%08X%s", addr, in_range ? "" : "<", val, in_range ? "" : " (out)");
        }
    }

    ImGui::EndChild();
}

static void DrawMemoryInline(cpu::CPU& cpu, GuiState& gui, float height) {
    ImGui::SeparatorText("Memory");
    ImGui::Checkbox("Auto-follow last access", &gui.auto_follow_mem);
    ImGui::SameLine();
    ImGui::Text("Last access: 0x%08X", gui.last_mem_addr);

    ImGui::Text("Base address (hex):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputScalar("##mem_base", ImGuiDataType_U32,
                       &gui.mem_base, nullptr, nullptr, "%08X",
                       ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    if (ImGui::Button("Focus last")) {
        gui.mem_base = gui.last_mem_addr & ~0xFu;
    }

    const int rows = 16;
    const int words_per_row = 4;
    uint32_t base_aligned = gui.mem_base & ~0xFu;

    ImGui::BeginChild("mem_child", ImVec2(0, height), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (int row = 0; row < rows; ++row) {
        uint32_t addr = base_aligned + (uint32_t)row * 16u;
        bool row_has_last = (gui.last_mem_highlight > 0.0f) && (gui.last_mem_addr >= addr) && (gui.last_mem_addr < addr + 16);
        if (row_has_last) {
            ImU32 col = ImGui::GetColorU32(ImVec4(1.0f, 1.0f - 0.5f * gui.last_mem_highlight, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, col);
        }

        ImGui::Text("0x%08X:", addr);
        ImGui::SameLine();

        for (int w = 0; w < words_per_row; ++w) {
            uint32_t a = addr + (uint32_t)w * 4u;
            ImGui::SameLine(0.0f, 12.0f);
            if (cpu.mem.is_mapped(a, 4)) {
                uint32_t v = cpu::load_u32(cpu.mem, a);
                ImGui::Text("%08X", v);
            } else {
                ImGui::Text("--------");
            }
        }

        if (row_has_last) {
            ImGui::PopStyleColor();
        }
    }

    ImGui::EndChild();
}

static void DrawUartInline(cpu::CPU& cpu, float height) {
    ImGui::SeparatorText("UART / Log");
    ImGui::Text("MMIO UART TX @ 0x%08X", cpu::CPU::UART_BASE);
    ImGui::SameLine();
    if (ImGui::Button("Clear UART")) {
        cpu.uart_buffer.clear();
    }

    static std::size_t last_len = 0;
    bool grew = cpu.uart_buffer.size() > last_len;
    last_len = cpu.uart_buffer.size();

    ImGui::BeginChild("uart_child", ImVec2(0, height), false);
    ImGui::TextUnformatted(cpu.uart_buffer.c_str());
    if (grew) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

static void DrawDashboard(cpu::CPU& cpu,
                          const cpu::Memory& loaded_mem,
                          uint32_t entry,
                          const std::unordered_map<uint32_t, std::string>& symbols,
                          bool& running,
                          int& cycles_per_frame,
                          CpiHistory& cpi_hist,
                          GuiState& gui)
{
    ImGui::Begin("RV32 Debugger Dashboard", nullptr, ImGuiWindowFlags_NoCollapse);

    // ---------- Control bar (pinned) ----------
    bool request_reset = false;
    bool request_step_cycle = false;
    bool request_step_commit = false;

    if (ImGui::Button(running ? "Pause" : "Run")) {
        running = !running;
        gui.breakpoint_hit = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step cycle")) {
        request_step_cycle = true;
        gui.breakpoint_hit = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step commit")) {
        request_step_commit = true;
        gui.breakpoint_hit = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        request_reset = true;
        gui.breakpoint_hit = false;
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::SliderInt("Cycles/frame", &cycles_per_frame, 1, 2000);
    ImGui::SameLine();
    ImGui::Text("Halted: %s", cpu.halted ? "yes" : "no");

    // ---------- Apply actions BEFORE drawing the rest (so it feels instant) ----------
    if (request_reset) {
        cpu = cpu::CPU{};
        cpu.mem = loaded_mem;
        cpu.pc = entry;
        cpu.uart_buffer.clear();
        running = false;
        cpi_hist.clear();
        reset_gui_state(gui, cpu, symbols, entry);
        update_events(gui, cpu);
    } else {
        if (!cpu.halted) {
            if (request_step_cycle) {
                cpu.step();
            } else if (request_step_commit) {
                (void)step_until_commit(cpu);
            } else if (running) {
                for (int i = 0; i < cycles_per_frame && !cpu.halted; ++i) {
                    cpu.step();
                    if (gui.break_enabled && cpu.pc == gui.break_pc) break;
                }
            }
        }

        // Breakpoint check
        if (gui.break_enabled && cpu.pc == gui.break_pc && !cpu.halted) {
            running = false;
            gui.breakpoint_hit = true;
        }

        update_after_stepping(cpu, gui, cpi_hist);
    }

    ImGui::Separator();

    // ---------- Scrollable content ----------
    ImGui::BeginChild("dashboard_scroll", ImVec2(0, 0), false);

    DrawNowExecuting(cpu, gui);
    DrawPipelineDiagram(cpu, gui);
    DrawInstructionLens(cpu, gui);

    ImGui::SeparatorText("Views");
    if (ImGui::BeginTable("views_split", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("Disasm", ImGuiTableColumnFlags_WidthStretch, 0.65f);
        ImGui::TableSetupColumn("Regs", ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        DrawDisasmInline(cpu, gui, 520.0f);
        ImGui::TableSetColumnIndex(1);
        DrawRegistersAndWatchesInline(cpu, gui, 520.0f);
        ImGui::EndTable();
    }

    DrawMemoryInline(cpu, gui, 260.0f);
    DrawUartInline(cpu, 200.0f);

    ImGui::EndChild();
    ImGui::End();
}

// ---------- Helper: categorize instruction for perf/heatmap ----------

static void categorize_for_perf(const cpu::DecodedInstr& d, PerfStats& perf) {
    using cpu::InstrKind;
    switch (d.kind) {
    case InstrKind::ADD:
    case InstrKind::SUB:
    case InstrKind::ADDI:
    case InstrKind::LUI:
        perf.alu++;
        break;
    case InstrKind::LW:
    case InstrKind::SW:
        perf.mem++;
        break;
    case InstrKind::BEQ:
    case InstrKind::BNE:
    case InstrKind::JAL:
    case InstrKind::JALR:
        perf.branch++;
        break;
    default:
        perf.other++;
        break;
    }
}

// ---------- ImGui windows ----------

static void DrawRegistersWindow(cpu::CPU& cpu, const GuiState& gui) {
    if (!ImGui::Begin("Registers")) {
        ImGui::End();
        return;
    }

    ImGui::Text("PC: 0x%08x", cpu.pc);
    ImGui::Separator();

    if (ImGui::BeginTable("regs_table", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        for (int i = 0; i < 32; ++i) {
            ImGui::TableNextColumn();

            float h = gui.reg_highlight[i];
            if (h > 0.0f) {
                // fade: 0 -> normal, 1 -> bright green
                ImU32 col = ImGui::GetColorU32(
                    ImVec4(1.0f - 0.5f * h, 1.0f, 1.0f - h, 1.0f)); // greenish/yellowish
                ImGui::PushStyleColor(ImGuiCol_Text, col);
                ImGui::Text("x%-2d = 0x%08x", i, cpu.regs[i]);
                ImGui::PopStyleColor();
            } else {
                ImGui::Text("x%-2d = 0x%08x", i, cpu.regs[i]);
            }
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

static void DrawPipelineWindow(cpu::CPU& cpu) {
    if (!ImGui::Begin("Pipeline")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Cycle: %llu",
                (unsigned long long)cpu.stats.cycles);
    ImGui::Separator();

    // IF
    ImGui::Text("IF : pc=0x%08x", cpu.pc);

    // ID
    ImGui::Separator();
    ImGui::Text("ID :");
    if (cpu.if_id.valid) {
        auto d = cpu::decode(cpu.if_id.instr);
        ImGui::BulletText("id=%llu  pc=0x%08x  instr=0x%08x  %s",
                          (unsigned long long)cpu.if_id.instr_id,
                          cpu.if_id.pc,
                          cpu.if_id.instr,
                          instr_to_asm(d).c_str());
    } else {
        ImGui::TextDisabled("  (bubble)");
    }

    // EX
    ImGui::Separator();
    ImGui::Text("EX :");
    if (cpu.id_ex.valid) {
        const auto& d = cpu.id_ex.dinstr;
        ImGui::BulletText("id=%llu  pc=0x%08x  %s",
                          (unsigned long long)cpu.id_ex.instr_id,
                          cpu.id_ex.pc,
                          instr_to_asm(d).c_str());
    } else {
        ImGui::TextDisabled("  (bubble)");
    }

    // MEM
    ImGui::Separator();
    ImGui::Text("MEM:");
    if (cpu.ex_mem.valid) {
        auto d = cpu::decode(cpu.ex_mem.raw);
        ImGui::BulletText("id=%llu  pc=0x%08x  raw=0x%08x  alu=0x%08x  rd=%d  mem_read=%d  mem_write=%d",
                          (unsigned long long)cpu.ex_mem.instr_id,
                          cpu.ex_mem.pc,
                          cpu.ex_mem.raw,
                          cpu.ex_mem.alu_result,
                          (int)cpu.ex_mem.rd,
                          cpu.ex_mem.mem_read,
                          cpu.ex_mem.mem_write);
        ImGui::Text("    %s", instr_to_asm(d).c_str());
    } else {
        ImGui::TextDisabled("  (bubble)");
    }

    // WB
    ImGui::Separator();
    ImGui::Text("WB :");
    if (cpu.mem_wb.valid) {
        auto d = cpu::decode(cpu.mem_wb.raw);
        ImGui::BulletText("id=%llu  pc=0x%08x  raw=0x%08x  rd=%d  data=0x%08x",
                          (unsigned long long)cpu.mem_wb.instr_id,
                          cpu.mem_wb.pc,
                          cpu.mem_wb.raw,
                          (int)cpu.mem_wb.rd,
                          cpu.mem_wb.wb_data);
        ImGui::Text("    %s", instr_to_asm(d).c_str());
    } else {
        ImGui::TextDisabled("  (bubble)");
    }

    ImGui::End();
}

static void DrawStatsWindow(cpu::CPU& cpu, CpiHistory& hist) {
    if (!ImGui::Begin("Stats")) {
        ImGui::End();
        return;
    }

    double cpi = (cpu.stats.committed_instructions == 0)
        ? 0.0
        : (double)cpu.stats.cycles /
          (double)cpu.stats.committed_instructions;

    ImGui::Text("Cycles:           %llu",
                (unsigned long long)cpu.stats.cycles);
    ImGui::Text("Committed instrs: %llu",
                (unsigned long long)cpu.stats.committed_instructions);
    ImGui::Text("Stall cycles:     %llu",
                (unsigned long long)cpu.stats.stall_cycles);
    ImGui::Text("CPI:              %.3f", cpi);
    ImGui::Separator();
    ImGui::Text("Branches:         %llu",
                (unsigned long long)cpu.stats.branch_instructions);
    ImGui::Text("Predictions:      %llu",
                (unsigned long long)cpu.stats.branch_predictions);
    ImGui::Text("Mispredicts:      %llu",
                (unsigned long long)cpu.stats.branch_mispredictions);
    double mis_rate = (cpu.stats.branch_predictions == 0)
        ? 0.0
        : (double)cpu.stats.branch_mispredictions /
          (double)cpu.stats.branch_predictions;
    ImGui::Text("Mispredict rate:  %.3f", mis_rate);

    ImGui::Separator();
    ImGui::Text("CPI vs Cycles:");

    if (!hist.x.empty()) {
        ImVec2 size(-1.0f, 200.0f); // full width, 200 px tall

        if (ImPlot::BeginPlot("CPI Plot", size)) {
            ImPlot::SetupAxes("Cycles", "CPI",
                              ImPlotAxisFlags_AutoFit,
                              ImPlotAxisFlags_AutoFit);
            ImPlot::PlotLine("CPI",
                             hist.x.data(),
                             hist.y.data(),
                             (int)hist.x.size());
            ImPlot::EndPlot();
        }
    } else {
        ImGui::TextDisabled("(no CPI data yet; run the CPU)");
    }

    ImGui::End();
}

// Simple scrollable memory viewer for dmem (with last-access highlight)
static void DrawMemoryWindow(cpu::CPU& cpu, GuiState& gui) {
    if (!ImGui::Begin("Memory")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Data memory viewer");
    ImGui::Separator();

    // Base address input (hex)
    ImGui::Text("Base address (byte, hex):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputScalar("##mem_base", ImGuiDataType_U32,
                       &gui.mem_base, nullptr, nullptr, "%08X",
                       ImGuiInputTextFlags_CharsHexadecimal);

    ImGui::Separator();

    // dmem is virtually mapped; use cpu.mem.is_mapped() for bounds.

    // Show 16 rows * 4 words = 64 words = 256 bytes
    const int rows = 16;
    const int words_per_row = 4;

    uint32_t base_aligned = gui.mem_base & ~0xFu; // align to 16 bytes

    ImGui::BeginChild("mem_scroll_region", ImVec2(0, 260), true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    for (int row = 0; row < rows; ++row) {
        uint32_t addr = base_aligned + static_cast<uint32_t>(row) * 16u; // 16 bytes per row

        bool row_has_last =
            (gui.last_mem_highlight > 0.0f) &&
            (gui.last_mem_addr >= addr) &&
            (gui.last_mem_addr < addr + 16);

        if (row_has_last) {
            ImU32 col = ImGui::GetColorU32(
                ImVec4(1.0f, 1.0f - 0.5f * gui.last_mem_highlight,
                       0.0f, 1.0f)); // orange-ish
            ImGui::PushStyleColor(ImGuiCol_Text, col);
        }

        ImGui::Text("0x%08X:", addr);
        ImGui::SameLine();

        for (int w = 0; w < words_per_row; ++w) {
            uint32_t a = addr + static_cast<uint32_t>(w) * 4u;
            ImGui::SameLine(0.0f, 12.0f);

            if (cpu.mem.is_mapped(a, 4)) {
                uint32_t v = cpu::load_u32(cpu.mem, a);
                ImGui::Text("%08X", v);
            } else {
                ImGui::Text("--------");
            }
        }

        if (row_has_last) {
            ImGui::PopStyleColor();
        }
    }

    ImGui::EndChild();

    // Fade out highlight
    if (gui.last_mem_highlight > 0.0f) {
        gui.last_mem_highlight -= 0.02f;
        if (gui.last_mem_highlight < 0.0f) gui.last_mem_highlight = 0.0f;
    }

    ImGui::End();
}

// Simple memory editor: read/write a single word
static void DrawMemoryEditorWindow(cpu::CPU& cpu, GuiState& gui) {
    if (!ImGui::Begin("Memory Editor")) {
        ImGui::End();
        return;
    }

    // dmem is virtually mapped; use cpu.mem.is_mapped() for bounds.

    static uint32_t edit_addr = 0;
    static uint32_t edit_value = 0;

    ImGui::Text("Edit a single 32-bit word in dmem (byte-addressed, little-endian)");
    ImGui::Separator();

    ImGui::Text("Address (byte, hex):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputScalar("##edit_addr", ImGuiDataType_U32,
                       &edit_addr, nullptr, nullptr, "%08X",
                       ImGuiInputTextFlags_CharsHexadecimal);

    uint32_t aligned = edit_addr & ~0x3u;

    bool in_range = cpu.mem.is_mapped(aligned, 4);

    if (ImGui::Button("Read")) {
        if (in_range) {
            edit_value = cpu::load_u32(cpu.mem, aligned);
        } else {
            edit_value = 0;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Write")) {
        if (in_range) {
            cpu::store_u32(cpu.mem, aligned, edit_value);
            gui.last_mem_addr = aligned;
            gui.last_mem_highlight = 1.0f;
        }
    }

    ImGui::Separator();

    ImGui::Text("Aligned address: 0x%08X  (%s)", aligned, in_range ? "in range" : "out of range");
    ImGui::Text("Value (hex):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputScalar("##edit_value", ImGuiDataType_U32,
                       &edit_value, nullptr, nullptr, "%08X",
                       ImGuiInputTextFlags_CharsHexadecimal);

    ImGui::End();
}

// Disassembly view of imem with PC highlight + click-to-break
static void DrawDisasmWindow(cpu::CPU& cpu, GuiState& gui) {
    if (!ImGui::Begin("Disassembly")) {
        ImGui::End();
        return;
    }

    const uint32_t prog_min = cpu.mem.prog_min;
    const uint32_t prog_max = cpu.mem.prog_max;
    const uint32_t span = (prog_max > prog_min) ? (prog_max - prog_min) : 0u;
    const std::size_t instr_count = span / 4u;

    ImGui::Text("Instruction memory (disasm)");
    ImGui::Separator();

    // Base PC to show (hex)
    ImGui::Text("Base PC (byte, hex):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputScalar("##disasm_base", ImGuiDataType_U32,
                       &gui.disasm_base_pc, nullptr, nullptr,
                       "%08X", ImGuiInputTextFlags_CharsHexadecimal);

    // Clamp to available range
    if (instr_count > 0) {
        uint32_t min_pc = prog_min;
        uint32_t max_pc = prog_min + static_cast<uint32_t>((instr_count - 1) * 4u);
        if (gui.disasm_base_pc < min_pc) gui.disasm_base_pc = min_pc;
        if (gui.disasm_base_pc > max_pc) gui.disasm_base_pc = max_pc & ~0x3u;
    } else {
        gui.disasm_base_pc = prog_min;
    }

    ImGui::Separator();

    ImGui::BeginChild("disasm_scroll", ImVec2(0, 260), true);

    const int rows = 40; // show 40 instructions
    uint32_t pc = gui.disasm_base_pc & ~0x3u;

    for (int i = 0; i < rows; ++i) {
        if (pc < prog_min || pc >= prog_max) {
            break;
        }

        uint32_t raw = cpu::load_u32(cpu.mem, pc);
        cpu::DecodedInstr d = cpu::decode(raw);
        std::string asm_str = instr_to_asm(d);

        bool is_current_pc = (pc == cpu.pc);

        // Optional symbol label line
        auto it = gui.symbols.find(pc);
        if (it != gui.symbols.end()) {
            ImGui::TextDisabled("%s:", it->second.c_str());
        }

        // Make the whole line selectable so we can click to set breakpoint
        std::ostringstream line;
        line << "0x" << std::hex << std::setw(8) << std::setfill('0') << pc
             << "  0x" << std::setw(8) << raw
             << "  " << asm_str;

        if (is_current_pc) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
        }

        bool selected = is_current_pc;
        if (ImGui::Selectable(line.str().c_str(), selected)) {
            // Clicking sets breakpoint to that PC
            gui.break_pc       = pc;
            gui.break_enabled  = true;
            gui.breakpoint_hit = false;
        }

        if (is_current_pc) {
            ImGui::PopStyleColor();
        }

        pc += 4;
    }

    ImGui::EndChild();
    ImGui::End();
}

// Watch window for registers and memory
static void DrawWatchWindow(cpu::CPU& cpu, GuiState& gui) {
    if (!ImGui::Begin("Watch")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Add watch (example: x5 or 0x1000):");
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputText("##watch_input", gui.watch.input_buf,
                         sizeof(gui.watch.input_buf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string s(gui.watch.input_buf);
        // trim
        s.erase(0, s.find_first_not_of(" \t\r\n"));
        s.erase(s.find_last_not_of(" \t\r\n") + 1);

        if (!s.empty()) {
            WatchItem item{};
            if (s[0] == 'x' || s[0] == 'X') {
                // register watch
                int reg_num = std::stoi(s.substr(1), nullptr, 10);
                if (reg_num >= 0 && reg_num < 32) {
                    item.kind = WatchKind::Reg;
                    item.reg  = reg_num;
                    gui.watch.items.push_back(item);
                }
            } else {
                // memory address, hex or dec
                uint32_t addr = 0;
                if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
                    addr = static_cast<uint32_t>(std::stoul(s, nullptr, 16));
                } else {
                    addr = static_cast<uint32_t>(std::stoul(s, nullptr, 10));
                }
                item.kind = WatchKind::Mem;
                item.addr = addr;
                gui.watch.items.push_back(item);
            }
        }
        gui.watch.input_buf[0] = '\0';
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        gui.watch.items.clear();
    }

    ImGui::Separator();

    for (std::size_t i = 0; i < gui.watch.items.size(); ++i) {
        const auto& w = gui.watch.items[i];

        if (w.kind == WatchKind::Reg) {
            int r = w.reg;
            uint32_t val = cpu.regs[r];
            ImGui::Text("x%-2d = 0x%08X (%u)", r, val, val);
        } else {
            uint32_t addr = w.addr;
            bool in_range = cpu.mem.is_mapped(addr, 4);
            uint32_t val = in_range ? cpu::load_u32(cpu.mem, addr) : 0u;
            ImGui::Text("[0x%08X] = %s0x%08X%s",
                        addr,
                        in_range ? "" : "<",
                        val,
                        in_range ? "" : " (out)");
        }
    }

    ImGui::End();
}
// Pipeline timing diagram: grid of [instruction row] x [cycle column]
// Each cell shows which stage that instruction was in on that cycle.
static void DrawPipelineTimelineWindow(cpu::CPU& cpu, GuiState& gui) {
    if (!ImGui::Begin("Pipeline Timeline")) {
        ImGui::End();
        return;
    }

    auto& hist = gui.pipeline_hist;
    if (hist.empty()) {
        ImGui::TextDisabled("No pipeline history yet. Run the CPU.");
        ImGui::End();
        return;
    }

    // --- 1) Choose a window of recent cycles ---
    const int max_cols = 32;                          // show last 32 cycles
    const int total_samples = (int)hist.size();
    int start_idx = std::max(0, total_samples - max_cols);
    int visible_samples = total_samples - start_idx;

    // --- 2) Collect unique PCs from visible window ---
    std::vector<uint32_t> pcs;
    pcs.reserve(visible_samples * 5);
    for (int i = start_idx; i < total_samples; ++i) {
        const auto& s = hist[i];
        uint32_t arr[5] = { s.if_pc, s.id_pc, s.ex_pc, s.mem_pc, s.wb_pc };
        for (uint32_t pc : arr) {
            if (pc == 0) continue;
            if (std::find(pcs.begin(), pcs.end(), pc) == pcs.end())
                pcs.push_back(pc);
        }
    }
    if (pcs.empty()) {
        ImGui::TextDisabled("No valid pipeline PCs in history.");
        ImGui::End();
        return;
    }

    // Sort PCs so rows are stable & ordered
    std::sort(pcs.begin(), pcs.end());

    // --- 3) Build label text per PC: "0x00000010: addi x1, x0, 1" ---
    auto& imem = cpu.mem.imem;
    std::vector<std::string> row_labels;
    row_labels.reserve(pcs.size());
    for (uint32_t pc : pcs) {
        std::ostringstream lab;
        lab << "0x" << std::hex << std::setw(8) << std::setfill('0') << pc;

        std::size_t idx = pc / 4;
        if (idx < imem.size()) {
            uint32_t raw = imem[idx];
            auto d = cpu::decode(raw);
            lab << ": " << instr_to_asm(d);
        }
        row_labels.push_back(lab.str());
    }

    // --- 4) Draw table: first column = instruction, others = cycles ---
    ImGui::BeginChild("pipe_grid_child", ImVec2(0, 260), true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    int columns = 1 + visible_samples; // 1 for label, N for cycles
    ImGuiTableFlags flags = ImGuiTableFlags_Borders |
                            ImGuiTableFlags_RowBg   |
                            ImGuiTableFlags_ScrollX;

    if (ImGui::BeginTable("pipe_grid", columns, flags)) {
        // Column headers
        ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthStretch);
        for (int i = 0; i < visible_samples; ++i) {
            char buf[32];
            const auto& s = hist[start_idx + i];
            std::snprintf(buf, sizeof(buf), "%llu",
                          (unsigned long long)s.cycle);
            ImGui::TableSetupColumn(buf, ImGuiTableColumnFlags_WidthFixed);
        }
        ImGui::TableHeadersRow();

        // One row per PC (instruction)
        for (std::size_t r = 0; r < pcs.size(); ++r) {
            uint32_t pc = pcs[r];
            ImGui::TableNextRow();

            // Column 0: instruction label
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row_labels[r].c_str());

            // Subsequent columns: per-cycle stage
            for (int c = 0; c < visible_samples; ++c) {
                const auto& s = hist[start_idx + c];
                ImGui::TableSetColumnIndex(1 + c);

                const char* text = "";
                ImVec4 col(1, 1, 1, 1);
                bool have_stage = false;

                if (s.if_pc == pc) {
                    text = "IF";
                    col  = ImVec4(0.3f, 0.6f, 1.0f, 1.0f);   // blue-ish
                    have_stage = true;
                } else if (s.id_pc == pc) {
                    text = "ID";
                    col  = ImVec4(1.0f, 0.7f, 0.3f, 1.0f);   // orange
                    have_stage = true;
                } else if (s.ex_pc == pc) {
                    text = "EX";
                    col  = ImVec4(0.3f, 1.0f, 0.5f, 1.0f);   // green-ish
                    have_stage = true;
                } else if (s.mem_pc == pc) {
                    text = "MEM";
                    col  = ImVec4(0.8f, 0.4f, 1.0f, 1.0f);   // purple
                    have_stage = true;
                } else if (s.wb_pc == pc) {
                    text = "WB";
                    col  = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);   // red-ish
                    have_stage = true;
                }

                if (have_stage) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(col));
                    ImGui::TextUnformatted(text);
                    ImGui::PopStyleColor();
                } else {
                    ImGui::TextUnformatted(" ");
                }
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();

    ImGui::TextDisabled(
        "Rows = static instructions (PC).\n"
        "Columns = recent cycles (rightmost = newest).\n"
        "Cell labels show which stage that instruction was in at that cycle."
    );

    ImGui::End();
}

// Hazards & forwarding visualization (approx, based on snapshot)
static void DrawHazardWindow(cpu::CPU& cpu) {
    if (!ImGui::Begin("Hazards & Forwarding")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Potential RAW hazards (approximate analysis):");
    ImGui::Separator();

    if (!cpu.id_ex.valid) {
        ImGui::TextDisabled("No instruction in EX (ID/EX latch).");
    } else {
        const auto& d = cpu.id_ex.dinstr;
        bool hazard_found = false;

        auto check_hazard = [&](const char* from_stage, uint8_t rd, bool mem_read) {
            if (rd == 0) return;
            if (d.rs1 == rd) {
                ImGui::BulletText("RAW: ID uses x%d (rs1), %s writing x%d%s",
                                  (int)d.rs1, from_stage, (int)rd,
                                  mem_read ? " (load-use hazard)" : "");
                hazard_found = true;
            }
            if (d.rs2 == rd) {
                ImGui::BulletText("RAW: ID uses x%d (rs2), %s writing x%d%s",
                                  (int)d.rs2, from_stage, (int)rd,
                                  mem_read ? " (load-use hazard)" : "");
                hazard_found = true;
            }
        };

        if (cpu.ex_mem.valid) {
            check_hazard("EX/MEM", cpu.ex_mem.rd, cpu.ex_mem.mem_read);
        }
        if (cpu.mem_wb.valid) {
            check_hazard("MEM/WB", cpu.mem_wb.rd, false);
        }

        if (!hazard_found) {
            ImGui::TextDisabled("No obvious RAW hazards in this snapshot.");
        }
    }

    ImGui::Separator();
    ImGui::Text("Stalls & branches (global):");
    ImGui::BulletText("Stall cycles: %llu",
                      (unsigned long long)cpu.stats.stall_cycles);
    ImGui::BulletText("Branch instrs: %llu",
                      (unsigned long long)cpu.stats.branch_instructions);
    ImGui::BulletText("Branch mispredicts: %llu",
                      (unsigned long long)cpu.stats.branch_mispredictions);

    double mis_rate = (cpu.stats.branch_predictions == 0)
        ? 0.0
        : (double)cpu.stats.branch_mispredictions /
          (double)cpu.stats.branch_predictions;
    ImGui::BulletText("Mispredict rate: %.3f", mis_rate);

    ImGui::TextDisabled("Note: This is an approximate view based on current latch contents.");

    ImGui::End();
}

// Heatmap window: reg usage bars + top memory hotspots
static void DrawHeatmapWindow(GuiState& gui) {
    if (!ImGui::Begin("Heatmap")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Register usage (reads/writes approximated from ID/EX):");
    ImGui::Separator();

    double xs[32], ys[32];
    double max_use = 0.0;
    for (int i = 0; i < 32; ++i) {
        xs[i] = (double)i;
        ys[i] = (double)gui.reg_use_count[i];
        if (ys[i] > max_use) max_use = ys[i];
    }

    if (ImPlot::BeginPlot("Reg Use", ImVec2(-1, 200))) {
        ImPlot::SetupAxes("Reg index", "Count",
                          ImPlotAxisFlags_AutoFit,
                          ImPlotAxisFlags_AutoFit);
        ImPlot::PlotBars("usage", xs, ys, 32, 0.5);
        ImPlot::EndPlot();
    }

    ImGui::Separator();
    ImGui::Text("Memory hotspots (top addresses by access count):");

    if (gui.mem_use_count.empty()) {
        ImGui::TextDisabled("No memory accesses recorded yet.");
    } else {
        std::vector<std::pair<uint32_t, uint64_t>> entries(
            gui.mem_use_count.begin(), gui.mem_use_count.end());
        std::sort(entries.begin(), entries.end(),
                  [](auto& a, auto& b) { return a.second > b.second; });

        int top_n = (int)std::min<std::size_t>(entries.size(), 16);
        if (ImGui::BeginTable("mem_hot", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Count");
            ImGui::TableSetupColumn("Relative");
            ImGui::TableHeadersRow();

            double max_cnt = (double)entries.front().second;
            for (int i = 0; i < top_n; ++i) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("0x%08X", entries[i].first);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%llu", (unsigned long long)entries[i].second);
                ImGui::TableSetColumnIndex(2);
                double ratio = max_cnt > 0.0 ? entries[i].second / max_cnt : 0.0;
                ImGui::Text("%.2f", ratio);
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

// Mini performance analyzer window
static void DrawPerfAnalyzerWindow(cpu::CPU& cpu, GuiState& gui) {
    if (!ImGui::Begin("Perf Analyzer")) {
        ImGui::End();
        return;
    }

    uint64_t total_instrs = gui.perf.alu + gui.perf.mem +
                            gui.perf.branch + gui.perf.other;

    ImGui::Text("Instruction categories (approx from EX stage):");
    ImGui::Separator();
    ImGui::Text("ALU    : %llu", (unsigned long long)gui.perf.alu);
    ImGui::Text("Memory : %llu", (unsigned long long)gui.perf.mem);
    ImGui::Text("Branch : %llu", (unsigned long long)gui.perf.branch);
    ImGui::Text("Other  : %llu", (unsigned long long)gui.perf.other);
    ImGui::Text("Total  : %llu", (unsigned long long)total_instrs);

    ImGui::Separator();
    ImGui::Text("IPC & stalls:");

    double ipc = (cpu.stats.cycles == 0)
        ? 0.0
        : (double)cpu.stats.committed_instructions /
          (double)cpu.stats.cycles;
    double stall_pct = (cpu.stats.cycles == 0)
        ? 0.0
        : 100.0 * (double)cpu.stats.stall_cycles /
          (double)cpu.stats.cycles;

    ImGui::Text("IPC          : %.3f", ipc);
    ImGui::Text("Stall %%      : %.2f%%", stall_pct);

    ImGui::Separator();
    ImGui::Text("Category distribution:");

    if (total_instrs == 0) {
        ImGui::TextDisabled("No instructions categorized yet.");
    } else if (ImPlot::BeginPlot("Category Pie", ImVec2(250, 250), ImPlotFlags_Equal)) {

        const char* labels[] = {"ALU", "MEM", "BR", "OTHER"};
        double values[4] = {
            (double)gui.perf.alu,
            (double)gui.perf.mem,
            (double)gui.perf.branch,
            (double)gui.perf.other
        };

        // use the overload with label_fmt (const char*)
        ImPlot::PlotPieChart(labels, values, 4,
                             0.5, 0.5, 0.4,
                             "%.1f");  // show percentages/values with 1 decimal

        ImPlot::EndPlot();
    }


    ImGui::End();
}

// UART console (MMIO @ 0x10000000)
static void DrawUartConsoleWindow(cpu::CPU& cpu) {
    if (!ImGui::Begin("UART Console")) {
        ImGui::End();
        return;
    }

    ImGui::Text("MMIO UART TX @ 0x%08X", cpu::CPU::UART_BASE);
    if (ImGui::Button("Clear")) {
        cpu.uart_buffer.clear();
    }
    ImGui::Separator();

    static std::size_t last_len = 0;
    bool grew = cpu.uart_buffer.size() > last_len;
    last_len = cpu.uart_buffer.size();

    ImGui::BeginChild("uart_scroll", ImVec2(0, 200), true);
    ImGui::TextUnformatted(cpu.uart_buffer.c_str());
    if (grew) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::End();
}

static bool DrawControlWindow(cpu::CPU& cpu,
                              bool& running,
                              int& cycles_per_frame,
                              bool& request_reset,
                              GuiState& gui)
{
    bool step_one = false;

    if (!ImGui::Begin("Control")) {
        ImGui::End();
        return false;
    }

    if (ImGui::Button(running ? "Pause" : "Run")) {
        running = !running;
        gui.breakpoint_hit = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        step_one = true;
        gui.breakpoint_hit = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        request_reset = true;
        gui.breakpoint_hit = false;
        std::fill(std::begin(gui.reg_highlight),
                  std::end(gui.reg_highlight), 0.0f);
    }

    ImGui::SliderInt("Cycles / frame", &cycles_per_frame, 1, 1000);
    ImGui::Text("Halted: %s", cpu.halted ? "yes" : "no");

    ImGui::Separator();
    ImGui::Text("Breakpoint at PC (byte, hex):");

    ImGui::InputScalar("PC", ImGuiDataType_U32,
                       &gui.break_pc, nullptr, nullptr,
                       "%08X", ImGuiInputTextFlags_CharsHexadecimal);

    ImGui::Checkbox("Enable breakpoint", &gui.break_enabled);

    if (gui.breakpoint_hit) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                           "Hit breakpoint at PC=0x%08X",
                           gui.break_pc);
    }

    ImGui::End();
    return step_one;
}

// ---------- Main ----------

int main(int argc, char** argv)
{
    // 1) Init GLFW + OpenGL + ImGui
    if (!glfwInit())
        return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(
        1280, 720, "RV32I CPU Simulator GUI", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();                 // ImPlot context
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // 2) Initialize CPU and program
    cpu::CPU cpu;
    cpu::Memory mem;
    uint32_t entry = 0;
    std::unordered_map<uint32_t, std::string> symbols;

    // Program path: argv[1] if provided, otherwise fall back to "../programs/test.hex"
    std::string program_path = (argc >= 2) ? std::string(argv[1]) : std::string("../programs/test.hex");

    if (!cpu::load_program(program_path, mem, entry, &symbols)) {
        std::fprintf(stderr, "Failed to load program: %s\n", program_path.c_str());
        return 1;
    }
    const cpu::Memory loaded_mem = mem; // keep an immutable copy for Reset
    cpu.mem = loaded_mem;
    cpu.pc  = entry;

    bool running = false;
    int  cycles_per_frame = 1;
    CpiHistory cpi_hist;   // track CPI over time
    GuiState   gui;        // all GUI state
    reset_gui_state(gui, cpu, symbols, entry);
    update_events(gui, cpu);

    // 3) Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Single-page debugger dashboard (applies stepping/reset BEFORE drawing the page)
        DrawDashboard(cpu, loaded_mem, entry, symbols, running, cycles_per_frame, cpi_hist, gui);

        // Render ImGui
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
