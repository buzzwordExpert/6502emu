#include "6502.h"

void cpu_reset(CPU *cpu, const MEM *mem) {
    uint16_t low = bus_read(mem, 0xFFFC);
    uint16_t high = bus_read(mem, 0xFFFD);

    cpu->PC = (high << 8) | low;
    cpu->SP = 0xFD;
    cpu->X = cpu->Y = cpu->A = 0;
    cpu->CPU_STATUS = FLAG_U | FLAG_I;
    cpu->cycles = 0;
}

void reboot(CPU *cpu, const MEM *mem) {
    cpu_reset(cpu, mem);
}

static uint8_t fetch_byte(CPU *cpu, const MEM *mem) {
    return bus_read(mem, cpu->PC++);
}

static uint16_t fetch_word(CPU *cpu, const MEM *mem) {
    uint8_t low = fetch_byte(cpu, mem);
    uint8_t high = fetch_byte(cpu, mem);

    return ((uint16_t) high << 8) | low;
}

static void update_zero_negative(CPU *cpu, uint8_t value) {
    if (value == 0) {
        cpu->CPU_STATUS |= FLAG_Z;
    }
    else {
        cpu->CPU_STATUS &= ~FLAG_Z;
    }

    if (value & 0x80) {
        cpu->CPU_STATUS |= FLAG_N;
    }
    else {
        cpu->CPU_STATUS &= ~FLAG_N;
    }
}

bool cpu_execute_instruction(CPU *cpu, MEM *mem) {
    uint8_t opcode = fetch_byte(cpu, mem);

    switch(opcode) {
        case (0xA9): { // LDA Immediate
            cpu->A = fetch_byte(cpu, mem);
            update_zero_negative(cpu, cpu->A);
            cpu->cycles += 2;
            return true;
        }
        case (0xA5): {  // LDA Zero Page
            uint16_t address = fetch_byte(cpu, mem);
            cpu->A = bus_read(mem, address);
            update_zero_negative(cpu, cpu->A);
            cpu->cycles += 3;
            return true;
        }
    
        default:
            return false;   // Instruction not implemented 
    }
}

void cpu_irq(CPU *cpu, MEM *mem) {

    if (!(cpu->CPU_STATUS & FLAG_I)) {
        bus_write(mem, 0x0100 | cpu->SP--, cpu->PC >> 8);
        bus_write(mem, 0x0100 | cpu->SP--, cpu->PC);
        bus_write(mem, 0x0100 | cpu->SP--, cpu->CPU_STATUS & ~FLAG_B);
        cpu->CPU_STATUS |= FLAG_I;
        cpu->PC = bus_read(mem, 0xFFFE) | ((uint16_t) bus_read(mem, 0xFFFF) << 8);
        cpu->cycles += 7;
    }
}

void cpu_nmi(CPU *cpu, MEM *mem) {
    bus_write(mem, 0x0100 | cpu->SP--, cpu->PC >> 8);
    bus_write(mem, 0x0100 | cpu->SP--, cpu->PC);
    bus_write(mem, 0x0100 | cpu->SP--, cpu->CPU_STATUS & ~FLAG_B);
    cpu->CPU_STATUS |= FLAG_I;
    cpu->PC = bus_read(mem, 0xFFFA) | ((uint16_t)bus_read(mem, 0xFFFB) << 8);
    cpu->cycles += 7;
}
