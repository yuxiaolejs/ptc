#include "serial.h"
#include "regs.h"
#include "pictl.h"
// This one should use mini uart instead of the bit-banging method.

inline volatile int ok_to_read()
{
    return (GET32(ADD_AUX_MU_STAT_REG) & 1u);
}

inline volatile int ok_to_write()
{
    return (GET32(ADD_AUX_MU_STAT_REG) & 2u);
}

void libc_uart_init()
{
    PUT32(ADD_AUX_ENABLES, 1);       // p9: enable mini uart
    PUT32(ADD_AUX_MU_CNTL_REG, 0);   // set 00 first thing
    PUT32(ADD_AUX_MU_BAUD_REG, 27); // 1152000 baud rate
    PUT32(ADD_AUX_MU_LCR_REG, 3);    // 8 bits
    PUT32(ADD_AUX_MU_IIR_REG, 6);    // clear fifo

    PUT32(ADD_AUX_MU_CNTL_REG, 3); // enable tx and tx
}
void libc_uart_putc(char c)
{
    while (!ok_to_write())
        ;
    PUT32(ADD_AUX_MU_IO_REG, (uint32_t)(c & 0xFF));
}
void libc_uart_puts(char *str)
{
    while (*str)
    {
        libc_uart_putc(*str);
        str++;
    }
}
void libc_uart_printInt(uint32_t num)
{
    if (num == 0)
    {
        libc_uart_putc('0');
        return;
    }

    char buf[11];
    int i = 0;

    while (num > 0)
    {
        buf[i++] = (char)((num % 10) + '0');
        num /= 10;
    }

    // Reverse the string
    for (int j = i - 1; j >= 0; j--)
    {
        libc_uart_putc(buf[j]);
    }
}
void libc_uart_getc(char *c)
{
    while (!ok_to_read())
        ;
    *c = (char)(GET32(ADD_AUX_MU_IO_REG) & 0xFF);
}
int libc_uart_getc_timeout(char *c, uint32_t timeout_ms)
{
    uint32_t target_time = timer_get_msec() + timeout_ms;
    while (!ok_to_read())
        if (timer_get_msec() >= target_time)
            return 0;
    *c = (char)(GET32(ADD_AUX_MU_IO_REG) & 0xFF);
    return 1; // Indicate success
}
void libc_uart_gets(char *buf, int maxlen)
{
}

void libc_uart_wait_tx(void)
{
    while (!(*PTR_AUX_MU_LSR_REG & (1 << 6)))
        ;
}