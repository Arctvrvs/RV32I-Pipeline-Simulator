#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace cpu {

struct Memory {
    std::vector<uint32_t> imem; // instructions (32-bit words)
    std::vector<uint8_t>  dmem; // data memory (byte-addressable)

    // Virtual address mapping: dmem[0] corresponds to virt_base.
    // ELF programs often live at high addresses (e.g., 0x80000000).
    uint32_t virt_base = 0;

    // Program region (best-effort). Used to decide when fetch has fallen off the end.
    // For HEX loads, this is [0, imem.size()*4). For ELF, this covers loaded segments.
    uint32_t prog_min  = 0;
    uint32_t prog_max  = 0;

    explicit Memory(std::size_t dmem_size_bytes = 64 * 1024);

    bool is_mapped(uint32_t addr, uint32_t size_bytes = 1) const;
    std::size_t to_index(uint32_t addr) const; // only valid if is_mapped(addr)
};

bool load_hex_program(const std::string& path, Memory& mem);

// Load either a .hex (word-per-line) or ELF32 (little-endian) program.
// Returns entry point (0 for HEX, ELF e_entry for ELF). Symbols are optional.
bool load_program(const std::string& path,
                  Memory& mem,
                  uint32_t& entry,
                  std::unordered_map<uint32_t, std::string>* symbols = nullptr);

// Byte-addressable little-endian loads (unaligned allowed)
uint8_t  load_u8 (const Memory& mem, uint32_t addr);
uint16_t load_u16(const Memory& mem, uint32_t addr);
uint32_t load_u32(const Memory& mem, uint32_t addr);

int32_t  load_s8 (const Memory& mem, uint32_t addr);
int32_t  load_s16(const Memory& mem, uint32_t addr);

// Generic load: size_bytes in {1,2,4}. If sign_extend=true, treats as signed for 1/2 bytes.
uint32_t load_n(const Memory& mem, uint32_t addr, uint8_t size_bytes, bool sign_extend);

// Byte-addressable little-endian stores (unaligned allowed)
void store_u8 (Memory& mem, uint32_t addr, uint8_t  value);
void store_u16(Memory& mem, uint32_t addr, uint16_t value);
void store_u32(Memory& mem, uint32_t addr, uint32_t value);

// Generic store: size_bytes in {1,2,4}. Writes low bits of value.
void store_n(Memory& mem, uint32_t addr, uint8_t size_bytes, uint32_t value);

// Backwards-compatible helpers (word = 32-bit little-endian)
inline uint32_t load_word(const Memory& mem, uint32_t addr) { return load_u32(mem, addr); }
inline void     store_word(Memory& mem, uint32_t addr, uint32_t value) { store_u32(mem, addr, value); }

} // namespace cpu
