#pragma once
# include <cstdint>

namespace cpu
{
    struct Metrics
    {
        uint64_t cycles                 = 0;
        uint64_t committed_instructions = 0;
        uint64_t stall_cycles           = 0;

        uint64_t branch_instructions   = 0;
        uint64_t branch_predictions    = 0;
        uint64_t branch_mispredictions = 0;
    };

}
