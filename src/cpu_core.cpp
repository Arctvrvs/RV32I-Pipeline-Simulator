#include "cpu/cpu_core.h"
#include "cpu/stages.h"

namespace cpu
{
    static inline uint32_t align_pc(uint32_t pc) { return pc & ~0x3u; }

    uint32_t CPU::read_csr(uint16_t addr) const {
        switch (addr) {
        case CSR_MSTATUS: return csr_mstatus;
        case CSR_MTVEC:   return csr_mtvec;
        case CSR_MEPC:    return csr_mepc;
        case CSR_MCAUSE:  return csr_mcause;
        default:          return 0;
        }
    }

    void CPU::write_csr(uint16_t addr, uint32_t value) {
        switch (addr) {
        case CSR_MSTATUS: csr_mstatus = value; break;
        case CSR_MTVEC:   csr_mtvec   = value; break;
        case CSR_MEPC:    csr_mepc    = value; break;
        case CSR_MCAUSE:  csr_mcause  = value; break;
        default:
            // ignore unknown
            break;
        }
    }

    uint32_t CPU::trap_vector() const {
        // mtvec[1:0] mode; we implement direct mode only (treat vectored as direct).
        return align_pc(csr_mtvec);
    }

    void CPU::enter_trap(uint32_t cause, uint32_t epc) {
        // Minimal RISC-V trap entry (M-mode only).
        // mstatus: MPIE <= MIE; MIE <= 0; MPP <= 3
        constexpr uint32_t MIE  = (1u << 3);
        constexpr uint32_t MPIE = (1u << 7);
        constexpr uint32_t MPP_SHIFT = 11;
        constexpr uint32_t MPP_MASK  = (3u << MPP_SHIFT);

        uint32_t mie = (csr_mstatus & MIE) ? 1u : 0u;
        // Set MPIE to previous MIE
        csr_mstatus = (csr_mstatus & ~MPIE) | (mie ? MPIE : 0u);
        // Disable interrupts
        csr_mstatus &= ~MIE;
        // Set MPP=Machine (11)
        csr_mstatus = (csr_mstatus & ~MPP_MASK) | (3u << MPP_SHIFT);

        csr_mepc   = epc;
        csr_mcause = cause; // bit31=0 => exception
    }

    uint32_t CPU::do_mret() {
        // Minimal MRET: restore MIE from MPIE; set MPIE=1; clear MPP.
        constexpr uint32_t MIE  = (1u << 3);
        constexpr uint32_t MPIE = (1u << 7);
        constexpr uint32_t MPP_SHIFT = 11;
        constexpr uint32_t MPP_MASK  = (3u << MPP_SHIFT);

        uint32_t mpie = (csr_mstatus & MPIE) ? 1u : 0u;
        // Restore MIE
        csr_mstatus = (csr_mstatus & ~MIE) | (mpie ? MIE : 0u);
        // Set MPIE=1
        csr_mstatus |= MPIE;
        // Clear MPP
        csr_mstatus &= ~MPP_MASK;

        return csr_mepc;
    }

    CPU::CPU(size_t dmem_size) : mem(dmem_size)
    {
        pc = 0;
        next_instr_id = 1;
        uart_buffer.clear();

        for (auto &r : regs)
        {
            r = 0;
        }

        // CSRs default to 0
        csr_mstatus = 0;
        csr_mtvec   = 0;
        csr_mepc    = 0;
        csr_mcause  = 0;

        if_id = {};
        id_ex = {};
        ex_mem = {};
        mem_wb = {};
        last_commit = {};

        for (size_t i = 0; i < BP_ENTRIES; ++i)
        {
            bpred_counter[i] = 1;
            bpred_target[i]  = 0;
            bpred_tag[i]     = 0;
            bpred_is_branch[i] = false;
            bpred_valid[i]   = false;
        }

        stats = {};
        halted = false;
    }

    void CPU::step()
    {
        if (halted) return;

        // Make "next" copies of pipeline regs
        IF_ID next_if_id  = if_id;
        ID_EX next_id_ex  = id_ex;
        EX_MEM next_ex_mem = ex_mem;
        MEM_WB next_mem_wb = mem_wb;

        bool stall        = false;
        bool redirect     = false;
        uint32_t redirect_target = 0;

        // Advance stages in reverse order
        do_wb(*this);
        do_mem(*this, next_mem_wb);
        do_ex(*this, next_ex_mem, redirect, redirect_target);
        do_id(*this, next_id_ex, stall);
        do_if(*this, next_if_id, redirect, redirect_target, stall);

        // If we redirected (branch/jump/trap), flush the instruction sitting in ID
        if (redirect)
        {
            next_id_ex.valid = false;
        }

        // commit new pipeline regs
        if_id = next_if_id;
        id_ex = next_id_ex;
        ex_mem = next_ex_mem;
        mem_wb = next_mem_wb;

        // x0 is hard-wired zero
        regs[0] = 0;

        // One more cycle done
        stats.cycles++;

        // Halt condition: nothing in pipeline and fetch has fallen off end of program
        bool pipeline_empty =
            !if_id.valid && !id_ex.valid && !ex_mem.valid && !mem_wb.valid;

        bool pc_out_of_program = (pc < mem.prog_min) || (pc >= mem.prog_max);
        if (pipeline_empty && pc_out_of_program)
        {
            halted = true;
        }
    }
}
