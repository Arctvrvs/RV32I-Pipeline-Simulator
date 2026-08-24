#!/usr/bin/env python
# RISC-V RV32I instruction encoder - generates hex files for simulator examples

from pathlib import Path

def i_type(imm, rs1, funct3, rd, opcode):
    imm = imm & 0xFFF
    return (imm << 20 | (rs1 & 0x1F) << 15 | (funct3 & 0x7) << 12 |
            (rd & 0x1F) << 7 | (opcode & 0x7F))

def r_type(funct7, rs2, rs1, funct3, rd, opcode):
    return ((funct7 & 0x7F) << 25 | (rs2 & 0x1F) << 20 | (rs1 & 0x1F) << 15 |
            (funct3 & 0x7) << 12 | (rd & 0x1F) << 7 | (opcode & 0x7F))

def s_type(imm, rs2, rs1, funct3, opcode):
    imm = imm & 0xFFF
    return (((imm >> 5) & 0x7F) << 25 | (rs2 & 0x1F) << 20 | (rs1 & 0x1F) << 15 |
            (funct3 & 0x7) << 12 | (imm & 0x1F) << 7 | (opcode & 0x7F))

def b_type(imm, rs2, rs1, funct3, opcode):
    imm = imm & 0x1FFF
    return (((imm >> 12) & 1) << 31 | ((imm >> 5) & 0x3F) << 25 |
            (rs2 & 0x1F) << 20 | (rs1 & 0x1F) << 15 |
            (funct3 & 0x7) << 12 | ((imm >> 1) & 0xF) << 8 |
            ((imm >> 11) & 1) << 7 | (opcode & 0x7F))

def j_type(imm, rd, opcode):
    imm = imm & 0x1FFFFF
    return (((imm >> 20) & 1) << 31 | ((imm >> 1) & 0x3FF) << 21 |
            ((imm >> 11) & 1) << 20 | ((imm >> 12) & 0xFF) << 12 |
            (rd & 0x1F) << 7 | (opcode & 0x7F))

def addi(rd, rs1, imm): return i_type(imm, rs1, 0, rd, 0x13)
def add(rd, rs1, rs2):  return r_type(0, rs2, rs1, 0, rd, 0x33)
def sub(rd, rs1, rs2):  return r_type(0b0100000, rs2, rs1, 0, rd, 0x33)
def slt(rd, rs1, rs2):  return r_type(0, rs2, rs1, 2, rd, 0x33)
def slti(rd, rs1, imm): return i_type(imm, rs1, 2, rd, 0x13)
def ori(rd, rs1, imm):  return i_type(imm, rs1, 6, rd, 0x13)
def andi(rd, rs1, imm): return i_type(imm, rs1, 7, rd, 0x13)
def xori(rd, rs1, imm): return i_type(imm, rs1, 4, rd, 0x13)
def slli(rd, rs1, shamt): return i_type(shamt & 0x1F, rs1, 1, rd, 0x13)
def srli(rd, rs1, shamt): return i_type(shamt & 0x1F, rs1, 5, rd, 0x13)
def lw(rd, rs1, imm):   return i_type(imm, rs1, 2, rd, 0x03)
def sw_(rs1, rs2, imm): return s_type(imm, rs2, rs1, 2, 0x23)
def bne(rs1, rs2, imm): return b_type(imm, rs2, rs1, 1, 0x63)
def beq(rs1, rs2, imm): return b_type(imm, rs2, rs1, 0, 0x63)
def blt(rs1, rs2, imm): return b_type(imm, rs2, rs1, 4, 0x63)
def bge(rs1, rs2, imm): return b_type(imm, rs2, rs1, 5, 0x63)
def bltu(rs1, rs2, imm): return b_type(imm, rs2, rs1, 6, 0x63)
def jal(rd, imm):       return j_type(imm, rd, 0x6F)
def jalr(rd, rs1, imm): return i_type(imm, rs1, 0, rd, 0x67)
def lui(rd, imm):       return ((imm >> 12) & 0xFFFFF) << 12 | (rd & 0x1F) << 7 | 0x37
def nop():              return addi(0, 0, 0)

# Verify against known encodings from existing test.hex
assert bne(4, 2, -16) == 0xfe2218e3
assert sw_(1, 3, 0)   == 0x0030a023
print("Encoders verified!")

OUTPUT_DIR = Path(__file__).resolve().parent

def save(prog, fname, desc):
    path = OUTPUT_DIR / fname
    with path.open('w', encoding='utf-8', newline='\n') as f:
        f.write("# {}\n".format(desc))
        for w, c in prog:
            f.write("{:08x}  # {}\n".format(w, c))
    print("Wrote {}  ({} instructions)".format(path.name, len(prog)))

# --------------------------------------------------------------------------
# 1. FIBONACCI
# Compute fib[0..19] and store starting at address 0x100
# x1=a  x2=b  x3=addr  x4=count  x5=tmp
# --------------------------------------------------------------------------
fib = [
    (addi(1,0,0),   "addi x1, x0,  0      # a = 0 (fib[0])"),
    (addi(2,0,1),   "addi x2, x0,  1      # b = 1 (fib[1])"),
    (addi(3,0,0x100), "addi x3, x0, 0x100   # addr = 0x100"),
    (addi(4,0,20),  "addi x4, x0, 20      # count = 20"),
    # PC=16: loop_start
    (sw_(3,1,0),    "sw   x1,  0(x3)      # mem[addr] = a"),
    (addi(3,3,4),   "addi x3, x3,  4      # addr += 4"),
    (add(5,1,2),    "add  x5, x1, x2      # tmp = a + b"),
    (addi(1,2,0),   "addi x1, x2,  0      # a = b"),
    (addi(2,5,0),   "addi x2, x5,  0      # b = tmp"),
    (addi(4,4,-1),  "addi x4, x4, -1      # count--"),
    (bne(4,0,-24),  "bne  x4, x0, -24     # if count!=0, goto loop_start"),
    (nop(),         "nop"),
    (nop(),         "nop"),
]
save(fib, "fibonacci.hex", "Fibonacci: fib[0..19] stored starting at address 0x0100")

# --------------------------------------------------------------------------
# 2. FACTORIAL (iterative, multiply by repeated addition)
# Computes 10! = 3628800, stored at address 0x100
# x1=result  x2=n  x3=acc  x4=inner_counter
# --------------------------------------------------------------------------
# outer loop: for n=10 downto 1: result = result * n
# inner loop: acc = 0; for i=n downto 1: acc += result
fact = [
    (addi(1,0,1),    "addi x1, x0,  1      # result = 1"),
    (addi(2,0,10),   "addi x2, x0, 10      # n = 10"),
    # PC=8: outer_loop
    (addi(3,0,0),    "addi x3, x0,  0      # acc = 0"),
    (addi(4,2,0),    "addi x4, x2,  0      # inner = n"),
    # PC=16: inner_loop
    (add(3,3,1),     "add  x3, x3, x1      # acc += result"),
    (addi(4,4,-1),   "addi x4, x4, -1      # inner--"),
    (bne(4,0,-8),    "bne  x4, x0, -8      # loop inner"),
    (addi(1,3,0),    "addi x1, x3,  0      # result = acc  (= old_result * n)"),
    (addi(2,2,-1),   "addi x2, x2, -1      # n--"),
    (bne(2,0,-28),   "bne  x2, x0, -28     # loop outer (to PC=8)"),
    # PC=40: done; x1 = 10! = 3628800
    (sw_(0,1,0x100), "sw   x1, 256(x0)     # store 10! at address 0x100"),
    (nop(),          "nop"),
    (nop(),          "nop"),
]
save(fact, "factorial.hex", "Factorial: 10! = 3628800 stored at address 0x0100")

# --------------------------------------------------------------------------
# 3. BUBBLE SORT
# Sorts 8 values [5,3,8,1,9,2,7,4] stored at 0x100
# After: [1,2,3,4,5,7,8,9] at 0x100
# x1=base  x3=i  x4=j  x5=a  x6=b  x8=addr_j  x12=tmp
# --------------------------------------------------------------------------
vals = [5, 3, 8, 1, 9, 2, 7, 4]
bsort = []

# Store the 8 input values at 0x100
bsort.append((addi(15,0,0x100), "addi x15, x0, 0x100  # ptr = 0x100"))
for i, v in enumerate(vals):
    bsort.append((addi(10,0,v),      "addi x10, x0, {:2d}".format(v)))
    bsort.append((sw_(15,10,i*4),    "sw   x10, {:2d}(x15)         # mem[0x{:03x}] = {:d}".format(i*4, 0x100+i*4, v)))

# Bubble sort
N = len(vals)
bsort.append((addi(1,0,0x100), "addi x1,  x0, 0x100  # base"))
bsort.append((addi(3,0,N-1),   "addi x3,  x0, {:d}     # i = N-1".format(N-1)))
# PC=outer_loop:
outer_pc = len(bsort)
bsort.append((addi(4,0,0),     "addi x4,  x0,  0      # j = 0"))
# PC=inner_loop:
inner_pc = len(bsort)
bsort.append((slli(8,4,2),     "slli x8,  x4,  2      # x8 = j*4"))
bsort.append((add(8,8,1),      "add  x8,  x8, x1      # x8 = base + j*4"))
bsort.append((lw(5,8,0),       "lw   x5,  0(x8)       # a = mem[j]"))
bsort.append((lw(6,8,4),       "lw   x6,  4(x8)       # b = mem[j+1]"))
bsort.append((bge(6,5,12),     "bge  x6, x5, +12      # if b>=a, no swap"))
bsort.append((sw_(8,6,0),      "sw   x6,  0(x8)       # mem[j]   = b"))
bsort.append((sw_(8,5,4),      "sw   x5,  4(x8)       # mem[j+1] = a"))
bsort.append((addi(4,4,1),     "addi x4,  x4,  1      # j++"))
inner_end = len(bsort)
inner_off = (inner_pc - inner_end) * 4
bsort.append((blt(4,3,inner_off), "blt  x4, x3, {:d}   # j<i => inner loop".format(inner_off)))
bsort.append((addi(3,3,-1),    "addi x3,  x3, -1      # i--"))
outer_end = len(bsort)
outer_off = (outer_pc - outer_end) * 4
bsort.append((bne(3,0,outer_off), "bne  x3, x0, {:d}   # i>0 => outer loop".format(outer_off)))
bsort.append((nop(),            "nop"))
bsort.append((nop(),            "nop"))
save(bsort, "bubble_sort.hex", "Bubble sort: sorts [5,3,8,1,9,2,7,4] -> [1,2,3,4,5,7,8,9] at 0x100")

# --------------------------------------------------------------------------
# 4. LOAD-USE HAZARD DEMO
# Intentional back-to-back load -> use sequences, maximising stall cycles
# --------------------------------------------------------------------------
hazard = [
    # Write test values into memory
    (addi(1,0,0x100), "addi x1, x0, 0x100  # base = 0x100"),
    (addi(2,0,42),   "addi x2,  x0, 42     # val = 42"),
    (sw_(1,2,0),     "sw   x2,  0(x1)      # mem[0] = 42"),
    (addi(2,0,99),   "addi x2,  x0, 99     # val = 99"),
    (sw_(1,2,4),     "sw   x2,  4(x1)      # mem[4] = 99"),
    (addi(2,0,7),    "addi x2,  x0,  7     # val = 7"),
    (sw_(1,2,8),     "sw   x2,  8(x1)      # mem[8] = 7"),
    (addi(2,0,13),   "addi x2,  x0, 13     # val = 13"),
    (sw_(1,2,12),    "sw   x2, 12(x1)      # mem[12] = 13"),
    # Load-use hazard chain (each load immediately used)
    (lw(3,1,0),      "lw   x3,  0(x1)      # LOAD x3 = 42"),
    (add(4,3,3),     "add  x4, x3, x3      # USE  x3 => stall 1 cycle"),
    (lw(5,1,4),      "lw   x5,  4(x1)      # LOAD x5 = 99"),
    (add(6,5,4),     "add  x6, x5, x4      # USE  x5 => stall 1 cycle"),
    (lw(7,1,8),      "lw   x7,  8(x1)      # LOAD x7 = 7"),
    (add(8,7,6),     "add  x8, x7, x6      # USE  x7 => stall 1 cycle"),
    (lw(9,1,12),     "lw   x9, 12(x1)      # LOAD x9 = 13"),
    (add(10,9,8),    "add x10, x9, x8      # USE  x9 => stall 1 cycle"),
    (sw_(1,10,16),   "sw  x10, 16(x1)      # store final result"),
    (nop(),          "nop"),
    (nop(),          "nop"),
]
save(hazard, "hazard.hex", "Hazard demo: 4 load-use stalls; watch stall_cycles counter rise")

# --------------------------------------------------------------------------
# 5. BRANCH PREDICTION STRESS
# 500-iteration loop; weak-not-taken initialization causes two mispredictions
# --------------------------------------------------------------------------
branch = [
    (addi(1,0,0),    "addi x1, x0,   0     # sum = 0"),
    (addi(2,0,500),  "addi x2, x0, 500     # n = 500"),
    (addi(3,0,1),    "addi x3, x0,   1     # step = 1"),
    # PC=12: loop
    (add(1,1,3),     "add  x1, x1, x3      # sum += step"),
    (addi(2,2,-1),   "addi x2, x2, -1      # n--"),
    (bne(2,0,-8),    "bne  x2, x0,  -8     # loop (taken 499 times, mispredict at 0)"),
    # loop ends; x1 = 500
    (sw_(0,1,0x100), "sw   x1, 256(x0)     # store sum = 500 at 0x100"),
    (nop(),          "nop"),
    (nop(),          "nop"),
]
save(branch, "branch_pred.hex",
     "Branch prediction: 500-iteration loop; observe first-taken and loop-exit mispredictions")
