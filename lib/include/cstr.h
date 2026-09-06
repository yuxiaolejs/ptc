#ifndef CSTR_H
#define CSTR_H

#include <stdarg.h>
#include "types.h"
void sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, uint32_t size, const char *fmt, ...);
int strncpy(char *dest, const char *src, uint32_t n);
void printk(const char *fmt, ...);
void force_printk(const char *fmt, ...);
int str_eq(const char *a, const char *b);
int strn_eq(const char *a, const char *b, uint32_t n);
char *strstr(const char *haystack, const char *needle);
uint32_t strlen(const char *s);
void vsprintf(char *buf, const char *fmt, va_list args);
const char *strrchr(const char *s, int c);
#endif