#ifndef GPIO_H
#define GPIO_H

#include "../peripheral/peripheral.h"
#include "../waveform.h"

#define GPIO_PORTA_DIR  0x00
#define GPIO_PORTA_OUT  0x04
#define GPIO_PORTA_IN   0x08
#define GPIO_PORTA_IE   0x0C
#define GPIO_PORTB_DIR  0x10
#define GPIO_PORTB_OUT  0x14
#define GPIO_PORTB_IN   0x18
#define GPIO_PORTB_IE   0x1C

#define GPIO_NUM_PINS   16
#define GPIO_BASE_ADDR  0x40000000

typedef struct {
    uint8_t dir[2];   
    uint8_t out[2];
    uint8_t in[2];
    uint8_t ie[2];
    WaveChannel *wave[GPIO_NUM_PINS];
} GPIOPriv;

Peripheral* gpio_create(void);
void        gpio_inject_input(Peripheral *p, int port, uint8_t val);
uint8_t     gpio_get_output(Peripheral *p, int port);
uint8_t     gpio_get_dir(Peripheral *p, int port);
WaveChannel* gpio_get_wave(Peripheral *p, int pin);

#endif 
