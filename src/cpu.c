#include <cpu.h>

void reboot(CPU *cpu, MEM * mem) {
    uint16_t low = mem->DATA[0xFFFC];
    uint16_t high = mem->DATA[0xFFFD];
    
    cpu->PC = (high << 8) | low;
    cpu->SP = 0xFD;  
    cpu->X = cpu->Y = cpu->A = 0;
    cpu->CPU_STATUS = FLAG_U | FLAG_I; // bit 5 always 1, I flag set by reboot
}