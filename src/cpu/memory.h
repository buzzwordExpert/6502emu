#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_MEM (1024 * 64)  // 64 KB

typedef struct {
    uint8_t DATA[MAX_MEM];
} MEM;

#endif // MEM_H