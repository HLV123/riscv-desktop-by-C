#include "cpu.h"
#include "bus.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char* REG_ABI_NAMES[32] = {
    "zero","ra","sp","gp","tp","t0","t1","t2",
    "s0","s1","a0","a1","a2","a3","a4","a5",
    "a6","a7","s2","s3","s4","s5","s6","s7",
    "s8","s9","s10","s11","t3","t4","t5","t6"
};

#define OP_LUI     0x37
#define OP_AUIPC   0x17
#define OP_JAL     0x6F
#define OP_JALR    0x67
#define OP_BRANCH  0x63
#define OP_LOAD    0x03
#define OP_STORE   0x23
#define OP_IMM     0x13
#define OP_REG     0x33
#define OP_FENCE   0x0F
#define OP_SYSTEM  0x73

CPU* cpu_create(void) {
    CPU *c = (CPU*)calloc(1, sizeof(CPU));
    if (!c) {
        fprintf(stderr, "[CPU] Fatal: failed to allocate CPU\n");
        return NULL;
    }
    return c;
}

void cpu_destroy(CPU *cpu) {
    free(cpu);
}

void cpu_reset(CPU *cpu, uint32_t entry_point) {
    memset(cpu->regs, 0, sizeof(cpu->regs));
    cpu->pc        = entry_point;
    cpu->ir        = 0;
    cpu->cycle     = 0;
    cpu->halted    = false;
    cpu->exception = EXC_NONE;
    cpu->last_mem_addr  = 0;
    cpu->last_mem_write = false;
}

const char* cpu_reg_name(int idx) {
    if (idx < 0 || idx >= 32) return "??";
    return REG_ABI_NAMES[idx];
}

const char* cpu_exception_name(CPUException e) {
    switch(e) {
        case EXC_NONE:                return "None";
        case EXC_ILLEGAL_INSTRUCTION: return "Illegal Instruction";
        case EXC_ILLEGAL_ACCESS:      return "Illegal Access";
        case EXC_ALIGNMENT_FAULT:     return "Alignment Fault";
        case EXC_BUS_FAULT:           return "Bus Fault";
        case EXC_BREAKPOINT:          return "Breakpoint";
        case EXC_ECALL:               return "ECALL";
        default:                      return "Unknown";
    }
}

static inline int32_t sign_ext(uint32_t val, int bits) {
    int shift = 32 - bits;
    return (int32_t)(val << shift) >> shift;
}

DecodedInst cpu_decode(uint32_t raw) {
    DecodedInst d = {0};
    d.raw    = raw;
    d.opcode = raw & 0x7F;
    d.rd     = (raw >>  7) & 0x1F;
    d.funct3 = (raw >> 12) & 0x07;
    d.rs1    = (raw >> 15) & 0x1F;
    d.rs2    = (raw >> 20) & 0x1F;
    d.funct7 = (raw >> 25) & 0x7F;

    switch (d.opcode) {
        case OP_LUI:
        case OP_AUIPC:
            d.format = INST_U;
            d.imm = (int32_t)(raw & 0xFFFFF000);
            break;
        case OP_JAL:
            d.format = INST_J;
            {
                uint32_t imm20   = (raw >> 31) & 1;
                uint32_t imm101  = (raw >> 21) & 0x3FF;
                uint32_t imm11   = (raw >> 20) & 1;
                uint32_t imm1912 = (raw >> 12) & 0xFF;
                uint32_t u = (imm20 << 20)|(imm1912 << 12)|(imm11 << 11)|(imm101 << 1);
                d.imm = sign_ext(u, 21);
            }
            break;
        case OP_JALR:
        case OP_LOAD:
        case OP_IMM:
        case OP_FENCE:
        case OP_SYSTEM:
            d.format = INST_I;
            d.imm = sign_ext(raw >> 20, 12);
            break;
        case OP_STORE:
            d.format = INST_S;
            {
                uint32_t u = ((raw >> 25) << 5) | ((raw >> 7) & 0x1F);
                d.imm = sign_ext(u, 12);
            }
            break;
        case OP_BRANCH:
            d.format = INST_B;
            {
                uint32_t imm12  = (raw >> 31) & 1;
                uint32_t imm105 = (raw >> 25) & 0x3F;
                uint32_t imm41  = (raw >>  8) & 0xF;
                uint32_t imm11  = (raw >>  7) & 1;
                uint32_t u = (imm12<<12)|(imm11<<11)|(imm105<<5)|(imm41<<1);
                d.imm = sign_ext(u, 13);
            }
            break;
        case OP_REG:
            d.format = INST_R;
            d.imm = 0;
            break;
        default:
            d.format = INST_UNKNOWN;
            d.imm = 0;
            break;
    }
    return d;
}

const char* cpu_disassemble(DecodedInst *d, char *buf, size_t len) {
    #define RN(i) cpu_reg_name(i)
    switch (d->opcode) {
        case OP_LUI:   snprintf(buf,len,"lui     %s, 0x%x",   RN(d->rd), (uint32_t)d->imm >> 12); break;
        case OP_AUIPC: snprintf(buf,len,"auipc   %s, 0x%x",   RN(d->rd), (uint32_t)d->imm >> 12); break;
        case OP_JAL:
            if (d->rd == 0) snprintf(buf,len,"j       %+d", d->imm);
            else            snprintf(buf,len,"jal     %s, %+d", RN(d->rd), d->imm);
            break;
        case OP_JALR:  snprintf(buf,len,"jalr    %s, %s, %d", RN(d->rd), RN(d->rs1), d->imm); break;
        case OP_BRANCH: {
            const char *mn[] = {"beq","bne","??","??","blt","bge","bltu","bgeu"};
            snprintf(buf,len,"%-7s %s, %s, %+d", mn[d->funct3], RN(d->rs1), RN(d->rs2), d->imm);
            break;
        }
        case OP_LOAD: {
            const char *mn[] = {"lb","lh","lw","??","lbu","lhu","??","??"};
            snprintf(buf,len,"%-7s %s, %d(%s)", mn[d->funct3], RN(d->rd), d->imm, RN(d->rs1));
            break;
        }
        case OP_STORE: {
            const char *mn[] = {"sb","sh","sw","??","??","??","??","??"};
            snprintf(buf,len,"%-7s %s, %d(%s)", mn[d->funct3], RN(d->rs2), d->imm, RN(d->rs1));
            break;
        }
        case OP_IMM: {
            switch (d->funct3) {
                case 0: snprintf(buf,len,"addi    %s, %s, %d",  RN(d->rd), RN(d->rs1), d->imm); break;
                case 1: snprintf(buf,len,"slli    %s, %s, %d",  RN(d->rd), RN(d->rs1), d->rs2); break;
                case 2: snprintf(buf,len,"slti    %s, %s, %d",  RN(d->rd), RN(d->rs1), d->imm); break;
                case 3: snprintf(buf,len,"sltiu   %s, %s, %d",  RN(d->rd), RN(d->rs1), d->imm); break;
                case 4: snprintf(buf,len,"xori    %s, %s, %d",  RN(d->rd), RN(d->rs1), d->imm); break;
                case 5:
                    if (d->funct7 & 0x20) snprintf(buf,len,"srai    %s, %s, %d", RN(d->rd), RN(d->rs1), d->rs2);
                    else                  snprintf(buf,len,"srli    %s, %s, %d", RN(d->rd), RN(d->rs1), d->rs2);
                    break;
                case 6: snprintf(buf,len,"ori     %s, %s, %d",  RN(d->rd), RN(d->rs1), d->imm); break;
                case 7: snprintf(buf,len,"andi    %s, %s, %d",  RN(d->rd), RN(d->rs1), d->imm); break;
            }
            break;
        }
        case OP_REG: {
            const char *mn_f7_00[] = {"add","sll","slt","sltu","xor","srl","or","and"};
            const char *mn_f7_20[] = {"sub","???","???","???", "???","sra","??","??"};
            const char **mn = (d->funct7 & 0x20) ? mn_f7_20 : mn_f7_00;
            snprintf(buf,len,"%-7s %s, %s, %s", mn[d->funct3], RN(d->rd), RN(d->rs1), RN(d->rs2));
            break;
        }
        case OP_SYSTEM:
            if (d->imm == 0) snprintf(buf,len,"ecall");
            else             snprintf(buf,len,"ebreak");
            break;
        case OP_FENCE:
            snprintf(buf,len,"fence");
            break;
        default:
            if (d->raw == 0x00000013) snprintf(buf,len,"nop");
            else snprintf(buf,len,"??? (0x%08X)", d->raw);
            break;
    }
    return buf;
}

int cpu_step(CPU *cpu, Bus *bus) {
    if (cpu->halted) return -1;

    if (cpu->pc % 4 != 0) {
        cpu->exception = EXC_ALIGNMENT_FAULT;
        cpu->halted = true;
        return -1;
    }
    cpu->ir = bus_read(bus, cpu->pc, ACCESS_WORD);
    if (bus->last_error != BUS_OK) {
        cpu->exception = EXC_BUS_FAULT;
        cpu->halted = true;
        return -1;
    }

    DecodedInst inst = cpu_decode(cpu->ir);
    if (inst.format == INST_UNKNOWN) {
        cpu->exception = EXC_ILLEGAL_INSTRUCTION;
        cpu->halted = true;
        return -1;
    }

    uint32_t pc_next = cpu->pc + 4;
    uint32_t rs1_val = cpu->regs[inst.rs1];
    uint32_t rs2_val = cpu->regs[inst.rs2];

    switch (inst.opcode) {
        case OP_LUI:
            cpu->regs[inst.rd] = (uint32_t)inst.imm;
            break;

        case OP_AUIPC:
            cpu->regs[inst.rd] = cpu->pc + (uint32_t)inst.imm;
            break;

        case OP_JAL:
            cpu->regs[inst.rd] = pc_next;
            pc_next = cpu->pc + (uint32_t)inst.imm;
            break;

        case OP_JALR:
            cpu->regs[inst.rd] = pc_next;
            pc_next = (rs1_val + (uint32_t)inst.imm) & ~1u;
            break;

        case OP_BRANCH: {
            bool taken = false;
            switch (inst.funct3) {
                case 0: taken = (rs1_val == rs2_val); break;
                case 1: taken = (rs1_val != rs2_val); break;
                case 4: taken = ((int32_t)rs1_val  < (int32_t)rs2_val); break;
                case 5: taken = ((int32_t)rs1_val >= (int32_t)rs2_val); break;
                case 6: taken = (rs1_val  < rs2_val); break;
                case 7: taken = (rs1_val >= rs2_val); break;
                default:
                    cpu->exception = EXC_ILLEGAL_INSTRUCTION;
                    cpu->halted = true;
                    return -1;
            }
            if (taken) pc_next = cpu->pc + (uint32_t)inst.imm;
            break;
        }

        case OP_LOAD: {
            uint32_t addr = rs1_val + (uint32_t)inst.imm;
            cpu->last_mem_addr  = addr;
            cpu->last_mem_write = false;
            uint32_t val;
            switch (inst.funct3) {
                case 0: val = (uint32_t)sign_ext(bus_read(bus,addr,ACCESS_BYTE), 8);  break;
                case 1: val = (uint32_t)sign_ext(bus_read(bus,addr,ACCESS_HALF), 16); break;
                case 2: val = bus_read(bus,addr,ACCESS_WORD); break;
                case 4: val = bus_read(bus,addr,ACCESS_BYTE) & 0xFF; break;
                case 5: val = bus_read(bus,addr,ACCESS_HALF) & 0xFFFF; break;
                default:
                    cpu->exception = EXC_ILLEGAL_INSTRUCTION;
                    cpu->halted = true;
                    return -1;
            }
            if (bus->last_error != BUS_OK) {
                cpu->exception = EXC_BUS_FAULT;
                cpu->halted = true;
                return -1;
            }
            cpu->regs[inst.rd] = val;
            break;
        }

        case OP_STORE: {
            uint32_t addr = rs1_val + (uint32_t)inst.imm;
            cpu->last_mem_addr  = addr;
            cpu->last_mem_write = true;
            switch (inst.funct3) {
                case 0: bus_write(bus,addr,rs2_val,ACCESS_BYTE); break;
                case 1: bus_write(bus,addr,rs2_val,ACCESS_HALF); break;
                case 2: bus_write(bus,addr,rs2_val,ACCESS_WORD); break;
                default:
                    cpu->exception = EXC_ILLEGAL_INSTRUCTION;
                    cpu->halted = true;
                    return -1;
            }
            if (bus->last_error != BUS_OK) {
                cpu->exception = EXC_BUS_FAULT;
                cpu->halted = true;
                return -1;
            }
            break;
        }

        case OP_IMM: {
            uint32_t shamt = inst.rs2; 
            switch (inst.funct3) {
                case 0: cpu->regs[inst.rd] = rs1_val + (uint32_t)inst.imm; break;
                case 1: cpu->regs[inst.rd] = rs1_val << shamt; break;
                case 2: cpu->regs[inst.rd] = ((int32_t)rs1_val < inst.imm) ? 1 : 0; break;
                case 3: cpu->regs[inst.rd] = (rs1_val < (uint32_t)inst.imm) ? 1 : 0; break;
                case 4: cpu->regs[inst.rd] = rs1_val ^ (uint32_t)inst.imm; break;
                case 5:
                    if (inst.funct7 & 0x20)
                        cpu->regs[inst.rd] = (uint32_t)((int32_t)rs1_val >> shamt);
                    else
                        cpu->regs[inst.rd] = rs1_val >> shamt;
                    break;
                case 6: cpu->regs[inst.rd] = rs1_val | (uint32_t)inst.imm; break;
                case 7: cpu->regs[inst.rd] = rs1_val & (uint32_t)inst.imm; break;
            }
            break;
        }

        case OP_REG: {
            uint32_t shamt = rs2_val & 0x1F;
            if (inst.funct7 & 0x20) {
                switch (inst.funct3) {
                    case 0: cpu->regs[inst.rd] = rs1_val - rs2_val; break;
                    case 5:
                        cpu->regs[inst.rd] = (uint32_t)((int32_t)rs1_val >> shamt); break;
                    default:
                        cpu->exception = EXC_ILLEGAL_INSTRUCTION;
                        cpu->halted = true;
                        return -1;
                }
            } else {
                switch (inst.funct3) {
                    case 0: cpu->regs[inst.rd] = rs1_val + rs2_val; break;
                    case 1: cpu->regs[inst.rd] = rs1_val << shamt; break;
                    case 2: cpu->regs[inst.rd] = ((int32_t)rs1_val < (int32_t)rs2_val) ? 1 : 0; break;
                    case 3: cpu->regs[inst.rd] = (rs1_val < rs2_val) ? 1 : 0; break;
                    case 4: cpu->regs[inst.rd] = rs1_val ^ rs2_val; break;
                    case 5: cpu->regs[inst.rd] = rs1_val >> shamt; break;
                    case 6: cpu->regs[inst.rd] = rs1_val | rs2_val; break;
                    case 7: cpu->regs[inst.rd] = rs1_val & rs2_val; break;
                }
            }
            break;
        }

        case OP_FENCE:
            break;

        case OP_SYSTEM:
            if (inst.imm == 0) {
                cpu->exception = EXC_ECALL;
                if (cpu->regs[17] == 93) {
                    cpu->halted = true;
                    return -1;
                }
            } else {
                cpu->exception = EXC_BREAKPOINT;
                cpu->halted = true;
                return -1;
            }
            break;

        default:
            cpu->exception = EXC_ILLEGAL_INSTRUCTION;
            cpu->halted = true;
            return -1;
    }

    cpu->regs[0] = 0;    
    cpu->pc      = pc_next;
    cpu->cycle++;

    return 0;
}

void cpu_dump(const CPU *cpu, char *buf, size_t len) {
    int off = 0;
    off += snprintf(buf+off, len-off, "PC=0x%08X  Cycle=%llu\n",
                    cpu->pc, (unsigned long long)cpu->cycle);
    for (int i = 0; i < 32; i++) {
        off += snprintf(buf+off, len-off, "x%-2d %-4s = 0x%08X%s",
                        i, REG_ABI_NAMES[i], cpu->regs[i],
                        (i%4==3) ? "\n" : "  ");
    }
}
