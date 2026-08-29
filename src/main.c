#include "6502.h"

#include <inttypes.h>
#include <stdio.h>

int main(void) {
    MEM mem;
    CPU cpu = {0};

    bus_init(&mem);

    // Reset will start execution at $8000
    bus_write(&mem, 0xFFFC, 0x00);
    bus_write(&mem, 0xFFFD, 0x80);

    // Program: LDA #$42; LDA $10 
    bus_write(&mem, 0x8000, 0xA9);
    bus_write(&mem, 0x8001, 0x42);
    bus_write(&mem, 0x8002, 0xA5);
    bus_write(&mem, 0x8003, 0x10);
    bus_write(&mem, 0x0010, 0x99);

    cpu_reset(&cpu, &mem);

    if (!cpu_execute_instruction(&cpu, &mem) || cpu.A != 0x42) {
        fprintf(stderr, "LDA immediate test failed\n");
        return 1;
    }

    if (!cpu_execute_instruction(&cpu, &mem) || cpu.A != 0x99) {
        fprintf(stderr, "LDA zero-page test failed\n");
        return 1;
    }

    printf("Tests passed: A=$%02X, PC=$%04X, cycles=%" PRIu64 "\n",
           cpu.A, cpu.PC, cpu.cycles);
    return 0;
}
