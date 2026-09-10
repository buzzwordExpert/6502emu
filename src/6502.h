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

// Instructions addressing mode lookup
typedef enum {
    AM_IMP, AM_ACC, AM_IMM, AM_ZP, AM_ZPX, AM_ZPY, AM_ABS, AM_ABSX, AM_ABSY,
    AM_IND, AM_INDX, AM_INDY, AM_REL, AM_COUNT
} ADDRESSING_MODE;

// instruction lookup
typedef enum {
    I_ORA, I_AND, I_EOR, I_ADC, I_STA, I_LDA, I_CMP, I_SBC,
    I_ASL, I_ROL, I_LSR, I_ROR, I_STX, I_LDX, I_DEC, I_INC,
    I_BIT, I_STY, I_LDY, I_CPX, I_CPY,
    I_BPL, I_BMI, I_BVC, I_BVS, I_BCC, I_BCS, I_BNE, I_BEQ,
    I_BRK, I_JSR, I_RTI, I_RTS, I_JMP,
    I_PHP, I_PLP, I_PHA, I_PLA,
    I_DEY, I_TAY, I_INY, I_INX, I_CLC, I_SEC, I_CLI, I_SEI,
    I_TYA, I_CLV, I_CLD, I_SED, I_TXA, I_TXS, I_TAX, I_TSX, I_DEX, I_NOP
} INSTRUCTION;

// opcode metadata
typedef struct {
    INSTRUCTION instruction;
    ADDRESSING_MODE mode;
    uint8_t cycles;
    bool page_cross_cycle;
    bool implemented;
} CPU_OPCODE;

void cpu_reset(CPU *cpu, const MEM *mem);
void reboot(CPU *cpu, const MEM *mem); 
bool cpu_execute_instruction(CPU *cpu, MEM *mem);
void cpu_irq(CPU *cpu, MEM *mem);
void cpu_nmi(CPU *cpu, MEM *mem);


#endif /* EMU6502_H */
