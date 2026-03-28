#ifndef UART_H
#define UART_H

#include "../peripheral/peripheral.h"
#include "../waveform.h"
#include <stddef.h>

#define UART_DR   0x00
#define UART_SR   0x04
#define UART_CR   0x08

#define UART_SR_TX_READY (1<<0)
#define UART_SR_RX_VALID (1<<1)
#define UART_CR_EN       (1<<0)

#define UART_BASE_ADDR   0x40002000
#define UART_BUF_SIZE    8192  

typedef struct {
    uint8_t  cr;
    char     buf[UART_BUF_SIZE];
    int      head;   
    int      tail;  
    int      count;  
    WaveChannel *wave_tx;
} UARTPriv;

Peripheral*  uart_create(void);

int          uart_read_buf(Peripheral *p, char *out, int max_len);
const char*  uart_get_buf(Peripheral *p, int *len);
void         uart_clear_buf(Peripheral *p);
WaveChannel* uart_get_wave(Peripheral *p);

#endif 
