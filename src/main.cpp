#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>


//gpio - general purpose input-output

//==============================================================================

int main() {

    while (true) {


    rcc_periph_clock_enable(RCC_GPIOE);

    gpio_mode_setup(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);


    while (true) {


        gpio_toggle(GPIOE, GPIO15);


        for (volatile uint32_t i = 0; i<2'000'000; ++i);

    }

}
}
