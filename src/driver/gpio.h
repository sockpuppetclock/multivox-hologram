#ifndef _GPIO_H_
#define _GPIO_H_

#include <stdint.h>

//Pi 2, 3, 4
#define BCM2708_PERI_BASE        0x20000000
#define BCM2709_PERI_BASE        0x3F000000
#define BCM2711_PERI_BASE        0xFE000000

#define BCM_BASE BCM2711_PERI_BASE

#define GPIO_BASE (BCM_BASE + 0x200000)
#define TIMER_CTRL (BCM_BASE + 0x3000)

#define GPFSEL0      0
#define GPFSEL1      1
#define GPFSEL2      2
#define GPFSEL3      3
#define GPFSEL4      4
#define GPFSEL5      5
#define GPSET0       7
#define GPSET1       8
#define GPCLR0       10
#define GPCLR1       11
#define GPLEV0       13
#define GPLEV1       14

#if (BCM_BASE) == (BCM2711_PERI_BASE)
#define GPPUPPDN0 57
#define GPPUPPDN1 58
#define GPPUPPDN2 59
#define GPPUPPDN3 60
#else
#define GPPUD        37
#define GPPUDCLK0    38
#define GPPUDCLK1    39
#endif


extern volatile uint32_t *gpio_base;
extern volatile uint32_t *timer_uS;

static inline void gpio_busy_wait(uint32_t uS) {
    uint32_t start = *timer_uS;
    while (*timer_uS - start <= uS);
}

static inline void gpio_set_bits(uint32_t bits) {
    gpio_base[GPSET0] = bits;
}
static inline void gpio_clear_bits(uint32_t bits) {
    gpio_base[GPCLR0] = bits;
}
static inline void gpio_set_pin(int pin) {
    gpio_set_bits(1ul << pin);
}
static inline void gpio_clear_pin(int pin) {
    gpio_clear_bits(1ul << pin);
}

static inline uint32_t gpio_get_bits(uint32_t bits) {
    return gpio_base[GPLEV0] & bits;
}
static inline int gpio_get_pin(int pin) {
    return gpio_get_bits(1ul << pin) != 0;
}

void gpio_init_pull(int pin, int pud);
void gpio_init_in(int pin);
void gpio_init_out(int pin);

bool gpio_init(void);

#endif

