#ifndef EMULATOR_H
#define EMULATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "cpu.h"
#include "bus.h"
#include "memory.h"
#include "waveform.h"
#include "peripheral/gpio.h"
#include "peripheral/timer.h"
#include "peripheral/uart.h"

#define ROM_BASE     0x00000000
#define ROM_SIZE     (64 * 1024)
#define RAM_BASE     0x20000000
#define RAM_SIZE     (64 * 1024)

#define MAX_BREAKPOINTS 16

typedef enum {
    EMU_IDLE,
    EMU_RUNNING,
    EMU_PAUSED,
    EMU_HALTED,
    EMU_ERROR
} EmuState;

typedef struct {
    CPU        *cpu;
    Bus        *bus;
    MemRegion  *rom;
    MemRegion  *ram;

    Peripheral *gpio;
    Peripheral *timer;
    Peripheral *uart;

    EmuState    state;
    uint64_t    total_cycles;
    uint32_t    cycles_per_frame;
    uint32_t    breakpoints[MAX_BREAKPOINTS];
    bool        bp_enabled[MAX_BREAKPOINTS];
    int         bp_count;
    uint64_t    cycles_last_sec;
    double      mips;
} Emulator;

Emulator* emu_create(void);
void      emu_destroy(Emulator *emu);
void      emu_reset(Emulator *emu);
bool      emu_load_binary(Emulator *emu, const uint8_t *data, size_t len);
bool      emu_load_file(Emulator *emu, const char *path);
void      emu_step(Emulator *emu);
void      emu_run_cycles(Emulator *emu, int n);
void      emu_set_state(Emulator *emu, EmuState s);
void      emu_add_breakpoint(Emulator *emu, uint32_t addr);
void      emu_remove_breakpoint(Emulator *emu, uint32_t addr);
bool      emu_has_breakpoint(Emulator *emu, uint32_t addr);
void      emu_toggle_breakpoint(Emulator *emu, uint32_t addr);
bool      emu_check_breakpoint(Emulator *emu);

#endif 
