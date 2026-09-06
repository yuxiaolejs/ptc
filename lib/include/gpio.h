#ifndef GPIO_H
#define GPIO_H
#include "types.h"
void gpio_set_output(uint32_t pin);
void gpio_set_input(uint32_t pin);
void gpio_set_on(uint32_t pin);
void gpio_set_off(uint32_t pin);
uint32_t gpio_read(uint32_t pin);
void gpio_pin_set_func(uint32_t pin, uint32_t func);
void gpio_set_pullup(unsigned pin);
void gpio_set_pulldown(unsigned pin);
#endif