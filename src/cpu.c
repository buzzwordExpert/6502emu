#include "6502.h"

typedef struct { uint16_t address; bool page_crossed; } ADDRESS_RESULT;
typedef ADDRESS_RESULT (*address_resolver_fn)(CPU *, const MEM *);

static uint8_t fetch_byte(CPU *cpu, const MEM *mem) {
    return bus_read(mem, cpu->PC++);
}

static uint16_t fetch_word(CPU *cpu, const MEM *mem) {
    uint8_t low = fetch_byte(cpu, mem);
    return low | ((uint16_t)fetch_byte(cpu, mem) << 8);
}

static void set_flag(CPU *cpu, uint8_t flag, bool value) {
    if (value) cpu->CPU_STATUS |= flag;
    else cpu->CPU_STATUS &= (uint8_t)~flag;
}

static void update_zero_negative(CPU *cpu, uint8_t value) {
    set_flag(cpu, FLAG_Z, value == 0);
    set_flag(cpu, FLAG_N, value & 0x80);
}

static void push_byte(CPU *cpu, MEM *mem, uint8_t value) {
    bus_write(mem, 0x0100 | cpu->SP, value);
    cpu->SP--;
}

static uint8_t pull_byte(CPU *cpu, const MEM *mem) {
    cpu->SP++;
    return bus_read(mem, 0x0100 | cpu->SP);
}

static void push_word(CPU *cpu, MEM *mem, uint16_t value) {
    push_byte(cpu, mem, value >> 8);
    push_byte(cpu, mem, value);
}

static uint16_t pull_word(CPU *cpu, const MEM *mem) {
    uint8_t low = pull_byte(cpu, mem);
    return low | ((uint16_t)pull_byte(cpu, mem) << 8);
}

static ADDRESS_RESULT result(uint16_t address, bool crossed) {
    return (ADDRESS_RESULT){ address, crossed };
}

static ADDRESS_RESULT resolve_imp(CPU *cpu, const MEM *mem) {
    (void)cpu; (void)mem; return result(0, false);
}

static ADDRESS_RESULT resolve_imm(CPU *cpu, const MEM *mem) {
    (void)mem; return result(cpu->PC++, false);
}

static ADDRESS_RESULT resolve_zp(CPU *cpu, const MEM *mem) {
    return result(fetch_byte(cpu, mem), false);
}

static ADDRESS_RESULT resolve_zpx(CPU *cpu, const MEM *mem) {
    return result((uint8_t)(fetch_byte(cpu, mem) + cpu->X), false);
}

static ADDRESS_RESULT resolve_zpy(CPU *cpu, const MEM *mem) {
    return result((uint8_t)(fetch_byte(cpu, mem) + cpu->Y), false);
}

static ADDRESS_RESULT resolve_abs(CPU *cpu, const MEM *mem) {
    return result(fetch_word(cpu, mem), false);
}

static ADDRESS_RESULT resolve_absx(CPU *cpu, const MEM *mem) {
    uint16_t base = fetch_word(cpu, mem), address = base + cpu->X;
    return result(address, (base & 0xFF00) != (address & 0xFF00));
}

static ADDRESS_RESULT resolve_absy(CPU *cpu, const MEM *mem) {
    uint16_t base = fetch_word(cpu, mem), address = base + cpu->Y;
    return result(address, (base & 0xFF00) != (address & 0xFF00));
}

static ADDRESS_RESULT resolve_ind(CPU *cpu, const MEM *mem) {
    uint16_t pointer = fetch_word(cpu, mem);
    uint8_t low = bus_read(mem, pointer);
    /* NMOS 6502 JMP ($xxFF) reads its high byte from $xx00. */
    uint8_t high = bus_read(mem, (pointer & 0xFF00) | (uint8_t)(pointer + 1));
    return result(low | ((uint16_t)high << 8), false);
}

static ADDRESS_RESULT resolve_indx(CPU *cpu, const MEM *mem) {
    uint8_t pointer = fetch_byte(cpu, mem) + cpu->X;
    uint8_t low = bus_read(mem, pointer);
    uint8_t high = bus_read(mem, (uint8_t)(pointer + 1));
    return result(low | ((uint16_t)high << 8), false);
}

static ADDRESS_RESULT resolve_indy(CPU *cpu, const MEM *mem) {
    uint8_t pointer = fetch_byte(cpu, mem);
    uint16_t base = bus_read(mem, pointer) |
                    ((uint16_t)bus_read(mem, (uint8_t)(pointer + 1)) << 8);
    uint16_t address = base + cpu->Y;
    return result(address, (base & 0xFF00) != (address & 0xFF00));
}

static ADDRESS_RESULT resolve_rel(CPU *cpu, const MEM *mem) {
    return result(cpu->PC + (int8_t)fetch_byte(cpu, mem), false);
}

static const address_resolver_fn ADDRESS_RESOLVERS[AM_COUNT] = {
    [AM_IMP] = resolve_imp, [AM_ACC] = resolve_imp, [AM_IMM] = resolve_imm,
    [AM_ZP] = resolve_zp, [AM_ZPX] = resolve_zpx, [AM_ZPY] = resolve_zpy,
    [AM_ABS] = resolve_abs, [AM_ABSX] = resolve_absx, [AM_ABSY] = resolve_absy,
    [AM_IND] = resolve_ind, [AM_INDX] = resolve_indx, [AM_INDY] = resolve_indy,
    [AM_REL] = resolve_rel,
};

static uint8_t read_operand(const MEM *mem, ADDRESS_RESULT operand) {
    return bus_read(mem, operand.address);
}

static void write_operand(CPU *cpu, MEM *mem, ADDRESSING_MODE mode,
                          ADDRESS_RESULT operand, uint8_t value) {
    if (mode == AM_ACC) cpu->A = value;
    else bus_write(mem, operand.address, value);
}

static void compare(CPU *cpu, uint8_t left, uint8_t right) {
    set_flag(cpu, FLAG_C, left >= right);
    update_zero_negative(cpu, left - right);
}

static uint8_t branch(CPU *cpu, ADDRESS_RESULT operand, bool condition) {
    if (!condition) return 0;
    bool crossed = (cpu->PC & 0xFF00) != (operand.address & 0xFF00);
    cpu->PC = operand.address;
    return crossed ? 2 : 1;
}

static uint8_t execute(CPU *cpu, MEM *mem, INSTRUCTION instruction,
                       ADDRESSING_MODE mode, ADDRESS_RESULT operand) {
    uint8_t value;
    uint8_t carry;
    uint16_t wide;
    int16_t difference;

    switch (instruction) {
    case I_ORA: cpu->A |= read_operand(mem, operand); update_zero_negative(cpu, cpu->A); break;
    case I_AND: cpu->A &= read_operand(mem, operand); update_zero_negative(cpu, cpu->A); break;
    case I_EOR: cpu->A ^= read_operand(mem, operand); update_zero_negative(cpu, cpu->A); break;
    case I_ADC:
        value = read_operand(mem, operand); carry = (cpu->CPU_STATUS & FLAG_C) != 0;
        wide = cpu->A + value + carry;
        set_flag(cpu, FLAG_V, (~(cpu->A ^ value) & (cpu->A ^ (uint8_t)wide) & 0x80) != 0);
        if (cpu->CPU_STATUS & FLAG_D) {
            if ((cpu->A & 0x0F) + (value & 0x0F) + carry > 9) wide += 0x06;
            if (wide > 0x99) wide += 0x60;
        }
        cpu->A = wide; set_flag(cpu, FLAG_C, wide > 0xFF); update_zero_negative(cpu, cpu->A); break;
    case I_SBC:
        value = read_operand(mem, operand); carry = (cpu->CPU_STATUS & FLAG_C) != 0;
        difference = (int16_t)cpu->A - value - (carry ? 0 : 1);
        set_flag(cpu, FLAG_V, ((cpu->A ^ (uint8_t)difference) & (cpu->A ^ value) & 0x80) != 0);
        if (cpu->CPU_STATUS & FLAG_D) {
            if ((int16_t)(cpu->A & 0x0F) - (carry ? 0 : 1) < (value & 0x0F)) difference -= 0x06;
            if (difference < 0) difference -= 0x60;
        }
        cpu->A = difference; set_flag(cpu, FLAG_C, difference >= 0); update_zero_negative(cpu, cpu->A); break;
    case I_LDA: cpu->A = read_operand(mem, operand); update_zero_negative(cpu, cpu->A); break;
    case I_LDX: cpu->X = read_operand(mem, operand); update_zero_negative(cpu, cpu->X); break;
    case I_LDY: cpu->Y = read_operand(mem, operand); update_zero_negative(cpu, cpu->Y); break;
    case I_STA: bus_write(mem, operand.address, cpu->A); break;
    case I_STX: bus_write(mem, operand.address, cpu->X); break;
    case I_STY: bus_write(mem, operand.address, cpu->Y); break;
    case I_CMP: compare(cpu, cpu->A, read_operand(mem, operand)); break;
    case I_CPX: compare(cpu, cpu->X, read_operand(mem, operand)); break;
    case I_CPY: compare(cpu, cpu->Y, read_operand(mem, operand)); break;
    case I_BIT:
        value = read_operand(mem, operand); set_flag(cpu, FLAG_Z, !(cpu->A & value));
        set_flag(cpu, FLAG_N, value & 0x80); set_flag(cpu, FLAG_V, value & 0x40); break;
    case I_ASL:
        value = mode == AM_ACC ? cpu->A : read_operand(mem, operand);
        set_flag(cpu, FLAG_C, value & 0x80); value <<= 1;
        write_operand(cpu, mem, mode, operand, value); update_zero_negative(cpu, value); break;
    case I_LSR:
        value = mode == AM_ACC ? cpu->A : read_operand(mem, operand);
        set_flag(cpu, FLAG_C, value & 1); value >>= 1;
        write_operand(cpu, mem, mode, operand, value); update_zero_negative(cpu, value); break;
    case I_ROL:
        value = mode == AM_ACC ? cpu->A : read_operand(mem, operand); carry = (cpu->CPU_STATUS & FLAG_C) != 0;
        set_flag(cpu, FLAG_C, value & 0x80); value = (value << 1) | carry;
        write_operand(cpu, mem, mode, operand, value); update_zero_negative(cpu, value); break;
    case I_ROR:
        value = mode == AM_ACC ? cpu->A : read_operand(mem, operand); carry = (cpu->CPU_STATUS & FLAG_C) != 0;
        set_flag(cpu, FLAG_C, value & 1); value = (value >> 1) | (carry << 7);
        write_operand(cpu, mem, mode, operand, value); update_zero_negative(cpu, value); break;
    case I_INC: value = read_operand(mem, operand) + 1; bus_write(mem, operand.address, value); update_zero_negative(cpu, value); break;
    case I_DEC: value = read_operand(mem, operand) - 1; bus_write(mem, operand.address, value); update_zero_negative(cpu, value); break;
    case I_INX: cpu->X++; update_zero_negative(cpu, cpu->X); break;
    case I_INY: cpu->Y++; update_zero_negative(cpu, cpu->Y); break;
    case I_DEX: cpu->X--; update_zero_negative(cpu, cpu->X); break;
    case I_DEY: cpu->Y--; update_zero_negative(cpu, cpu->Y); break;
    case I_TAX: cpu->X = cpu->A; update_zero_negative(cpu, cpu->X); break;
    case I_TAY: cpu->Y = cpu->A; update_zero_negative(cpu, cpu->Y); break;
    case I_TXA: cpu->A = cpu->X; update_zero_negative(cpu, cpu->A); break;
    case I_TYA: cpu->A = cpu->Y; update_zero_negative(cpu, cpu->A); break;
    case I_TSX: cpu->X = cpu->SP; update_zero_negative(cpu, cpu->X); break;
    case I_TXS: cpu->SP = cpu->X; break;
    case I_JMP: cpu->PC = operand.address; break;
    case I_JSR: push_word(cpu, mem, cpu->PC - 1); cpu->PC = operand.address; break;
    case I_RTS: cpu->PC = pull_word(cpu, mem) + 1; break;
    case I_BRK:
        cpu->PC++; push_word(cpu, mem, cpu->PC); push_byte(cpu, mem, cpu->CPU_STATUS | FLAG_B | FLAG_U);
        cpu->CPU_STATUS |= FLAG_I; cpu->PC = bus_read(mem, 0xFFFE) | ((uint16_t)bus_read(mem, 0xFFFF) << 8); break;
    case I_RTI: cpu->CPU_STATUS = (pull_byte(cpu, mem) & (uint8_t)~FLAG_B) | FLAG_U; cpu->PC = pull_word(cpu, mem); break;
    case I_PHA: push_byte(cpu, mem, cpu->A); break;
    case I_PHP: push_byte(cpu, mem, cpu->CPU_STATUS | FLAG_B | FLAG_U); break;
    case I_PLA: cpu->A = pull_byte(cpu, mem); update_zero_negative(cpu, cpu->A); break;
    case I_PLP: cpu->CPU_STATUS = (pull_byte(cpu, mem) & (uint8_t)~FLAG_B) | FLAG_U; break;
    case I_CLC: set_flag(cpu, FLAG_C, false); break;
    case I_SEC: set_flag(cpu, FLAG_C, true); break;
    case I_CLI: set_flag(cpu, FLAG_I, false); break;
    case I_SEI: set_flag(cpu, FLAG_I, true); break;
    case I_CLV: set_flag(cpu, FLAG_V, false); break;
    case I_CLD: set_flag(cpu, FLAG_D, false); break;
    case I_SED: set_flag(cpu, FLAG_D, true); break;
    case I_BPL: return branch(cpu, operand, !(cpu->CPU_STATUS & FLAG_N));
    case I_BMI: return branch(cpu, operand, cpu->CPU_STATUS & FLAG_N);
    case I_BVC: return branch(cpu, operand, !(cpu->CPU_STATUS & FLAG_V));
    case I_BVS: return branch(cpu, operand, cpu->CPU_STATUS & FLAG_V);
    case I_BCC: return branch(cpu, operand, !(cpu->CPU_STATUS & FLAG_C));
    case I_BCS: return branch(cpu, operand, cpu->CPU_STATUS & FLAG_C);
    case I_BNE: return branch(cpu, operand, !(cpu->CPU_STATUS & FLAG_Z));
    case I_BEQ: return branch(cpu, operand, cpu->CPU_STATUS & FLAG_Z);
    case I_NOP: break;
    }
    return 0;
}

/* Official NMOS 6502 opcodes; undocumented opcodes are intentionally absent. */
#define OP(code, inst, addr, base, page) [code] = { inst, addr, base, page, true }
static const CPU_OPCODE OPCODE_TABLE[UINT8_MAX + 1] = {
    OP(0x00,I_BRK,AM_IMP,7,0), OP(0x01,I_ORA,AM_INDX,6,0), OP(0x05,I_ORA,AM_ZP,3,0), OP(0x06,I_ASL,AM_ZP,5,0), OP(0x08,I_PHP,AM_IMP,3,0), OP(0x09,I_ORA,AM_IMM,2,0), OP(0x0A,I_ASL,AM_ACC,2,0), OP(0x0D,I_ORA,AM_ABS,4,0), OP(0x0E,I_ASL,AM_ABS,6,0),
    OP(0x10,I_BPL,AM_REL,2,0), OP(0x11,I_ORA,AM_INDY,5,1), OP(0x15,I_ORA,AM_ZPX,4,0), OP(0x16,I_ASL,AM_ZPX,6,0), OP(0x18,I_CLC,AM_IMP,2,0), OP(0x19,I_ORA,AM_ABSY,4,1), OP(0x1D,I_ORA,AM_ABSX,4,1), OP(0x1E,I_ASL,AM_ABSX,7,0),
    OP(0x20,I_JSR,AM_ABS,6,0), OP(0x21,I_AND,AM_INDX,6,0), OP(0x24,I_BIT,AM_ZP,3,0), OP(0x25,I_AND,AM_ZP,3,0), OP(0x26,I_ROL,AM_ZP,5,0), OP(0x28,I_PLP,AM_IMP,4,0), OP(0x29,I_AND,AM_IMM,2,0), OP(0x2A,I_ROL,AM_ACC,2,0), OP(0x2C,I_BIT,AM_ABS,4,0), OP(0x2D,I_AND,AM_ABS,4,0), OP(0x2E,I_ROL,AM_ABS,6,0),
    OP(0x30,I_BMI,AM_REL,2,0), OP(0x31,I_AND,AM_INDY,5,1), OP(0x35,I_AND,AM_ZPX,4,0), OP(0x36,I_ROL,AM_ZPX,6,0), OP(0x38,I_SEC,AM_IMP,2,0), OP(0x39,I_AND,AM_ABSY,4,1), OP(0x3D,I_AND,AM_ABSX,4,1), OP(0x3E,I_ROL,AM_ABSX,7,0),
    OP(0x40,I_RTI,AM_IMP,6,0), OP(0x41,I_EOR,AM_INDX,6,0), OP(0x45,I_EOR,AM_ZP,3,0), OP(0x46,I_LSR,AM_ZP,5,0), OP(0x48,I_PHA,AM_IMP,3,0), OP(0x49,I_EOR,AM_IMM,2,0), OP(0x4A,I_LSR,AM_ACC,2,0), OP(0x4C,I_JMP,AM_ABS,3,0), OP(0x4D,I_EOR,AM_ABS,4,0), OP(0x4E,I_LSR,AM_ABS,6,0),
    OP(0x50,I_BVC,AM_REL,2,0), OP(0x51,I_EOR,AM_INDY,5,1), OP(0x55,I_EOR,AM_ZPX,4,0), OP(0x56,I_LSR,AM_ZPX,6,0), OP(0x58,I_CLI,AM_IMP,2,0), OP(0x59,I_EOR,AM_ABSY,4,1), OP(0x5D,I_EOR,AM_ABSX,4,1), OP(0x5E,I_LSR,AM_ABSX,7,0),
    OP(0x60,I_RTS,AM_IMP,6,0), OP(0x61,I_ADC,AM_INDX,6,0), OP(0x65,I_ADC,AM_ZP,3,0), OP(0x66,I_ROR,AM_ZP,5,0), OP(0x68,I_PLA,AM_IMP,4,0), OP(0x69,I_ADC,AM_IMM,2,0), OP(0x6A,I_ROR,AM_ACC,2,0), OP(0x6C,I_JMP,AM_IND,5,0), OP(0x6D,I_ADC,AM_ABS,4,0), OP(0x6E,I_ROR,AM_ABS,6,0),
    OP(0x70,I_BVS,AM_REL,2,0), OP(0x71,I_ADC,AM_INDY,5,1), OP(0x75,I_ADC,AM_ZPX,4,0), OP(0x76,I_ROR,AM_ZPX,6,0), OP(0x78,I_SEI,AM_IMP,2,0), OP(0x79,I_ADC,AM_ABSY,4,1), OP(0x7D,I_ADC,AM_ABSX,4,1), OP(0x7E,I_ROR,AM_ABSX,7,0),
    OP(0x81,I_STA,AM_INDX,6,0), OP(0x84,I_STY,AM_ZP,3,0), OP(0x85,I_STA,AM_ZP,3,0), OP(0x86,I_STX,AM_ZP,3,0), OP(0x88,I_DEY,AM_IMP,2,0), OP(0x8A,I_TXA,AM_IMP,2,0), OP(0x8C,I_STY,AM_ABS,4,0), OP(0x8D,I_STA,AM_ABS,4,0), OP(0x8E,I_STX,AM_ABS,4,0),
    OP(0x90,I_BCC,AM_REL,2,0), OP(0x91,I_STA,AM_INDY,6,0), OP(0x94,I_STY,AM_ZPX,4,0), OP(0x95,I_STA,AM_ZPX,4,0), OP(0x96,I_STX,AM_ZPY,4,0), OP(0x98,I_TYA,AM_IMP,2,0), OP(0x99,I_STA,AM_ABSY,5,0), OP(0x9A,I_TXS,AM_IMP,2,0), OP(0x9D,I_STA,AM_ABSX,5,0),
    OP(0xA0,I_LDY,AM_IMM,2,0), OP(0xA1,I_LDA,AM_INDX,6,0), OP(0xA2,I_LDX,AM_IMM,2,0), OP(0xA4,I_LDY,AM_ZP,3,0), OP(0xA5,I_LDA,AM_ZP,3,0), OP(0xA6,I_LDX,AM_ZP,3,0), OP(0xA8,I_TAY,AM_IMP,2,0), OP(0xA9,I_LDA,AM_IMM,2,0), OP(0xAA,I_TAX,AM_IMP,2,0), OP(0xAC,I_LDY,AM_ABS,4,0), OP(0xAD,I_LDA,AM_ABS,4,0), OP(0xAE,I_LDX,AM_ABS,4,0),
    OP(0xB0,I_BCS,AM_REL,2,0), OP(0xB1,I_LDA,AM_INDY,5,1), OP(0xB4,I_LDY,AM_ZPX,4,0), OP(0xB5,I_LDA,AM_ZPX,4,0), OP(0xB6,I_LDX,AM_ZPY,4,0), OP(0xB8,I_CLV,AM_IMP,2,0), OP(0xB9,I_LDA,AM_ABSY,4,1), OP(0xBA,I_TSX,AM_IMP,2,0), OP(0xBC,I_LDY,AM_ABSX,4,1), OP(0xBD,I_LDA,AM_ABSX,4,1), OP(0xBE,I_LDX,AM_ABSY,4,1),
    OP(0xC0,I_CPY,AM_IMM,2,0), OP(0xC1,I_CMP,AM_INDX,6,0), OP(0xC4,I_CPY,AM_ZP,3,0), OP(0xC5,I_CMP,AM_ZP,3,0), OP(0xC6,I_DEC,AM_ZP,5,0), OP(0xC8,I_INY,AM_IMP,2,0), OP(0xC9,I_CMP,AM_IMM,2,0), OP(0xCA,I_DEX,AM_IMP,2,0), OP(0xCC,I_CPY,AM_ABS,4,0), OP(0xCD,I_CMP,AM_ABS,4,0), OP(0xCE,I_DEC,AM_ABS,6,0),
    OP(0xD0,I_BNE,AM_REL,2,0), OP(0xD1,I_CMP,AM_INDY,5,1), OP(0xD5,I_CMP,AM_ZPX,4,0), OP(0xD6,I_DEC,AM_ZPX,6,0), OP(0xD8,I_CLD,AM_IMP,2,0), OP(0xD9,I_CMP,AM_ABSY,4,1), OP(0xDD,I_CMP,AM_ABSX,4,1), OP(0xDE,I_DEC,AM_ABSX,7,0),
    OP(0xE0,I_CPX,AM_IMM,2,0), OP(0xE1,I_SBC,AM_INDX,6,0), OP(0xE4,I_CPX,AM_ZP,3,0), OP(0xE5,I_SBC,AM_ZP,3,0), OP(0xE6,I_INC,AM_ZP,5,0), OP(0xE8,I_INX,AM_IMP,2,0), OP(0xE9,I_SBC,AM_IMM,2,0), OP(0xEA,I_NOP,AM_IMP,2,0), OP(0xEC,I_CPX,AM_ABS,4,0), OP(0xED,I_SBC,AM_ABS,4,0), OP(0xEE,I_INC,AM_ABS,6,0),
    OP(0xF0,I_BEQ,AM_REL,2,0), OP(0xF1,I_SBC,AM_INDY,5,1), OP(0xF5,I_SBC,AM_ZPX,4,0), OP(0xF6,I_INC,AM_ZPX,6,0), OP(0xF8,I_SED,AM_IMP,2,0), OP(0xF9,I_SBC,AM_ABSY,4,1), OP(0xFD,I_SBC,AM_ABSX,4,1), OP(0xFE,I_INC,AM_ABSX,7,0),
};
#undef OP

void cpu_reset(CPU *cpu, const MEM *mem) {
    cpu->PC = bus_read(mem, 0xFFFC) | ((uint16_t)bus_read(mem, 0xFFFD) << 8);
    cpu->SP = 0xFD;
    cpu->X = cpu->Y = cpu->A = 0;
    cpu->CPU_STATUS = FLAG_U | FLAG_I;
    cpu->cycles = 0;
}

void reboot(CPU *cpu, const MEM *mem) { cpu_reset(cpu, mem); }

bool cpu_execute_instruction(CPU *cpu, MEM *mem) {
    const CPU_OPCODE *opcode = &OPCODE_TABLE[fetch_byte(cpu, mem)];
    if (!opcode->implemented) return false;
    ADDRESS_RESULT operand = ADDRESS_RESOLVERS[opcode->mode](cpu, mem);
    uint8_t extra = execute(cpu, mem, opcode->instruction, opcode->mode, operand);
    cpu->cycles += opcode->cycles + extra + (opcode->page_cross_cycle && operand.page_crossed);
    return true;
}

void cpu_irq(CPU *cpu, MEM *mem) {
    if (!(cpu->CPU_STATUS & FLAG_I)) {
        push_word(cpu, mem, cpu->PC);
        push_byte(cpu, mem, cpu->CPU_STATUS & (uint8_t)~FLAG_B);
        cpu->CPU_STATUS |= FLAG_I;
        cpu->PC = bus_read(mem, 0xFFFE) | ((uint16_t)bus_read(mem, 0xFFFF) << 8);
        cpu->cycles += 7;
    }
}

void cpu_nmi(CPU *cpu, MEM *mem) {
    push_word(cpu, mem, cpu->PC);
    push_byte(cpu, mem, cpu->CPU_STATUS & (uint8_t)~FLAG_B);
    cpu->CPU_STATUS |= FLAG_I;
    cpu->PC = bus_read(mem, 0xFFFA) | ((uint16_t)bus_read(mem, 0xFFFB) << 8);
    cpu->cycles += 7;
}
