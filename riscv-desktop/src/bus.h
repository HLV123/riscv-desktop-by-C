#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include <stdbool.h>
#include "memory.h"
#include "peripheral/peripheral.h"

#define BUS_MAX_REGIONS     8
#define BUS_MAX_PERIPHERALS 8

typedef enum {
    BUS_OK         = 0,
    BUS_FAULT      = 1,
    BUS_ACCESS_VIO = 2,
    BUS_ALIGN_FAULT= 3,
} BusError;

struct Bus {
    MemRegion   *regions[BUS_MAX_REGIONS];
    int          region_count;
    Peripheral  *peripherals[BUS_MAX_PERIPHERALS];
    int          periph_count;
    BusError     last_error;
    uint32_t     last_fault_addr;
};
typedef struct Bus Bus;

Bus*     bus_create(void);
void     bus_destroy(Bus *bus);
void     bus_add_region(Bus *bus, MemRegion *mem);
void     bus_add_peripheral(Bus *bus, Peripheral *p);

uint32_t bus_read(Bus *bus, uint32_t addr, AccessSize size);
void     bus_write(Bus *bus, uint32_t addr, uint32_t data, AccessSize size);
void     bus_tick_all(Bus *bus, uint64_t cycle);

#endif 
