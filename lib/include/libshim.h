#ifndef LIBSHIM_H
#define LIBSHIM_H

#if PLATFORM == 1

#include "mem.h"
#include "cstr.h"

#define printf printk
#define malloc kalloc

#elif PLATFORM == 2

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#else
#error "Unknown platform"
#endif

#endif