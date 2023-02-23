#include "memory.h"

uint8_t memory[0x10000]{};

uint8_t read_byte(uint16_t address)
{
	return memory[address];
}

uint16_t read_word(uint16_t address)
{
	return read_byte(address) | (read_byte(address + 1) << 8);
}

void write_byte(uint16_t address, uint8_t value)
{
	memory[address] = value;
}

void write_word(uint16_t address, uint16_t value)
{
	write_byte(address, (value & 0x00FF));
	write_byte(address + 1, (value & 0xFF00) >> 8);
}