#include "uart.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    UARTPriv base;
    uint64_t last_cycle;
} UARTPrivExt;

static uint32_t uart_read(Peripheral *p, uint32_t offset, AccessSize sz) {
    UARTPrivExt *ue = (UARTPrivExt*)p->priv;
    UARTPriv    *u  = &ue->base;
    (void)sz;
    switch (offset) {
        case UART_DR:  return 0;     
        case UART_SR:  return UART_SR_TX_READY; 
        case UART_CR:  return u->cr;
        default: return 0;
    }
}

static void uart_write(Peripheral *p, uint32_t offset, uint32_t val, AccessSize sz) {
    UARTPrivExt *ue = (UARTPrivExt*)p->priv;
    UARTPriv    *u  = &ue->base;
    (void)sz;
    switch (offset) {
        case UART_DR:
            if (u->cr & UART_CR_EN) {
                u->buf[u->head] = (char)(val & 0xFF);
                u->head = (u->head + 1) & (UART_BUF_SIZE - 1);
                if (u->count < UART_BUF_SIZE) {
                    u->count++;
                } else {
                    u->tail = (u->tail + 1) & (UART_BUF_SIZE - 1);
                }
                wave_push(u->wave_tx, ue->last_cycle, 1.0f);
                wave_push(u->wave_tx, ue->last_cycle + 8, 0.0f);
            }
            break;
        case UART_CR:
            u->cr = val & 0xFF;
            break;
        default: break;
    }
}

static void uart_tick(Peripheral *p, uint64_t cycle) {
    UARTPrivExt *ue = (UARTPrivExt*)p->priv;
    ue->last_cycle = cycle;
}

static void uart_reset_fn(Peripheral *p) {
    UARTPrivExt *ue = (UARTPrivExt*)p->priv;
    UARTPriv    *u  = &ue->base;
    u->cr   = 0;
    u->head = 0;
    u->tail = 0;
    u->count = 0;
    wave_clear(u->wave_tx);
}

static void uart_destroy_fn(Peripheral *p) {
    UARTPrivExt *ue = (UARTPrivExt*)p->priv;
    wave_destroy(ue->base.wave_tx);
    free(ue);
    free(p);
}

Peripheral* uart_create(void) {
    Peripheral  *p  = (Peripheral*)calloc(1, sizeof(Peripheral));
    UARTPrivExt *ue = (UARTPrivExt*)calloc(1, sizeof(UARTPrivExt));
    if (!p || !ue) {
        fprintf(stderr, "[UART] Fatal: failed to allocate UART peripheral\n");
        free(p);
        free(ue);
        return NULL;
    }
    UARTPriv    *u  = &ue->base;

    u->wave_tx = wave_create("UART_TX", WAVE_DEFAULT_CAP, true);
    u->cr      = UART_CR_EN;  

    p->ops.read    = uart_read;
    p->ops.write   = uart_write;
    p->ops.tick    = uart_tick;
    p->ops.reset   = uart_reset_fn;
    p->ops.destroy = uart_destroy_fn;
    p->base_addr   = UART_BASE_ADDR;
    p->size        = 0x10;
    p->name        = "UART";
    p->priv        = ue;

    return p;
}

const char* uart_get_buf(Peripheral *p, int *len) {
    UARTPrivExt *ue = (UARTPrivExt*)p->priv;
    UARTPriv    *u  = &ue->base;
    if (len) *len = u->count;
    static char snapshot[UART_BUF_SIZE + 1];
    int n = u->count;
    for (int i = 0; i < n; i++) {
        snapshot[i] = u->buf[(u->tail + i) & (UART_BUF_SIZE - 1)];
    }
    snapshot[n] = '\0';
    return snapshot;
}

int uart_read_buf(Peripheral *p, char *out, int max_len) {
    UARTPrivExt *ue = (UARTPrivExt*)p->priv;
    UARTPriv    *u  = &ue->base;
    int n = u->count < max_len - 1 ? u->count : max_len - 1;
    for (int i = 0; i < n; i++) {
        out[i] = u->buf[u->tail];
        u->tail = (u->tail + 1) & (UART_BUF_SIZE - 1);
        u->count--;
    }
    out[n] = '\0';
    return n;
}

void uart_clear_buf(Peripheral *p) {
    UARTPrivExt *ue = (UARTPrivExt*)p->priv;
    UARTPriv    *u  = &ue->base;
    u->head  = 0;
    u->tail  = 0;
    u->count = 0;
}

WaveChannel* uart_get_wave(Peripheral *p) {
    UARTPrivExt *ue = (UARTPrivExt*)p->priv;
    return ue->base.wave_tx;
}
