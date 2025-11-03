#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/usart.h>
#include <stdio.h>
#include <math.h>

/*
ПРОТОКОЛ:

на стороне передающего (данные с сенсора):
idle -> collect -> transmit -> idle

на стороне принимающего:
idle -> accept -> transmit -> idle

Передающая сторона(далее СОД - сторона, отдающая данные) получает сигнал из accept
со стороны принимающей(далее СПД - сторона приёма данных).

СОД переходит в состояние collect, где получаются данные с датчиков.

Когда данные получены, СОД переходит в состояние transmit, где формирует посылку и отправляет её
ЮАРТ работает таким образом, что строка на самом деле отправляет побитово, поэтому нужно сделать
опен-флаг (открывающий) и клоз-флаг (закрывающий). Между будет передаваться информация о расстоянии.

После того, как закрывающий флаг будет отправлен, СОД переходит в idle

Вместе с этим СПД переходит в transmit, где по каналу связи (радиоканалу, к примеру) передаёт данные другому человеку
И возвращается в idle, откуда может перейти в accept по команде (по кнопке, к примеру)

Чтобы сделать схему универсальной, схему СПД можно ограничить до accept. Главное - принять и передать сигнал.

[ПРОБЛЕМА, КОТОРУЮ МОЖНО ПРОИГНОРИРОВАТЬ ПОКА ЧТО

В КАКОЙ МОМЕНТ ОТПРАВЛЯТЬ ДАННЫЕ С ДАТЧИКА? ИБО ОН МОЖЕТ ОТОСЛАТЬ ПРОШЛЫЕ И ОТОСЛАТЬ НОВЫЕ]

*/

// ЗЕЛЕНЫЙ TX, БЕЛЫЙ RX

//gpio - general purpose input-output

// Переменные для хранения значений захвата

volatile double range_output = 0;

void usart_setup(void) {
    usart_set_baudrate(USART2, 9600);
    usart_set_databits(USART2, 8);
    usart_set_stopbits(USART2, USART_STOPBITS_1);
    usart_set_mode(USART2, USART_MODE_TX_RX);
    usart_set_parity(USART2, USART_PARITY_NONE);
    usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);
    usart_enable(USART2);

}

void clock_setup(void) {
    rcc_clock_setup_hsi(&rcc_hsi_configs[RCC_CLOCK_HSI_64MHZ]);
   
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_USART2);// включаем USART2
    rcc_periph_clock_enable(RCC_TIM1);
}

void gpio_setup(void) {
    //USART1
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2 | GPIO3);
    gpio_set_af(GPIOA, GPIO_AF7, GPIO2 | GPIO3); //PA2 (TX - зеленый) и PA3(RX - белый)
}


void tim1_setup(void) {
    // Сбрасываем таймер
    rcc_periph_reset_pulse(RST_TIM1);

    timer_set_prescaler(TIM1, 64 - 1); // ВСЁ НОРМАЛЬНО!! ЗДЕСЬ ВРЕМЯ ТИКА СОСТАВЛЯЕТ 1 МКС (64/64'000'000)
    timer_set_period(TIM1, 50000 - 1); //а вот здесь стоит увеличить время прерывания, до 50 мс хотя бы

    // Включаем таймер
    timer_enable_counter(TIM1);
}


void uart_puts(char *string) {
    while (*string) {
        usart_send_blocking(USART2, *string);
        string++;
    }
}

void uart_putln(char *string) {
    uart_puts(string);
    uart_puts("\r\n");
}

//==============================================================================

int main() {
    clock_setup();
    gpio_setup();
    tim1_setup();
    usart_setup();

    rcc_periph_clock_enable(RCC_GPIOE);

    gpio_mode_setup(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15 | GPIO12);

    while (true) {
        uart_putln("LED on");
        gpio_set(GPIOE, GPIO15);
        for (volatile uint32_t i = 0; i<2'000'000; ++i);
        uart_putln("LED off");
        gpio_clear(GPIOE, GPIO15);
        for (volatile uint32_t i = 0; i<2'000'000; ++i);
    }

}

