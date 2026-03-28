#include "gpio.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint32_t gpio_read(Peripheral *p, uint32_t offset, AccessSize sz) {
    GPIOPriv *g = (GPIOPriv*)p->priv;
    (void)sz;
    switch (offset) {
        case GPIO_PORTA_DIR: return g->dir[0];
        case GPIO_PORTA_OUT: return g->out[0];
        case GPIO_PORTA_IN:  return g->in[0];
        case GPIO_PORTA_IE:  return g->ie[0];
        case GPIO_PORTB_DIR: return g->dir[1];
        case GPIO_PORTB_OUT: return g->out[1];
        case GPIO_PORTB_IN:  return g->in[1];
        case GPIO_PORTB_IE:  return g->ie[1];
        default: return 0;
    }
}

static void gpio_update_wave(GPIOPriv *g, uint64_t cycle, int port, uint8_t old_val, uint8_t new_val) {
    uint8_t changed = old_val ^ new_val;
    for (int bit = 0; bit < 8; bit++) {
        if (changed & (1 << bit)) {
            int pin_idx = port * 8 + bit;
            float val = (new_val >> bit) & 1 ? 1.0f : 0.0f;
            wave_push(g->wave[pin_idx], cycle, val);
        }
    }
}

typedef struct {
    GPIOPriv base;
    uint64_t last_cycle;
} GPIOPrivExt;

static void gpio_write(Peripheral *p, uint32_t offset, uint32_t val, AccessSize sz) {
    GPIOPrivExt *ge = (GPIOPrivExt*)p->priv;
    GPIOPriv    *g  = &ge->base;
    (void)sz;
    uint8_t old;
    switch (offset) {
        case GPIO_PORTA_DIR: g->dir[0] = val & 0xFF; break;
        case GPIO_PORTA_OUT:
            old = g->out[0];
            g->out[0] = val & 0xFF;
            gpio_update_wave(g, ge->last_cycle, 0, old, g->out[0]);
            break;
        case GPIO_PORTA_IN:  /* read-only */ break;
        case GPIO_PORTA_IE:  g->ie[0] = val & 0xFF; break;
        case GPIO_PORTB_DIR: g->dir[1] = val & 0xFF; break;
        case GPIO_PORTB_OUT:
            old = g->out[1];
            g->out[1] = val & 0xFF;
            gpio_update_wave(g, ge->last_cycle, 1, old, g->out[1]);
            break;
        case GPIO_PORTB_IN:  /* read-only */ break;
        case GPIO_PORTB_IE:  g->ie[1] = val & 0xFF; break;
        default: break;
    }
}

static void gpio_tick(Peripheral *p, uint64_t cycle) {
    GPIOPrivExt *ge = (GPIOPrivExt*)p->priv;
    ge->last_cycle = cycle;
}

static void gpio_reset_fn(Peripheral *p) {
    GPIOPrivExt *ge = (GPIOPrivExt*)p->priv;
    GPIOPriv    *g  = &ge->base;
    memset(g->dir, 0, sizeof(g->dir));
    memset(g->out, 0, sizeof(g->out));
    memset(g->in,  0, sizeof(g->in));
    memset(g->ie,  0, sizeof(g->ie));
    for (int i = 0; i < GPIO_NUM_PINS; i++) wave_clear(g->wave[i]);
}

static void gpio_destroy_fn(Peripheral *p) {
    GPIOPrivExt *ge = (GPIOPrivExt*)p->priv;
    GPIOPriv    *g  = &ge->base;
    for (int i = 0; i < GPIO_NUM_PINS; i++) wave_destroy(g->wave[i]);
    free(ge);
    free(p);
}

Peripheral* gpio_create(void) {
    Peripheral  *p  = (Peripheral*)calloc(1, sizeof(Peripheral));
    GPIOPrivExt *ge = (GPIOPrivExt*)calloc(1, sizeof(GPIOPrivExt));
    if (!p || !ge) {
        fprintf(stderr, "[GPIO] Fatal: failed to allocate GPIO peripheral\n");
        free(p);
        free(ge);
        return NULL;
    }
    GPIOPriv    *g  = &ge->base;

    for (int i = 0; i < GPIO_NUM_PINS; i++) {
        char name[32];
        snprintf(name, sizeof(name), "GPIO_P%c%d", i<8?'A':'B', i%8);
        g->wave[i] = wave_create(name, WAVE_DEFAULT_CAP, true);
    }

    p->ops.read    = gpio_read;
    p->ops.write   = gpio_write;
    p->ops.tick    = gpio_tick;
    p->ops.reset   = gpio_reset_fn;
    p->ops.destroy = gpio_destroy_fn;
    p->base_addr   = GPIO_BASE_ADDR;
    p->size        = 0x20;
    p->name        = "GPIO";
    p->priv        = ge;

    return p;
}

void gpio_inject_input(Peripheral *p, int port, uint8_t val) {
    GPIOPrivExt *ge = (GPIOPrivExt*)p->priv;
    ge->base.in[port & 1] = val;
}

uint8_t gpio_get_output(Peripheral *p, int port) {
    GPIOPrivExt *ge = (GPIOPrivExt*)p->priv;
    return ge->base.out[port & 1];
}

uint8_t gpio_get_dir(Peripheral *p, int port) {
    GPIOPrivExt *ge = (GPIOPrivExt*)p->priv;
    return ge->base.dir[port & 1];
}

WaveChannel* gpio_get_wave(Peripheral *p, int pin) {
    GPIOPrivExt *ge = (GPIOPrivExt*)p->priv;
    if (pin < 0 || pin >= GPIO_NUM_PINS) return NULL;
    return ge->base.wave[pin];
}
