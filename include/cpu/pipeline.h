#pragma once
# include <cstdint>
# include "instr.h"

namespace cpu
{
    struct IF_ID
    {
        uint64_t instr_id = 0; // monotonically increasing id for traceability

        uint32_t pc       = 0;
        uint32_t instr    = 0;
        uint16_t bp_index = 0;

        bool     predicted_taken  = false;
        uint32_t predicted_target = 0;
        bool     valid            = false;
    };

    struct ID_EX
    {
        uint64_t instr_id = 0;

        uint32_t pc       = 0;
        DecodedInstr dinstr;
        uint32_t rs1_val  = 0;
        uint32_t rs2_val  = 0;
        uint16_t bp_index = 0;

        bool     predicted_taken  = false;
        uint32_t predicted_target = 0;
        bool     valid            = false;
    };

    struct EX_MEM
    {
        uint64_t instr_id = 0;
        uint32_t pc       = 0;
        uint32_t raw      = 0;

        uint32_t alu_result = 0; // ALU result or effective address
        uint32_t store_data = 0;
        uint8_t  rd = 0;

        bool     reg_write = false;
        bool     mem_read = false;
        bool     mem_write = false;
        bool     mem_to_reg = false;
        uint8_t  mem_size = 0;     // bytes (1/2/4)
        bool     mem_signed = false; // for loads only

        bool     valid = false;
    };

    struct MEM_WB
    {
        uint64_t instr_id = 0;
        uint32_t pc       = 0;
        uint32_t raw      = 0;

        // Memory access info (for cosimulation / debugging)
        bool     mem_is_load     = false;
        bool     mem_is_store    = false;
        uint32_t mem_addr        = 0;
        uint32_t mem_store_data  = 0;
        uint8_t  mem_size        = 0;
        bool     mem_signed      = false;

        uint32_t wb_data = 0;
        uint8_t  rd = 0;
        bool     reg_write = false;
        bool     valid = false;
    };

}
