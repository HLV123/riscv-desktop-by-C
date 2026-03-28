#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emulator.h"
#include "cpu.h"
#include "peripheral/uart.h"
#include "peripheral/gpio.h"
#include "peripheral/timer.h"

static void escape_json_string(const char *s, char *out, int maxlen) {
    int j = 0;
    for (int i = 0; s[i] && j < maxlen - 2; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"') { out[j++] = '\\'; out[j++] = '"'; }
        else if (c == '\\') { out[j++] = '\\'; out[j++] = '\\'; }
        else if (c == '\n') { out[j++] = '\\'; out[j++] = 'n'; }
        else if (c == '\r') { out[j++] = '\\'; out[j++] = 'r'; }
        else if (c == '\t') { out[j++] = '\\'; out[j++] = 't'; }
        else if (c < 0x20) { j += snprintf(out+j, maxlen-j, "\\u%04x", c); }
        else out[j++] = c;
    }
    out[j] = '\0';
}

static void print_json_state(Emulator *emu) {
    CPU *cpu = emu->cpu;

    char state_str[16];
    switch (emu->state) {
        case EMU_IDLE:    strcpy(state_str, "IDLE");    break;
        case EMU_RUNNING: strcpy(state_str, "RUNNING"); break;
        case EMU_PAUSED:  strcpy(state_str, "PAUSED");  break;
        case EMU_HALTED:  strcpy(state_str, "HALTED");  break;
        case EMU_ERROR:   strcpy(state_str, "ERROR");   break;
        default:          strcpy(state_str, "UNKNOWN");
    }

    printf("{");
    printf("\"state\":\"%s\",", state_str);
    printf("\"pc\":%u,", cpu->pc);
    printf("\"cycle\":%llu,", (unsigned long long)cpu->cycle);
    printf("\"exception\":\"%s\",", cpu_exception_name(cpu->exception));

    printf("\"regs\":[");
    for (int i = 0; i < 32; i++) {
        printf("%u", (uint32_t)cpu->regs[i]);
        if (i < 31) printf(",");
    }
    printf("],");

    GPIOPriv *g = (GPIOPriv*)emu->gpio->priv;
    printf("\"gpio\":{");
    printf("\"porta_dir\":%u,\"porta_out\":%u,\"porta_in\":%u,\"porta_ie\":%u,",
           g->dir[0], g->out[0], g->in[0], g->ie[0]);
    printf("\"portb_dir\":%u,\"portb_out\":%u,\"portb_in\":%u,\"portb_ie\":%u",
           g->dir[1], g->out[1], g->in[1], g->ie[1]);
    printf("},");

    TimerPriv *t = (TimerPriv*)emu->timer->priv;
    printf("\"timer\":{");
    printf("\"ctrl\":%u,\"load\":%u,\"cnt\":%u,\"status\":%u",
           t->ctrl, t->load, t->cnt, t->status);
    printf("},");

    char uart_raw[8192] = {0};
    char uart_esc[16384] = {0};
    int ulen = uart_read_buf(emu->uart, uart_raw, sizeof(uart_raw)-1);
    if (ulen < 0) ulen = 0;
    uart_raw[ulen] = '\0';
    escape_json_string(uart_raw, uart_esc, sizeof(uart_esc));
    printf("\"uart\":\"%s\"", uart_esc);

    printf("}\n");
    fflush(stdout);
}

static void print_disasm_json(Emulator *emu) {
    uint8_t *rom_data = emu->rom->data;
    uint32_t rom_size = emu->rom->size;
    uint32_t rom_base = emu->rom->base;

    printf("[");
    int first = 1;
    for (uint32_t off = 0; off < rom_size - 3; off += 4) {
        uint32_t raw = (uint32_t)rom_data[off]
                     | ((uint32_t)rom_data[off+1] << 8)
                     | ((uint32_t)rom_data[off+2] << 16)
                     | ((uint32_t)rom_data[off+3] << 24);
        if (raw == 0 && off > 0) {
            int allzero = 1;
            for (uint32_t j = off; j < off+16 && j < rom_size; j++) {
                if (rom_data[j]) { allzero = 0; break; }
            }
            if (allzero) break;
        }
        uint32_t addr = rom_base + off;
        char disasm_text[64] = {0};
        DecodedInst inst = cpu_decode(raw);
        cpu_disassemble(&inst, disasm_text, sizeof(disasm_text));
        char esc[128];
        escape_json_string(disasm_text, esc, sizeof(esc));
        if (!first) printf(",");
        printf("{\"addr\":%u,\"raw\":%u,\"text\":\"%s\"}", addr, raw, esc);
        first = 0;
    }
    printf("]\n");
    fflush(stdout);
}

static void print_memory_json(Emulator *emu, const char *region) {
    MemRegion *mem = NULL;
    if (strcmp(region, "rom") == 0) mem = emu->rom;
    else if (strcmp(region, "ram") == 0) mem = emu->ram;
    else { printf("{\"error\":\"unknown region\"}\n"); fflush(stdout); return; }

    uint32_t size = mem->size;
    printf("{\"base\":%u,\"size\":%u,\"data\":[", mem->base, size);
    for (uint32_t i = 0; i < size; i++) {
        printf("%u", mem->data[i]);
        if (i < size - 1) printf(",");
    }
    printf("]}\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <binary.bin> [--json] [max_cycles]\n", argv[0]);
        return 1;
    }

    int json_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) json_mode = 1;
    }

    if (json_mode) {
        Emulator *emu = emu_create();
        if (!emu) { fprintf(stderr, "FATAL: emu_create failed\n"); return 1; }

        char line[4096];
        while (fgets(line, sizeof(line), stdin)) {
            int len = (int)strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';

            if (strncmp(line, "load ", 5) == 0) {
                const char *path = line + 5;
                if (emu_load_file(emu, path)) {
                    printf("{\"ok\":true}\n");
                } else {
                    printf("{\"ok\":false,\"error\":\"load failed\"}\n");
                }
                fflush(stdout);

            } else if (strcmp(line, "state") == 0) {
                print_json_state(emu);

            } else if (strcmp(line, "disasm") == 0) {
                print_disasm_json(emu);

            } else if (strncmp(line, "mem ", 4) == 0) {
                print_memory_json(emu, line + 4);

            } else if (strncmp(line, "run", 3) == 0 && (line[3] == '\0' || line[3] == ' ')) {
                if (emu->state != EMU_HALTED && emu->state != EMU_ERROR) {
                    emu_set_state(emu, EMU_RUNNING);
                    int cycles = (line[3] == ' ' && line[4] != '\0') ? atoi(line + 4) : 100000;
                    if (cycles <= 0) cycles = 100000;
                    emu_run_cycles(emu, cycles);
                }
                print_json_state(emu);

            } else if (strcmp(line, "step") == 0) {
                emu_step(emu);
                print_json_state(emu);

            } else if (strcmp(line, "pause") == 0) {
                if (emu->state == EMU_RUNNING) emu->state = EMU_PAUSED;
                print_json_state(emu);

            } else if (strcmp(line, "reset") == 0) {
                emu_reset(emu);
                printf("{\"ok\":true}\n");
                fflush(stdout);

            } else if (strncmp(line, "bp_add ", 7) == 0) {
                uint32_t addr = (uint32_t)strtoul(line + 7, NULL, 0);
                emu_add_breakpoint(emu, addr);
                printf("{\"ok\":true}\n");
                fflush(stdout);

            } else if (strncmp(line, "bp_del ", 7) == 0) {
                uint32_t addr = (uint32_t)strtoul(line + 7, NULL, 0);
                emu_remove_breakpoint(emu, addr);
                printf("{\"ok\":true}\n");
                fflush(stdout);

            } else if (strncmp(line, "gpio_input ", 11) == 0) {
                int port = 0;
                uint32_t val = 0;
                sscanf(line + 11, "%d %u", &port, &val);
                if (port >= 0 && port < 2) {
                    gpio_inject_input(emu->gpio, port, (uint8_t)val);
                }
                printf("{\"ok\":true}\n");
                fflush(stdout);

            } else if (strcmp(line, "quit") == 0) {
                break;
            } else {
                printf("{\"error\":\"unknown command\"}\n");
                fflush(stdout);
            }
        }

        emu_destroy(emu);
        return 0;
    }

    printf("RISC-V RV32I Emulator - CLI Mode\n");
    printf("==================================\n\n");

    Emulator *emu = emu_create();
    if (!emu) { fprintf(stderr, "ERROR: Failed to initialize emulator\n"); return 1; }

    printf("Loading: %s\n", argv[1]);
    if (!emu_load_file(emu, argv[1])) {
        printf("ERROR: Failed to load binary\n");
        emu_destroy(emu);
        return 1;
    }
    printf("Loaded OK.\n\n");

    uint64_t max_cycles = 1000000;
    for (int i = 2; i < argc; i++) {
        if (argv[i][0] != '-') max_cycles = (uint64_t)atoll(argv[i]);
    }

    printf("Running... (max %llu cycles)\n", (unsigned long long)max_cycles);
    emu_set_state(emu, EMU_RUNNING);
    uint64_t count = 0;
    while (emu->state == EMU_RUNNING && count < max_cycles) {
        int r = cpu_step(emu->cpu, emu->bus);
        bus_tick_all(emu->bus, emu->cpu->cycle);
        count++;
        if (r != 0) emu->state = EMU_HALTED;
    }

    printf("\nDone. Executed %llu cycles.\n", (unsigned long long)count);
    if (emu->cpu->exception != EXC_NONE)
        printf("Exception: %s\n", cpu_exception_name(emu->cpu->exception));
    if (count >= max_cycles)
        printf("(Reached cycle limit)\n");

    char uart_buf[8192];
    int ulen = uart_read_buf(emu->uart, uart_buf, sizeof(uart_buf)-1);
    if (ulen > 0) { uart_buf[ulen] = '\0'; printf("\n=== UART Output ===\n%s\n", uart_buf); }

    printf("\n=== CPU State @ cycle %llu ===\n", (unsigned long long)emu->cpu->cycle);
    printf("PC = 0x%08X\n", emu->cpu->pc);
    for (int i = 0; i < 32; i++) {
        printf("  x%-2d %-4s = 0x%08X%s",
               i, cpu_reg_name(i), emu->cpu->regs[i], (i%2==1)?"\n":"   ");
    }

    emu_destroy(emu);
    return 0;
}
