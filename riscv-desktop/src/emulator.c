#include "emulator.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Emulator* emu_create(void) {
    Emulator *e = (Emulator*)calloc(1, sizeof(Emulator));
    if (!e) {
        fprintf(stderr, "[EMU] Fatal: failed to allocate Emulator\n");
        return NULL;
    }

    e->cpu   = cpu_create();
    e->bus   = bus_create();
    e->rom   = mem_create(ROM_BASE, ROM_SIZE, true,  "ROM");
    e->ram   = mem_create(RAM_BASE, RAM_SIZE, false, "RAM");
    e->gpio  = gpio_create();
    e->timer = timer_create();
    e->uart  = uart_create();

    if (!e->cpu || !e->bus || !e->rom || !e->ram ||
        !e->gpio || !e->timer || !e->uart) {
        fprintf(stderr, "[EMU] Fatal: one or more sub-components failed to allocate\n");
        emu_destroy(e);
        return NULL;
    }

    bus_add_region(e->bus, e->rom);
    bus_add_region(e->bus, e->ram);
    bus_add_peripheral(e->bus, e->gpio);
    bus_add_peripheral(e->bus, e->timer);
    bus_add_peripheral(e->bus, e->uart);

    e->state            = EMU_IDLE;
    e->cycles_per_frame = 50000; 
    e->bp_count         = 0;

    return e;
}

void emu_destroy(Emulator *emu) {
    if (!emu) return;
    cpu_destroy(emu->cpu);
    mem_destroy(emu->rom);
    mem_destroy(emu->ram);
    PERIPH_DESTROY(emu->gpio);
    PERIPH_DESTROY(emu->timer);
    PERIPH_DESTROY(emu->uart);
    bus_destroy(emu->bus);
    free(emu);
}

void emu_reset(Emulator *emu) {
    cpu_reset(emu->cpu, ROM_BASE);
    mem_clear(emu->ram);
    PERIPH_RESET(emu->gpio);
    PERIPH_RESET(emu->timer);
    PERIPH_RESET(emu->uart);
    emu->bus->last_error = BUS_OK;
    emu->total_cycles    = 0;
    emu->state           = EMU_IDLE;
}

bool emu_load_binary(Emulator *emu, const uint8_t *data, size_t len) {
    if (len > ROM_SIZE) {
        fprintf(stderr, "[EMU] Binary too large: %zu > %d\n", len, ROM_SIZE);
        return false;
    }
    mem_clear(emu->rom);
    emu->rom->readonly = false;
    mem_load(emu->rom, data, len);
    emu->rom->readonly = true;
    cpu_reset(emu->cpu, ROM_BASE);
    emu->state = EMU_PAUSED;
    return true;
}

bool emu_load_file(Emulator *emu, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[EMU] Cannot open: %s\n", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0 || len > ROM_SIZE) {
        fclose(f);
        fprintf(stderr, "[EMU] File size invalid: %ld\n", len);
        return false;
    }

    uint8_t *buf = (uint8_t*)malloc((size_t)len);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "[EMU] Cannot allocate %ld bytes for binary\n", len);
        return false;
    }
    size_t read_n = fread(buf, 1, (size_t)len, f);
    fclose(f);
    if ((long)read_n != len) {
        fprintf(stderr, "[EMU] Short read: expected %ld bytes, got %zu\n", len, read_n);
        free(buf);
        return false;
    }

    bool ok = emu_load_binary(emu, buf, (size_t)len);
    free(buf);
    return ok;
}

void emu_step(Emulator *emu) {
    if (emu->state == EMU_HALTED || emu->state == EMU_ERROR) return;
    emu->state = EMU_PAUSED;

    int r = cpu_step(emu->cpu, emu->bus);
    bus_tick_all(emu->bus, emu->cpu->cycle);
    emu->total_cycles++;

    if (r != 0) {
        if (emu->cpu->exception == EXC_BREAKPOINT) {
            emu->state = EMU_PAUSED;
        } else {
            emu->state = EMU_HALTED;
        }
    }
}

void emu_run_cycles(Emulator *emu, int n) {
    if (emu->state != EMU_RUNNING) return;

    for (int i = 0; i < n; i++) {
        int r = cpu_step(emu->cpu, emu->bus);
        bus_tick_all(emu->bus, emu->cpu->cycle);
        emu->total_cycles++;

        if (r != 0) {
            emu->state = EMU_HALTED;
            return;
        }

        if (emu_check_breakpoint(emu)) {
            emu->state = EMU_PAUSED;
            return;
        }
    }
}

void emu_set_state(Emulator *emu, EmuState s) {
    emu->state = s;
}

void emu_add_breakpoint(Emulator *emu, uint32_t addr) {
    if (emu->bp_count >= MAX_BREAKPOINTS) return;
    emu->breakpoints[emu->bp_count] = addr;
    emu->bp_enabled[emu->bp_count]  = true;
    emu->bp_count++;
}

void emu_remove_breakpoint(Emulator *emu, uint32_t addr) {
    for (int i = 0; i < emu->bp_count; i++) {
        if (emu->breakpoints[i] == addr) {
            for (int j = i; j < emu->bp_count - 1; j++) {
                emu->breakpoints[j] = emu->breakpoints[j+1];
                emu->bp_enabled[j]  = emu->bp_enabled[j+1];
            }
            emu->bp_count--;
            return;
        }
    }
}

bool emu_has_breakpoint(Emulator *emu, uint32_t addr) {
    for (int i = 0; i < emu->bp_count; i++) {
        if (emu->breakpoints[i] == addr && emu->bp_enabled[i]) return true;
    }
    return false;
}

void emu_toggle_breakpoint(Emulator *emu, uint32_t addr) {
    if (emu_has_breakpoint(emu, addr)) {
        emu_remove_breakpoint(emu, addr);
    } else {
        emu_add_breakpoint(emu, addr);
    }
}

bool emu_check_breakpoint(Emulator *emu) {
    return emu_has_breakpoint(emu, emu->cpu->pc);
}
