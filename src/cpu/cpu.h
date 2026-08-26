#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>
#include "memory.h"

// Status Flag
#define FLAG_C (1 << 0)     // Carry Flag
#define FLAG_Z (1 << 1)     // Zero Flag
#define FLAG_I (1 << 2)     // Interrupt Disable Flag
#define FLAG_D (1 << 3)     // Decimal Flag
#define FLAG_B (1 << 4)     // Break
#define FLAG_U (1 << 5)     // Unused
#define FLAG_V (1 << 6)     // Overflow Flag
#define FLAG_N (1 << 7)     // Negative Flag

typedef struct {
    uint16_t PC;        // Program Counter
    uint8_t  SP;        // Stack Pointer
    uint8_t A, X, Y;    // Accumulator, Index reg X, Index reg Y
    uint8_t CPU_STATUS;
    uint64_t cycles;
} CPU;

void reboot(CPU *cpu, MEM *mem);


#endif // CPU_H