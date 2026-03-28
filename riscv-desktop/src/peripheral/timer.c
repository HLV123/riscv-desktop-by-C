#include "timer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    TimerPriv base;
    uint64_t  last_cycle;
    uint64_t  next_tick;
} TimerPrivExt;

static uint32_t timer_read(Peripheral *p, uint32_t offset, AccessSize sz) {
    TimerPrivExt *te = (TimerPrivExt*)p->priv;
    TimerPriv    *t  = &te->base;
    (void)sz;
    switch (offset) {
        case TIMER_CTRL:   return t->ctrl;
        case TIMER_LOAD:   return t->load;
        case TIMER_CNT:    return t->cnt;
        case TIMER_STATUS: return t->status;
        default: return 0;
    }
}

static void timer_write(Peripheral *p, uint32_t offset, uint32_t val, AccessSize sz) {
    TimerPrivExt *te = (TimerPrivExt*)p->priv;
    TimerPriv    *t  = &te->base;
    (void)sz;
    switch (offset) {
        case TIMER_CTRL:
            t->ctrl = val;
            if (val & TIMER_CTRL_EN) {
                t->cnt = t->load;
            }
            break;
        case TIMER_LOAD:
            t->load = val;
            break;
        case TIMER_CNT: /* read-only */ break;
        case TIMER_STATUS:
            if (val & 1) t->status = 0; 
            break;
        default: break;
    }
}

static void timer_tick(Peripheral *p, uint64_t cycle) {
    TimerPrivExt *te = (TimerPrivExt*)p->priv;
    TimerPriv    *t  = &te->base;
    te->last_cycle = cycle;

    if (!(t->ctrl & TIMER_CTRL_EN)) return;
    if (t->load == 0) return;
    if (cycle % 100 != 0) return;

    if (t->cnt > 0) {
        t->cnt--;
        float duty = (float)t->cnt / (float)(t->load > 0 ? t->load : 1);
        wave_push(t->wave_out, cycle, duty > 0.5f ? 1.0f : 0.0f);
    } else {
        t->status |= 1;  
        wave_push(t->wave_out, cycle, 0.0f);

        if (t->ctrl & TIMER_CTRL_MODE) {
            t->cnt = t->load;
        } else {
            t->ctrl &= ~TIMER_CTRL_EN;
        }
    }
}

static void timer_reset_fn(Peripheral *p) {
    TimerPrivExt *te = (TimerPrivExt*)p->priv;
    TimerPriv    *t  = &te->base;
    t->ctrl   = 0;
    t->load   = 0;
    t->cnt    = 0;
    t->status = 0;
    t->irq_pending = false;
    wave_clear(t->wave_out);
}

static void timer_destroy_fn(Peripheral *p) {
    TimerPrivExt *te = (TimerPrivExt*)p->priv;
    wave_destroy(te->base.wave_out);
    free(te);
    free(p);
}

Peripheral* timer_create(void) {
    Peripheral   *p  = (Peripheral*)calloc(1, sizeof(Peripheral));
    TimerPrivExt *te = (TimerPrivExt*)calloc(1, sizeof(TimerPrivExt));
    if (!p || !te) {
        fprintf(stderr, "[TIMER] Fatal: failed to allocate Timer peripheral\n");
        free(p);
        free(te);
        return NULL;
    }
    TimerPriv    *t  = &te->base;

    t->wave_out = wave_create("TIMER_OUT", WAVE_DEFAULT_CAP, true);

    p->ops.read    = timer_read;
    p->ops.write   = timer_write;
    p->ops.tick    = timer_tick;
    p->ops.reset   = timer_reset_fn;
    p->ops.destroy = timer_destroy_fn;
    p->base_addr   = TIMER_BASE_ADDR;
    p->size        = 0x10;
    p->name        = "TIMER";
    p->priv        = te;

    return p;
}

bool timer_irq_pending(Peripheral *p) {
    TimerPrivExt *te = (TimerPrivExt*)p->priv;
    return te->base.status & 1;
}

void timer_clear_irq(Peripheral *p) {
    TimerPrivExt *te = (TimerPrivExt*)p->priv;
    te->base.status = 0;
}

uint32_t timer_get_cnt(Peripheral *p) {
    TimerPrivExt *te = (TimerPrivExt*)p->priv;
    return te->base.cnt;
}

uint32_t timer_get_load(Peripheral *p) {
    TimerPrivExt *te = (TimerPrivExt*)p->priv;
    return te->base.load;
}

WaveChannel* timer_get_wave(Peripheral *p) {
    TimerPrivExt *te = (TimerPrivExt*)p->priv;
    return te->base.wave_out;
}
