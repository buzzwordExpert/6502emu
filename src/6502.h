#include <stdint.h>
#include <stdbool.h>

#define MAX_MEM (1024 * 64)  // 64 KB

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

typedef struct {
    uint8_t DATA[MAX_MEM];
} MEM;