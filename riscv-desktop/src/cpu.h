#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    EXC_NONE                = 0,
    EXC_ILLEGAL_INSTRUCTION = 1,
    EXC_ILLEGAL_ACCESS      = 2,
    EXC_ALIGNMENT_FAULT     = 3,
    EXC_BUS_FAULT           = 4,
    EXC_BREAKPOINT          = 5,
    EXC_ECALL               = 8,
} CPUException;

typedef enum {
    INST_R, INST_I, INST_S, INST_B, INST_U, INST_J, INST_UNKNOWN
} InstFormat;

typedef struct {
    uint32_t   raw;
    InstFormat format;
    uint8_t    opcode;
    uint8_t    rd;
    uint8_t    rs1;
    uint8_t    rs2;
    uint8_t    funct3;
    uint8_t    funct7;
    int32_t    imm;
} DecodedInst;

struct Bus;

typedef struct CPU {
    uint32_t     regs[32];
    uint32_t     pc;
    uint32_t     ir;
    uint64_t     cycle;
    bool         halted;
    CPUException exception;
    uint32_t     last_mem_addr;
    bool         last_mem_write;
} CPU;

CPU*        cpu_create(void);
void        cpu_destroy(CPU *cpu);
void        cpu_reset(CPU *cpu, uint32_t entry_point);
int         cpu_step(CPU *cpu, struct Bus *bus);
const char* cpu_reg_name(int idx);
const char* cpu_exception_name(CPUException e);
void        cpu_dump(const CPU *cpu, char *buf, size_t len);

DecodedInst cpu_decode(uint32_t raw);
const char* cpu_disassemble(DecodedInst *inst, char *buf, size_t len);

#endif 
