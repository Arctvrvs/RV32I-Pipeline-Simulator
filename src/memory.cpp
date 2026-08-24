#include "cpu/memory.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace cpu {

static constexpr uint32_t ELF_MAGIC = 0x464C457Fu; // "\x7FELF" little-endian

static inline uint16_t read_u16_le(const std::vector<uint8_t>& b, std::size_t off) {
    if (off + 2 > b.size()) return 0;
    return static_cast<uint16_t>(b[off] | (static_cast<uint16_t>(b[off + 1]) << 8));
}

static inline uint32_t read_u32_le(const std::vector<uint8_t>& b, std::size_t off) {
    if (off + 4 > b.size()) return 0;
    return static_cast<uint32_t>(b[off] |
                                 (static_cast<uint32_t>(b[off + 1]) << 8) |
                                 (static_cast<uint32_t>(b[off + 2]) << 16) |
                                 (static_cast<uint32_t>(b[off + 3]) << 24));
}

Memory::Memory(std::size_t dmem_size_bytes) {
    imem.clear();
    dmem.assign(dmem_size_bytes, 0);
    virt_base = 0;
    prog_min = 0;
    prog_max = 0;
}

bool Memory::is_mapped(uint32_t addr, uint32_t size_bytes) const {
    if (size_bytes == 0) return true;
    if (addr < virt_base) return false;
    uint64_t start = static_cast<uint64_t>(addr) - virt_base;
    uint64_t end = start + static_cast<uint64_t>(size_bytes);
    return end <= static_cast<uint64_t>(dmem.size());
}

std::size_t Memory::to_index(uint32_t addr) const {
    return static_cast<std::size_t>(addr - virt_base);
}

static inline uint8_t get_byte(const Memory& mem, uint32_t addr) {
    if (!mem.is_mapped(addr, 1)) return 0;
    return mem.dmem[mem.to_index(addr)];
}

static inline void set_byte(Memory& mem, uint32_t addr, uint8_t v) {
    if (!mem.is_mapped(addr, 1)) return;
    mem.dmem[mem.to_index(addr)] = v;
}

// ----------------- Loads/Stores -----------------

uint8_t load_u8(const Memory& mem, uint32_t addr) {
    return get_byte(mem, addr);
}

uint16_t load_u16(const Memory& mem, uint32_t addr) {
    // Little-endian, unaligned allowed
    uint16_t b0 = get_byte(mem, addr);
    uint16_t b1 = get_byte(mem, addr + 1u);
    return static_cast<uint16_t>(b0 | (b1 << 8));
}

uint32_t load_u32(const Memory& mem, uint32_t addr) {
    uint32_t b0 = get_byte(mem, addr);
    uint32_t b1 = get_byte(mem, addr + 1u);
    uint32_t b2 = get_byte(mem, addr + 2u);
    uint32_t b3 = get_byte(mem, addr + 3u);
    return (b0) | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

int32_t load_s8(const Memory& mem, uint32_t addr) {
    int8_t v = static_cast<int8_t>(get_byte(mem, addr));
    return static_cast<int32_t>(v);
}

int32_t load_s16(const Memory& mem, uint32_t addr) {
    uint16_t u = load_u16(mem, addr);
    int16_t v = static_cast<int16_t>(u);
    return static_cast<int32_t>(v);
}

uint32_t load_n(const Memory& mem, uint32_t addr, uint8_t size_bytes, bool sign_extend) {
    switch (size_bytes) {
    case 1:
        return sign_extend ? static_cast<uint32_t>(load_s8(mem, addr))
                           : static_cast<uint32_t>(load_u8(mem, addr));
    case 2:
        return sign_extend ? static_cast<uint32_t>(load_s16(mem, addr))
                           : static_cast<uint32_t>(load_u16(mem, addr));
    case 4:
        return load_u32(mem, addr);
    default:
        return 0;
    }
}

void store_u8(Memory& mem, uint32_t addr, uint8_t value) {
    set_byte(mem, addr, value);
}

void store_u16(Memory& mem, uint32_t addr, uint16_t value) {
    set_byte(mem, addr, static_cast<uint8_t>(value & 0xFFu));
    set_byte(mem, addr + 1u, static_cast<uint8_t>((value >> 8) & 0xFFu));
}

void store_u32(Memory& mem, uint32_t addr, uint32_t value) {
    set_byte(mem, addr, static_cast<uint8_t>(value & 0xFFu));
    set_byte(mem, addr + 1u, static_cast<uint8_t>((value >> 8) & 0xFFu));
    set_byte(mem, addr + 2u, static_cast<uint8_t>((value >> 16) & 0xFFu));
    set_byte(mem, addr + 3u, static_cast<uint8_t>((value >> 24) & 0xFFu));
}

void store_n(Memory& mem, uint32_t addr, uint8_t size_bytes, uint32_t value) {
    switch (size_bytes) {
    case 1:
        store_u8(mem, addr, static_cast<uint8_t>(value & 0xFFu));
        break;
    case 2:
        store_u16(mem, addr, static_cast<uint16_t>(value & 0xFFFFu));
        break;
    case 4:
        store_u32(mem, addr, value);
        break;
    default:
        break;
    }
}

// ----------------- Program Loading -----------------

static bool read_file_bytes(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    f.seekg(0, std::ios::end);
    std::streamsize sz = f.tellg();
    if (sz <= 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(sz));
    if (!f.read(reinterpret_cast<char*>(out.data()), sz)) return false;
    return true;
}

static bool load_elf32_le(const std::string& path,
                          Memory& mem,
                          uint32_t& entry,
                          std::unordered_map<uint32_t, std::string>* symbols)
{
    std::vector<uint8_t> b;
    if (!read_file_bytes(path, b)) return false;
    if (b.size() < 0x34) return false;

    // Check magic + class + endianness
    uint32_t magic = read_u32_le(b, 0);
    if (magic != ELF_MAGIC) return false;
    uint8_t ei_class = b[4]; // 1 = 32-bit
    uint8_t ei_data  = b[5]; // 1 = little
    if (ei_class != 1 || ei_data != 1) return false;

    entry = read_u32_le(b, 0x18);
    uint32_t e_phoff = read_u32_le(b, 0x1C);
    uint32_t e_shoff = read_u32_le(b, 0x20);
    uint16_t e_phentsize = read_u16_le(b, 0x2A);
    uint16_t e_phnum     = read_u16_le(b, 0x2C);
    uint16_t e_shentsize = read_u16_le(b, 0x2E);
    uint16_t e_shnum     = read_u16_le(b, 0x30);

    if (e_phoff == 0 || e_phentsize < 32 || e_phnum == 0) return false;
    if (static_cast<uint64_t>(e_phoff) + static_cast<uint64_t>(e_phentsize) * e_phnum > b.size()) return false;

    // Scan PT_LOAD segments
    uint32_t min_vaddr = 0xFFFFFFFFu;
    uint32_t max_end   = 0u;
    bool any_load = false;

    for (uint16_t i = 0; i < e_phnum; ++i) {
        std::size_t off = static_cast<std::size_t>(e_phoff) + static_cast<std::size_t>(i) * e_phentsize;
        uint32_t p_type   = read_u32_le(b, off + 0);
        if (p_type != 1) continue; // PT_LOAD

        uint32_t p_vaddr  = read_u32_le(b, off + 8);
        uint32_t p_filesz = read_u32_le(b, off + 16);
        uint32_t p_memsz  = read_u32_le(b, off + 20);

        any_load = true;
        min_vaddr = std::min(min_vaddr, p_vaddr);
        uint32_t end = p_vaddr + p_memsz;
        if (end > max_end) max_end = end;

        (void)p_filesz;
    }

    if (!any_load) return false;

    // Align mapping base down to 4KB for a little extra room
    uint32_t virt_base = min_vaddr & ~0xFFFu;
    uint64_t required  = static_cast<uint64_t>(max_end) - virt_base;

    // Add some extra memory for stack/heap growth (best-effort)
    constexpr uint64_t EXTRA = 64ull * 1024ull;
    uint64_t alloc = required + EXTRA;

    // Safety cap: 256 MiB
    constexpr uint64_t CAP = 256ull * 1024ull * 1024ull;
    if (alloc > CAP) return false;

    mem.imem.clear();
    mem.dmem.assign(static_cast<std::size_t>(alloc), 0);
    mem.virt_base = virt_base;
    mem.prog_min  = min_vaddr;
    mem.prog_max  = max_end;

    // Load segments
    for (uint16_t i = 0; i < e_phnum; ++i) {
        std::size_t off = static_cast<std::size_t>(e_phoff) + static_cast<std::size_t>(i) * e_phentsize;
        uint32_t p_type   = read_u32_le(b, off + 0);
        if (p_type != 1) continue;

        uint32_t p_offset = read_u32_le(b, off + 4);
        uint32_t p_vaddr  = read_u32_le(b, off + 8);
        uint32_t p_filesz = read_u32_le(b, off + 16);
        uint32_t p_memsz  = read_u32_le(b, off + 20);

        if (static_cast<uint64_t>(p_offset) + p_filesz > b.size()) return false;
        uint64_t dst_off = static_cast<uint64_t>(p_vaddr) - virt_base;
        if (dst_off + p_memsz > mem.dmem.size()) return false;

        // Copy file bytes
        std::memcpy(mem.dmem.data() + dst_off, b.data() + p_offset, p_filesz);
        // BSS already zeroed by dmem init
        (void)p_memsz;
    }

    // Populate imem for convenience (disasm). Best-effort.
    if (mem.prog_max > mem.prog_min) {
        uint32_t span = mem.prog_max - mem.prog_min;
        std::size_t words = (span + 3u) / 4u;
        mem.imem.resize(words);
        for (std::size_t i = 0; i < words; ++i) {
            uint32_t pc = mem.prog_min + static_cast<uint32_t>(i) * 4u;
            mem.imem[i] = load_u32(mem, pc);
        }
    }

    // Optional symbol extraction (best-effort)
    if (symbols) symbols->clear();
    if (symbols && e_shoff && e_shentsize >= 40 && e_shnum > 0) {
        if (static_cast<uint64_t>(e_shoff) + static_cast<uint64_t>(e_shentsize) * e_shnum <= b.size()) {
            // Load section headers
            auto sh_read_u32 = [&](std::size_t sh_off, std::size_t field) {
                return read_u32_le(b, sh_off + field);
            };

            for (uint16_t si = 0; si < e_shnum; ++si) {
                std::size_t sh_off = static_cast<std::size_t>(e_shoff) + static_cast<std::size_t>(si) * e_shentsize;
                uint32_t sh_type   = sh_read_u32(sh_off, 4);
                if (sh_type != 2 && sh_type != 11) continue; // SHT_SYMTAB or SHT_DYNSYM

                uint32_t sh_offset = sh_read_u32(sh_off, 16);
                uint32_t sh_size   = sh_read_u32(sh_off, 20);
                uint32_t sh_link   = sh_read_u32(sh_off, 24); // string table section index
                uint32_t sh_entsize = sh_read_u32(sh_off, 36);
                if (sh_entsize == 0) sh_entsize = 16;

                if (sh_offset == 0 || sh_size < sh_entsize) continue;
                if (static_cast<uint64_t>(sh_offset) + sh_size > b.size()) continue;

                // Load associated strtab
                if (sh_link >= e_shnum) continue;
                std::size_t str_sh_off = static_cast<std::size_t>(e_shoff) + static_cast<std::size_t>(sh_link) * e_shentsize;
                uint32_t str_off = sh_read_u32(str_sh_off, 16);
                uint32_t str_sz  = sh_read_u32(str_sh_off, 20);
                if (static_cast<uint64_t>(str_off) + str_sz > b.size()) continue;

                const char* strtab = reinterpret_cast<const char*>(b.data() + str_off);

                std::size_t count = sh_size / sh_entsize;
                std::size_t limit = std::min<std::size_t>(count, 5000);

                for (std::size_t i = 0; i < limit; ++i) {
                    std::size_t sym_off = static_cast<std::size_t>(sh_offset) + i * sh_entsize;
                    uint32_t st_name  = read_u32_le(b, sym_off + 0);
                    uint32_t st_value = read_u32_le(b, sym_off + 4);
                    uint8_t  st_info  = (sym_off + 12 < b.size()) ? b[sym_off + 12] : 0;
                    uint8_t  type = st_info & 0x0Fu;

                    if (st_name == 0 || st_value == 0) continue;
                    if (st_name >= str_sz) continue;

                    // Prefer functions/objects; keep others too if desired
                    if (type != 2 && type != 1) {
                        // skip non func/object to reduce noise
                        continue;
                    }

                    const char* name = strtab + st_name;
                    if (!name || !*name) continue;

                    (*symbols)[st_value] = std::string(name);
                }
            }
        }
    }

    return true;
}

bool load_hex_program(const std::string& path, Memory& mem) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    mem.imem.clear();
    mem.virt_base = 0;

    std::string line;
    while (std::getline(f, line)) {
        // Strip comments (# or //)
        auto pos_hash = line.find('#');
        if (pos_hash != std::string::npos) line = line.substr(0, pos_hash);
        auto pos_slash = line.find("//");
        if (pos_slash != std::string::npos) line = line.substr(0, pos_slash);

        // Trim whitespace
        auto is_ws = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
        while (!line.empty() && is_ws((unsigned char)line.front())) line.erase(line.begin());
        while (!line.empty() && is_ws((unsigned char)line.back())) line.pop_back();
        if (line.empty()) continue;

        uint32_t word = 0;
        try {
            word = static_cast<uint32_t>(std::stoul(line, nullptr, 16));
        } catch (...) {
            return false;
        }
        mem.imem.push_back(word);
    }

    mem.prog_min = 0;
    mem.prog_max = static_cast<uint32_t>(mem.imem.size() * 4ull);

    // Ensure dmem can hold the program image at base 0
    if (mem.dmem.size() < mem.prog_max) {
        mem.dmem.resize(mem.prog_max, 0);
    }

    // Mirror instruction words into dmem for unified fetching
    for (std::size_t i = 0; i < mem.imem.size(); ++i) {
        uint32_t pc = static_cast<uint32_t>(i) * 4u;
        store_u32(mem, pc, mem.imem[i]);
    }

    return true;
}

bool load_program(const std::string& path,
                  Memory& mem,
                  uint32_t& entry,
                  std::unordered_map<uint32_t, std::string>* symbols)
{
    entry = 0;

    // Try ELF first if magic matches
    std::vector<uint8_t> head;
    if (read_file_bytes(path, head) && head.size() >= 4) {
        uint32_t magic = read_u32_le(head, 0);
        if (magic == ELF_MAGIC) {
            return load_elf32_le(path, mem, entry, symbols);
        }
    }

    // Fallback: treat as hex
    bool ok = load_hex_program(path, mem);
    entry = 0;
    if (symbols) symbols->clear();
    return ok;
}

} // namespace cpu
