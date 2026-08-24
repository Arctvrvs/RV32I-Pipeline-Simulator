#pragma once

# include <cstdint>
# include <cstddef>
# include "pipeline.h"
# include "memory.h"
# include "metrics.h"
 # include <string>
using namespace std;

namespace cpu
{
    struct CPU
    {
        static constexpr size_t BP_ENTRIES = 256;

        // Information about the most recent committed instruction (WB stage)
        // Filled during do_wb() each cycle; used by per-commit cosimulation.
        struct CommitInfo {
            bool     valid = false;
            uint64_t instr_id = 0;
            uint32_t pc = 0;
            uint32_t raw = 0;

            bool     reg_write = false;
            uint8_t  rd = 0;
            uint32_t wb_data = 0;

            bool     mem_is_load = false;
            bool     mem_is_store = false;
            uint32_t mem_addr = 0;
            uint32_t mem_store_data = 0;
            uint8_t  mem_size = 0;
            bool     mem_signed = false;
        };

        uint32_t pc = 0;
        uint32_t regs[32] = {0};

        // ---------------- Minimal CSR / Trap State (M-mode only) ----------------
        static constexpr uint16_t CSR_MSTATUS = 0x300;
        static constexpr uint16_t CSR_MTVEC   = 0x305;
        static constexpr uint16_t CSR_MEPC    = 0x341;
        static constexpr uint16_t CSR_MCAUSE  = 0x342;

        // Minimal CSR storage
        uint32_t csr_mstatus = 0;
        uint32_t csr_mtvec   = 0;
        uint32_t csr_mepc    = 0;
        uint32_t csr_mcause  = 0;

        uint32_t read_csr(uint16_t addr) const;
        void     write_csr(uint16_t addr, uint32_t value);
        void     enter_trap(uint32_t cause, uint32_t epc);
        uint32_t trap_vector() const; // aligned mtvec base
        uint32_t do_mret();           // returns new PC

        // ---------------- UART MMIO (0x10000000) ----------------
        static constexpr uint32_t UART_BASE = 0x10000000u;
        std::string uart_buffer;

        // Assigned in IF for each fetched instruction (used for GUI tracing / verification)
        uint64_t next_instr_id = 1;

        // Updated each cycle during WB.
        CommitInfo last_commit;

        Memory  mem;
        IF_ID   if_id;
        ID_EX   id_ex;
        EX_MEM  ex_mem;
        MEM_WB  mem_wb;

        uint8_t  bpred_counter[BP_ENTRIES] = {};
        uint32_t bpred_target[BP_ENTRIES]  = {};
        uint32_t bpred_tag[BP_ENTRIES]     = {};
        bool     bpred_is_branch[BP_ENTRIES] = {};
        bool     bpred_valid[BP_ENTRIES]   = {};

        Metrics stats;
        bool    halted = false;

        explicit CPU(size_t dmem_size = 64 * 1024);

        void step(); // one cycle
    };

}
