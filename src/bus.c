#include "6502.h"

#include <string.h>

void bus_init(MEM *mem) {
    memset(mem, 0, sizeof(*mem));
}

uint8_t bus_read(const MEM *mem, uint16_t address) {
    const BUS_DEVICE *device = mem->devices[address];

    if (device != NULL) {
        return device->read != NULL ? device->read(device->context, address) : 0xFF;
    }
    return mem->DATA[address];
}

void bus_write(MEM *mem, uint16_t address, uint8_t value) {
    const BUS_DEVICE *device = mem->devices[address];

    if (device != NULL) {
        if (device->write != NULL) {
            device->write(device->context, address, value);
        }
        return;
    }

    mem->DATA[address] = value;
}

void bus_map_device(MEM *mem, uint16_t start, uint16_t end,
                    const BUS_DEVICE *device) {
    for (uint32_t address = start; address <= end; ++address) {
        mem->devices[address] = device;
    }
}
