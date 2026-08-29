#ifndef EMU6502_H
#define EMU6502_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_MEM (1024 * 64)  // 64 KB


// -------- MEMORY --------

typedef uint8_t (*bus_device_read_fn)(void *context, uint16_t address);
typedef void (*bus_device_write_fn)(void *context, uint16_t address, uint8_t value);

typedef struct {
    bus_device_read_fn read;
    bus_device_write_fn write;
    void *context;
} BUS_DEVICE;

typedef struct {
    uint8_t DATA[MAX_MEM];
    const BUS_DEVICE *devices[MAX_MEM];
} MEM;


void bus_init(MEM *mem);
uint8_t bus_read(const MEM *mem, uint16_t address);
void bus_write(MEM *mem, uint16_t address, uint8_t value);
void bus_map_device(MEM *mem, uint16_t start, uint16_t end,
                    const BUS_DEVICE *device);

// -------- CPU --------

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

void cpu_reset(CPU *cpu, const MEM *mem);
void reboot(CPU *cpu, const MEM *mem); 
bool cpu_execute_instruction(CPU *cpu, MEM *mem);
void cpu_irq(CPU *cpu, MEM *mem);
void cpu_nmi(CPU *cpu, MEM *mem);


#endif /* EMU6502_H */
