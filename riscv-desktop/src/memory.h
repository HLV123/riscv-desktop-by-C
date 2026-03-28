#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    ACCESS_BYTE = 1,
    ACCESS_HALF = 2,
    ACCESS_WORD = 4
} AccessSize;

typedef struct {
    uint8_t  *data;
    uint32_t  base;
    uint32_t  size;
    bool      readonly;
    const char *name;
} MemRegion;

MemRegion* mem_create(uint32_t base, uint32_t size, bool readonly, const char *name);
void       mem_destroy(MemRegion *mem);
bool       mem_load(MemRegion *mem, const uint8_t *data, size_t len);
void       mem_clear(MemRegion *mem);

uint32_t   mem_read(MemRegion *mem, uint32_t addr, AccessSize sz);
bool       mem_write(MemRegion *mem, uint32_t addr, uint32_t val, AccessSize sz);
bool       mem_contains(MemRegion *mem, uint32_t addr);

#endif 
