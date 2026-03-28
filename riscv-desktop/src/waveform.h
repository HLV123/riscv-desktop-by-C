#ifndef WAVEFORM_H
#define WAVEFORM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define WAVE_MAX_CHANNELS 32
#define WAVE_DEFAULT_CAP  16384

typedef struct {
    uint64_t cycle;
    float    value;
} WavePoint;

typedef struct {
    char        name[64];
    WavePoint  *buffer;
    size_t      cap;
    size_t      head;
    size_t      count;
    float       min_val;
    float       max_val;
    bool        is_digital;
    float       last_value;
} WaveChannel;

WaveChannel* wave_create(const char *name, size_t cap, bool digital);
void         wave_destroy(WaveChannel *ch);
void         wave_push(WaveChannel *ch, uint64_t cycle, float value);
void         wave_clear(WaveChannel *ch);

WavePoint    wave_get(const WaveChannel *ch, size_t idx);

#endif
