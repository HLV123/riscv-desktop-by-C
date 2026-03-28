#include "waveform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

WaveChannel* wave_create(const char *name, size_t cap, bool digital) {
    WaveChannel *ch = (WaveChannel*)calloc(1, sizeof(WaveChannel));
    if (!ch) {
        fprintf(stderr, "[WAVE] Fatal: failed to allocate WaveChannel '%s'\n", name ? name : "?");
        return NULL;
    }
    ch->buffer = (WavePoint*)calloc(cap, sizeof(WavePoint));
    if (!ch->buffer) {
        fprintf(stderr, "[WAVE] Fatal: failed to allocate buffer for '%s' (cap=%zu)\n", name ? name : "?", cap);
        free(ch);
        return NULL;
    }
    ch->cap        = cap;
    ch->head       = 0;
    ch->count      = 0;
    ch->is_digital = digital;
    ch->min_val    = digital ? 0.0f : -1.0f;
    ch->max_val    = digital ? 1.0f :  1.0f;
    ch->last_value = 0.0f;
    snprintf(ch->name, sizeof(ch->name), "%s", name);
    return ch;
}

void wave_destroy(WaveChannel *ch) {
    if (!ch) return;
    free(ch->buffer);
    free(ch);
}

void wave_push(WaveChannel *ch, uint64_t cycle, float value) {
    if (ch->count > 0 && ch->last_value == value) return;
    ch->buffer[ch->head].cycle = cycle;
    ch->buffer[ch->head].value = value;
    ch->head = (ch->head + 1) % ch->cap;
    if (ch->count < ch->cap) ch->count++;
    ch->last_value = value;
    if (value < ch->min_val) ch->min_val = value;
    if (value > ch->max_val) ch->max_val = value;
}

void wave_clear(WaveChannel *ch) {
    ch->head  = 0;
    ch->count = 0;
    ch->last_value = 0.0f;
}

WavePoint wave_get(const WaveChannel *ch, size_t idx) {
    if (idx >= ch->count) {
        WavePoint p = {0};
        return p;
    }
    size_t real_idx;
    if (ch->count < ch->cap) {
        real_idx = idx;
    } else {
        real_idx = (ch->head + idx) % ch->cap;
    }
    return ch->buffer[real_idx];
}
