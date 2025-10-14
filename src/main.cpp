#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/usart.h>
#include <stdio.h>
#include <math.h>

// ЗЕЛЕНЫЙ TX, БЕЛЫЙ RX

//gpio - general purpose input-output

// Переменные для хранения значений захвата
volatile uint32_t capture_rising = 0;
volatile uint32_t capture_falling = 0;
volatile uint32_t pulse_width = 0;
volatile uint8_t capture_done = 0;

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
    //PA8 в TIM1_CH1(PWM)
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8);
    gpio_set_af(GPIOA, GPIO_AF6, GPIO8);
    
    //USART1
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2 | GPIO3);
    gpio_set_af(GPIOA, GPIO_AF7, GPIO2 | GPIO3); //PA2 (TX - зеленый) и PA3(RX - белый)
}


void tim1_setup(void) {
    // Сбрасываем таймер
    rcc_periph_reset_pulse(RST_TIM1);

    // Настраиваем таймер на частоту, например, 1 МГц (1 мкс на тик)
    // Используем системную частоту (по умолчанию)
    timer_set_prescaler(TIM1, (64000000 / 1000000) - 1); // 1 МГц
    timer_set_period(TIM1, 0xFFFF); // Максимальный период

    timer_ic_set_input(TIM1, TIM_IC1, TIM_IC_IN_TI1); // Прямой вход c TI1 (PA8)
    timer_ic_set_filter(TIM1, TIM_IC1, TIM_IC_CK_INT_N_2); // Фильтр для подавления шумов
    timer_ic_set_prescaler(TIM1, TIM_IC1, TIM_IC_PSC_OFF); // Без предделителя
    timer_ic_set_polarity(TIM1, TIM_IC1, TIM_IC_RISING); // Захват на восходящем фронте

    // Включаем канал захвата
    timer_ic_enable(TIM1, TIM_IC1);

    // Включаем прерывание для захвата
    timer_enable_irq(TIM1, TIM_DIER_CC1IE);
    nvic_enable_irq(NVIC_TIM1_CC_IRQ);

    // Включаем таймер
    timer_enable_counter(TIM1);
}

// Обработчик прерывания для захвата
void tim1_cc_isr(void) {
    static uint8_t edge = 0; // 0 - ждём восходящий, 1 - ждём нисходящий 

    if (timer_get_flag(TIM1, TIM_SR_CC1IF)) {
        timer_clear_flag(TIM1, TIM_SR_CC1IF); // Сбрасываем флаг в начале во избежание залипания флага

        if (edge == 0) {
            // Восходящий фронт
            capture_rising = TIM_CCR1(TIM1);
            timer_ic_set_polarity(TIM1, TIM_IC1, TIM_IC_FALLING); // Меняем на нисходящий
            edge = 1;
        }
        else {
            // Нисходящий фронт
            capture_falling = TIM_CCR1(TIM1);
            pulse_width = capture_falling - capture_rising; // Длительность импульса
            range_output = pulse_width / 58;
            capture_done = 1;
            timer_ic_set_polarity(TIM1, TIM_IC1, TIM_IC_RISING); // Возвращаем на восходящий
            edge = 0;
        }
        
    }
}

void uart_puts(char *string) {
    while (*string) {
        usart_send_blocking(USART1, *string);
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

        if (capture_done) {
            gpio_toggle(GPIOE, GPIO12);
            for (volatile uint32_t i = 0; i<2'000'000; ++i);
            gpio_toggle(GPIOE, GPIO12);
        }

        uart_putln("LED on");
        gpio_set(GPIOE, GPIO15);
        for (volatile uint32_t i = 0; i<2'000'000; ++i);
        uart_putln("LED off");
        gpio_clear(GPIOE, GPIO15);
        for (volatile uint32_t i = 0; i<2'000'000; ++i);
    }

}

