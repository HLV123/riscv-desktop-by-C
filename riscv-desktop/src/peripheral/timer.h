#ifndef TIMER_H
#define TIMER_H

#include "../peripheral/peripheral.h"
#include "../waveform.h"

#define TIMER_CTRL    0x00
#define TIMER_LOAD    0x04
#define TIMER_CNT     0x08
#define TIMER_STATUS  0x0C

#define TIMER_CTRL_EN   (1<<0)
#define TIMER_CTRL_IE   (1<<1)
#define TIMER_CTRL_MODE (1<<2)  

#define TIMER_BASE_ADDR 0x40001000

typedef struct {
    uint32_t ctrl;
    uint32_t load;
    uint32_t cnt;
    uint32_t status;
    bool     irq_pending;
    WaveChannel *wave_out;
} TimerPriv;

Peripheral*  timer_create(void);
bool         timer_irq_pending(Peripheral *p);
void         timer_clear_irq(Peripheral *p);
uint32_t     timer_get_cnt(Peripheral *p);
uint32_t     timer_get_load(Peripheral *p);
WaveChannel* timer_get_wave(Peripheral *p);

#endif 
