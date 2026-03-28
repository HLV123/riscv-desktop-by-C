#ifndef PERIPHERAL_H
#define PERIPHERAL_H

#include <stdint.h>
#include <stdbool.h>
#include "memory.h"

typedef struct Peripheral Peripheral;

typedef struct {
    uint32_t (*read) (Peripheral *p, uint32_t offset, AccessSize sz);
    void     (*write)(Peripheral *p, uint32_t offset, uint32_t val, AccessSize sz);
    void     (*tick) (Peripheral *p, uint64_t cycle);
    void     (*reset)(Peripheral *p);
    void     (*destroy)(Peripheral *p);
} PeripheralOps;

struct Peripheral {
    PeripheralOps  ops;
    uint32_t       base_addr;
    uint32_t       size;
    const char    *name;
    void          *priv;
};

#define PERIPH_READ(p, off, sz)      ((p)->ops.read((p),(off),(sz)))
#define PERIPH_WRITE(p, off, v, sz)  ((p)->ops.write((p),(off),(v),(sz)))
#define PERIPH_TICK(p, cyc)          do { if((p)->ops.tick) (p)->ops.tick((p),(cyc)); } while(0)
#define PERIPH_RESET(p)              do { if((p)->ops.reset) (p)->ops.reset((p)); } while(0)
#define PERIPH_DESTROY(p)            do { if((p)->ops.destroy) (p)->ops.destroy((p)); } while(0)

#endif 
