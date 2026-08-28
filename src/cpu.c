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

bool cpu_execite_instruction(CPU *cpu, MEM *mem) {
    (void)cpu;
    (void)mem;

    return false;
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
