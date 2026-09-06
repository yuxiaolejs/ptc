#include "regs.h"

void PUT32(uint32_t addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}
void put32(uint32_t *addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}
uint32_t GET32(uint32_t addr)
{
    return *(volatile uint32_t *)addr;
}
uint32_t get32(uint32_t *addr)
{
    return *(volatile uint32_t *)addr;
}