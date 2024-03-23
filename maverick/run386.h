#pragma once

#include "../common/i386.h"

uint32_t bus_read_8(uint32_t addr);
uint32_t bus_read_16(uint32_t addr);
uint32_t bus_read_32(uint32_t addr);
void bus_write_8(uint32_t addr, uint8_t value);
void bus_write_16(uint32_t addr, uint16_t value);
void bus_write_32(uint32_t addr, uint32_t value);

int i386_step(i386* pCPU);
void dump_386(i386* pCPU);

void i386_push(i386* pCPU, uint32_t value);