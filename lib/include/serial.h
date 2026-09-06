#ifndef SERIAL_H
#define SERIAL_H
#include "types.h"
// void libc_serial_puts(char *data);
// void libc_serial_printInt(uint32_t num);
// void libc_serial_wait_till_deadline(uint32_t deadline);
void libc_uart_init();
void libc_uart_putc(char c);
void libc_uart_puts(char *str);
void libc_uart_printInt(uint32_t num);
void libc_uart_getc(char *c);
int libc_uart_getc_timeout(char *c, uint32_t timeout_ms);
void libc_uart_gets(char *buf, int maxlen);
void libc_uart_wait_tx(void);
#endif