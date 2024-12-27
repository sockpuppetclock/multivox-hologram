#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>

#include "gadget.h"
#include "rammel.h"

#include "gpio.h"

volatile uint32_t *gpio_base;
volatile uint32_t *timer_uS;


void gpio_init_pull(int pin, int pud) {
    // pud: 0:off 1:up 2:down
    _Static_assert(BCM_BASE==BCM2711_PERI_BASE, "2711 specific");

    uint32_t bits = gpio_base[GPPUPPDN0 + (pin>>4)];

    int shift = (pin & 0xf) << 1;
    bits &= ~(3 << shift);
    bits |= (pud << shift);

    gpio_base[GPPUPPDN0 + (pin>>4)] = bits;
}

void gpio_init_in(int pin) {
    gpio_base[pin / 10] &= ~(7ull << ((pin % 10) * 3));
}
void gpio_init_out(int pin) {
    gpio_base[pin / 10] &= ~(7ull << ((pin % 10) * 3)); // apparently needed?
    gpio_base[pin / 10] |=  (1ull << ((pin % 10) * 3));

    gpio_init_pull(pin, 0);
}

static bool gpio_mapmem(void) {
    int memfd;

    if ((memfd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        perror("Can't open /dev/mem (must be root)");
        return NULL;
    }

    gpio_base = (uint32_t*)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, GPIO_BASE);
    
    void* timer_base = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, TIMER_CTRL);
    timer_uS = (uint32_t*)(timer_base ? (uint8_t*)timer_base + 4 : NULL); // just ignore the upper 32 bits
    
    close(memfd);

    if (gpio_base == MAP_FAILED || timer_base == MAP_FAILED) {
        perror("mmap error");
        return false;
    }

    return true;
}

bool gpio_init(void) {
    if (!gpio_mapmem()) {
        return false;
    }

    gpio_init_in(SPIN_SYNC);
    gpio_init_pull(SPIN_SYNC, 0);

    for (int i = 0; i < count_of(matrix_init_out); ++i) {
        gpio_init_out(matrix_init_out[i]);
    }

    gpio_set_pin(RGB_BLANK);
    
    return true;
}



