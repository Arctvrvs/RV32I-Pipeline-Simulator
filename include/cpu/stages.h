#pragma once

# include "cpu_core.h"

namespace cpu
{
    void do_wb  (CPU& cpu);
    void do_mem (CPU& cpu, MEM_WB& next_mem_wb);
    void do_ex  (CPU& cpu, EX_MEM& next_ex_mem,
                 bool& redirect, uint32_t& redirect_target);
    void do_id  (CPU& cpu, ID_EX& next_id_ex, bool& stall);
    void do_if  (CPU& cpu, IF_ID& next_if_id,
                 bool redirect, uint32_t redirect_target, bool stall);

}