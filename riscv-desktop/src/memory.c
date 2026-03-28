#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

MemRegion* mem_create(uint32_t base, uint32_t size, bool readonly, const char *name) {
    MemRegion *m = (MemRegion*)calloc(1, sizeof(MemRegion));
    if (!m) {
        fprintf(stderr, "[MEM] Fatal: failed to allocate MemRegion '%s'\n", name ? name : "?");
        return NULL;
    }
    m->data = (uint8_t*)calloc(1, size);
    if (!m->data) {
        fprintf(stderr, "[MEM] Fatal: failed to allocate %u bytes for '%s'\n", size, name ? name : "?");
        free(m);
        return NULL;
    }
    m->base     = base;
    m->size     = size;
    m->readonly = readonly;
    m->name     = name;
    return m;
}

void mem_destroy(MemRegion *mem) {
    if (!mem) return;
    free(mem->data);
    free(mem);
}

bool mem_load(MemRegion *mem, const uint8_t *data, size_t len) {
    if (len > mem->size) return false;
    memcpy(mem->data, data, len);
    return true;
}

void mem_clear(MemRegion *mem) {
    memset(mem->data, 0, mem->size);
}

bool mem_contains(MemRegion *mem, uint32_t addr) {
    return (addr >= mem->base) && (addr < mem->base + mem->size);
}

static uint32_t mem_local_offset(MemRegion *mem, uint32_t addr) {
    return addr - mem->base;
}

uint32_t mem_read(MemRegion *mem, uint32_t addr, AccessSize sz) {
    uint32_t off = mem_local_offset(mem, addr);
    if (off + sz > mem->size) return 0;
    switch (sz) {
        case ACCESS_BYTE: return mem->data[off];
        case ACCESS_HALF: {
            uint32_t v = mem->data[off] | ((uint32_t)mem->data[off+1] << 8);
            return v;
        }
        case ACCESS_WORD: {
            uint32_t v = mem->data[off]
                       | ((uint32_t)mem->data[off+1] << 8)
                       | ((uint32_t)mem->data[off+2] << 16)
                       | ((uint32_t)mem->data[off+3] << 24);
            return v;
        }
    }
    return 0;
}

bool mem_write(MemRegion *mem, uint32_t addr, uint32_t val, AccessSize sz) {
    if (mem->readonly) return false;
    uint32_t off = mem_local_offset(mem, addr);
    if (off + sz > mem->size) return false;
    switch (sz) {
        case ACCESS_BYTE:
            mem->data[off] = val & 0xFF;
            break;
        case ACCESS_HALF:
            mem->data[off]   = val & 0xFF;
            mem->data[off+1] = (val >> 8) & 0xFF;
            break;
        case ACCESS_WORD:
            mem->data[off]   = val & 0xFF;
            mem->data[off+1] = (val >> 8) & 0xFF;
            mem->data[off+2] = (val >> 16) & 0xFF;
            mem->data[off+3] = (val >> 24) & 0xFF;
            break;
    }
    return true;
}
