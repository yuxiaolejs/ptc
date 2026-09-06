#include "cstr.h"
#include "serial.h"
#include "pimath.h"
#include "types.h"

void sprint_uint(char **buf, unsigned int num, int base)
{
    char temp[33];
    int i = 0;

    if (num == 0)
    {
        temp[i++] = '0';
    }
    else
    {
        while (num > 0)
        {
            unsigned int rem = 0, quot = 0;
            udivmod(num, (unsigned int)base, &quot, &rem);
            if (rem < 10)
                temp[i++] = (char)(rem + '0');
            else
                temp[i++] = (char)(rem - 10 + 'a');
            num = quot;
        }
    }

    // Reverse the string into the buffer
    for (int j = i - 1; j >= 0; j--)
    {
        **buf = temp[j];
        (*buf)++;
    }
}

void sprint_int(char **buf, int num, int base)
{
    if (num < 0 && base == 10)
    {
        **buf = '-';
        (*buf)++;
        num = -num;
    }
    sprint_uint(buf, (unsigned int)num, base);
}

void sprint_char(char **buf, char c)
{
    **buf = c;
    (*buf)++;
}

void sprint_str(char **buf, const char *str)
{
    while (*str)
    {
        **buf = *str;
        (*buf)++;
        str++;
    }
}

void vsprintf(char *buf, const char *fmt, va_list args)
{
    while (*fmt)
    {
        if (*fmt == '%')
        {
            fmt++;

            switch (*fmt)
            {
            case 'd':
            {
                int val = va_arg(args, int);
                sprint_int(&buf, val, 10);
                break;
            }
            case 'b':
            {
                int val = va_arg(args, int);
                sprint_int(&buf, val, 2);
                break;
            }
            case 'u':
            {
                unsigned int val = va_arg(args, unsigned int);
                sprint_uint(&buf, val, 10);
                break;
            }
            case 'x':
            {
                unsigned int val = va_arg(args, unsigned int);
                sprint_str(&buf, "0x");
                sprint_uint(&buf, val, 16);
                break;
            }
            case 'c':
            {
                char val = va_arg(args, int);
                sprint_char(&buf, val);
                break;
            }
            case 's':
            {
                const char *val = va_arg(args, const char *);
                sprint_str(&buf, val);
                break;
            }
            case 'p':
            {
                void *val = va_arg(args, void *);
                sprint_str(&buf, "0x");
                sprint_uint(&buf, (unsigned long)val, 16);
                break;
            }
            case '%':
            {
                sprint_char(&buf, '%');
                break;
            }
            default:
                sprint_char(&buf, *fmt);
                break;
            }
        }
        else
        {
            sprint_char(&buf, *fmt);
        }
        fmt++;
    }
    *buf = '\0';
}

void sprintf(char *buf, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
}

void vnprintk(const char *fmt, unsigned int len, va_list ap)
{
    char buffer[len];
    vsprintf(buffer, fmt, ap);
    libc_uart_puts(buffer);
}

void printk(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifndef DISABLE_PRINTK
    vnprintk(fmt, 1024, ap); // hardcoded max length, badddd
#endif
    va_end(ap);
}

void force_printk(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vnprintk(fmt, 1024, ap); // hardcoded max length, badddd
    va_end(ap);
}

int str_eq(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b)
        {
            return 0; // Not equal
        }
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0'); // Both should be at the end
}
int strn_eq(const char *a, const char *b, uint32_t n)
{
    while (*a && *b && n--)
    {
        if (*a != *b)
        {
            return 0; // Not equal
        }
        a++;
        b++;
    }
    return 1;
}

char *strstr(const char *haystack, const char *needle)
{
    // Copied from google
    if (!*needle)
        return (char *)haystack;

    for (; *haystack; haystack++)
    {
        const char *h = haystack;
        const char *n = needle;

        while (*h && *n && (*h == *n))
        {
            h++;
            n++;
        }

        if (!*n)
            return (char *)haystack;
    }

    return 0;
}

uint32_t strlen(const char *s)
{
    uint32_t len = 0;
    while (s && s[len] != '\0')
    {
        len++;
    }
    return len;
}

const char *strrchr(const char *s, int c)
{
    const char *last = 0;
    while (s && *s)
    {
        if (*s == (char)c)
        {
            last = s;
        }
        s++;
    }
    return last;
}

int snprintf(char *buf, uint32_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    // Ensure null-termination
    if (size > 0)
    {
        buf[size - 1] = '\0';
    }

    return strlen(buf); // Return the number of characters written (excluding null terminator)
}
int strncpy(char *dest, const char *src, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }
    for (; i < n; i++)
    {
        dest[i] = '\0';
    }
    return 0; // Return 0 to indicate success
}