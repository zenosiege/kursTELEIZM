#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/usart.h>
#include <stdio.h>

// ЗЕЛЕНЫЙ TX, БЕЛЫЙ RX

//gpio - general purpose input-output

// макросы для удобства
#define UART_PORT USART1 //какой порт юарта
#define UART_BAUDRATE 115200 //какой бодрейт
#define GPIO_PORT GPIOA //какой порт
#define GPIO_PIN GPIO8 //какой пин под PWM/Input Capture

// Переменные для хранения значений захвата
volatile uint32_t capture_rising = 0;
volatile uint32_t capture_falling = 0;
volatile uint32_t pulse_width = 0;
volatile uint8_t capture_done = 0;

void clock_setup(void) {
    // Включаем тактирование
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_TIM1);
    rcc_periph_clock_enable(RCC_USART1);
}

void gpio_setup(void) {
    //PA8 в TIM1_CH1(PWM)
    gpio_mode_setup(GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN);
    gpio_set_af(GPIO_PORT, GPIO_AF6, GPIO_PIN);

    //Настраиваем PA9 (TX) и PA10 (RX) для USART1
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9 | GPIO10);
    gpio_set_af(GPIOA, GPIO_AF7, GPIO9 | GPIO10);
}

void usart_setup(void) {
    usart_set_baudrate(UART_PORT, UART_BAUDRATE);
    usart_set_databits(UART_PORT, 8);
    usart_set_stopbits(UART_PORT, USART_STOPBITS_1);
    usart_set_mode(UART_PORT, USART_MODE_TX_RX); //Включаем TX и RX
    usart_set_parity(UART_PORT, USART_PARITY_NONE);
    usart_set_flow_control(UART_PORT, USART_FLOWCONTROL_NONE);
    usart_enable(UART_PORT);
}

void tim1_setup(void) {
    // Сбрасываем таймер
    rcc_periph_reset_pulse(RST_TIM1);

    // Настраиваем таймер на частоту, например, 1 МГц (1 мкс на тик)
    // Используем системную частоту (по умолчанию)
    timer_set_prescaler(TIM1, (rcc_apb2_frequency / 1000000) - 1); // 1 МГц
    timer_set_period(TIM1, 0xFFFF); // Максимальный период

    timer_ic_set_input(TIM1, TIM_IC1, TIM_IC_IN_TI1); // Прямой вход c TI1 (PA8)
    timer_ic_set_filter(TIM1, TIM_IC1, TIM_IC_CK_INT_N_2); // Фильтр для подавления шумов
    timer_ic_set_prescaler(TIM1, TIM_IC1, TIM_IC_PSC_OFF); // Без предделителя
    timer_ic_set_polarity(TIM1, TIM_IC1, TIM_IC_RISING); // Захват на восходящем фронте

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
            capture_done = 1;
            timer_ic_set_polarity(TIM1, TIM_IC1, TIM_IC_RISING); // Возвращаем на восходящий
            edge = 0;
        }
        timer_clear_flag(TIM1, TIM_SR_CC1IF); // Сбрасываем флаг
    }
}

void usart_send_string(const char *str) {
    while (*str) {
        usart_send_blocking(UART_PORT, *str++);
    }
}
//==============================================================================

int main() {
    clock_setup();
    gpio_setup();
    usart_setup();
    tim1_setup();

    char buffer[64];

    rcc_periph_clock_enable(RCC_GPIOE);

    gpio_mode_setup(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);

    
    while (true) {

        if (capture_done) {
            snprintf(buffer, sizeof(buffer), "Pulse width: %lu us\r\n", pulse_width);
            usart_send_string(buffer);
            capture_done = 0; // Сбрасываем флаг
        }

        gpio_toggle(GPIOE, GPIO15);

        snprintf(buffer, sizeof(buffer), "Cycle Skip");
        usart_send_string(buffer);

        for (volatile uint32_t i = 0; i<2'000'000; ++i);

    }

}

