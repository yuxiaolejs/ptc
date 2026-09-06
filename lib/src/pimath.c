// #include "math.h"
#include "types.h"
void udivmod(uint32_t n, uint32_t d,
             uint32_t *quot, uint32_t *rem)
{
    // copied from UCSB CS16 w/ Kevin Burk
    uint32_t q = 0;
    uint32_t r = 0;

    for (int i = 31; i >= 0; i--)
    {
        r <<= 1;
        r |= (n >> i) & 1;

        if (r >= d)
        {
            r -= d;
            q |= (1u << i);
        }
    }

    if (quot)
        *quot = q;
    if (rem)
        *rem = r;
}

uint32_t math_div(uint32_t num, uint32_t den)
{
    uint32_t quot, rem;
    udivmod(num, den, &quot, &rem);
    return quot;
}
uint32_t math_mod(uint32_t num, uint32_t den)
{
    uint32_t quot, rem;
    udivmod(num, den, &quot, &rem);
    return rem;
}
uint32_t __aeabi_uidiv(uint32_t num, uint32_t den)
{
    uint32_t q, r;
    udivmod(num, den, &q, &r);
    return q;
}
uint32_t __aeabi_uidivmod(uint32_t num, uint32_t den)
{
    uint32_t q, r;
    udivmod(num, den, &q, &r);
    __asm__ volatile(
        "mov r1, %0"
        :
        : "r"(r)
        : "r1");
    return q; // r is returned in r1 by ABI
}

uint32_t __divsi3(uint32_t num, uint32_t den)
{
    return math_div(num, den);
}

uint32_t __udivsi3(uint32_t num, uint32_t den)
{
    return math_div(num, den);
}

uint32_t __modsi3(uint32_t num, uint32_t den)
{
    return math_mod(num, den);
}

uint32_t __umodsi3(uint32_t num, uint32_t den)
{
    return math_mod(num, den);
}

uint32_t __divdi3(uint32_t num, uint32_t den)
{
    return math_div(num, den);
}