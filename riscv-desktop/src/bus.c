#include "bus.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Bus* bus_create(void) {
    Bus *b = (Bus*)calloc(1, sizeof(Bus));
    if (!b) {
        fprintf(stderr, "[BUS] Fatal: failed to allocate Bus\n");
        return NULL;
    }
    return b;
}

void bus_destroy(Bus *bus) {
    free(bus);
}

void bus_add_region(Bus *bus, MemRegion *mem) {
    if (bus->region_count >= BUS_MAX_REGIONS) return;
    bus->regions[bus->region_count++] = mem;
}

void bus_add_peripheral(Bus *bus, Peripheral *p) {
    if (bus->periph_count >= BUS_MAX_PERIPHERALS) return;
    bus->peripherals[bus->periph_count++] = p;
}

uint32_t bus_read(Bus *bus, uint32_t addr, AccessSize size) {
    bus->last_error = BUS_OK;

    for (int i = 0; i < bus->region_count; i++) {
        MemRegion *m = bus->regions[i];
        if (mem_contains(m, addr)) {
            return mem_read(m, addr, size);
        }
    }

    for (int i = 0; i < bus->periph_count; i++) {
        Peripheral *p = bus->peripherals[i];
        if (addr >= p->base_addr && addr < p->base_addr + p->size) {
            return PERIPH_READ(p, addr - p->base_addr, size);
        }
    }

    bus->last_error     = BUS_FAULT;
    bus->last_fault_addr = addr;
    fprintf(stderr, "[BUS] Read fault @ 0x%08X\n", addr);
    return 0xDEADBEEF;
}

void bus_write(Bus *bus, uint32_t addr, uint32_t data, AccessSize size) {
    bus->last_error = BUS_OK;

    for (int i = 0; i < bus->region_count; i++) {
        MemRegion *m = bus->regions[i];
        if (mem_contains(m, addr)) {
            if (!mem_write(m, addr, data, size)) {
                bus->last_error = BUS_ACCESS_VIO;
                fprintf(stderr, "[BUS] Write to ROM @ 0x%08X\n", addr);
            }
            return;
        }
    }

    for (int i = 0; i < bus->periph_count; i++) {
        Peripheral *p = bus->peripherals[i];
        if (addr >= p->base_addr && addr < p->base_addr + p->size) {
            PERIPH_WRITE(p, addr - p->base_addr, data, size);
            return;
        }
    }

    bus->last_error      = BUS_FAULT;
    bus->last_fault_addr = addr;
    fprintf(stderr, "[BUS] Write fault @ 0x%08X\n", addr);
}

void bus_tick_all(Bus *bus, uint64_t cycle) {
    for (int i = 0; i < bus->periph_count; i++) {
        PERIPH_TICK(bus->peripherals[i], cycle);
    }
}
