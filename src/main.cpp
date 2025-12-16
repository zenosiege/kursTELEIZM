#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/usart.h>
#include <stdio.h>
#include <math.h>
#include <string>


#define PERIOD 50000
#define MAX_RANGE_MM 6500

// Переменные для хранения значений захвата

volatile uint32_t capture_rising = 0;
volatile uint32_t capture_falling = 0;
volatile uint32_t pulse_width = 0;
volatile uint8_t capture_done = 0;

// Переменная для вывода расстояния
volatile int range_output = 0;
volatile int last_valid_range = 0;

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
    
    //USART2
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2 | GPIO3);
    gpio_set_af(GPIOA, GPIO_AF7, GPIO2 | GPIO3); //PA2 (TX) и PA3(RX)
}


void tim1_setup(void) {
    // Сбрасываем таймер
    rcc_periph_reset_pulse(RST_TIM1);

    timer_set_prescaler(TIM1, 64 - 1); // ЗДЕСЬ ВРЕМЯ ТИКА СОСТАВЛЯЕТ 1 МКС (64/64'000'000)
    timer_set_period(TIM1, PERIOD - 1); // 50мс, потому что время полного цикла записи данных составляет 49мс

    timer_ic_set_input(TIM1, TIM_IC1, TIM_IC_IN_TI1); // Прямой вход c TIM1 (PA8)
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
        

        if (edge == 0) {
            // Восходящий фронт
            capture_rising = TIM_CCR1(TIM1);
            timer_ic_set_polarity(TIM1, TIM_IC1, TIM_IC_FALLING); // Меняем на нисходящий
            edge = 1;
        }
        else {
            // Нисходящий фронт
            capture_falling = TIM_CCR1(TIM1);

            if (capture_falling > capture_rising) {
                pulse_width = capture_falling - capture_rising; // Длительность импульса (в микросекундах)
            }
            else {
                // если было переполнение между фронтами
                pulse_width = (PERIOD - capture_falling + capture_rising + 1);
            }
            
            range_output = ceil((pulse_width / 147) * 25.4); //147uS per inch (1 inch = 25.4мм = 2.54cm = 0.0254m)
            capture_done = 1;
            timer_ic_set_polarity(TIM1, TIM_IC1, TIM_IC_RISING); // Возвращаем на восходящий
            edge = 0;
        }

        timer_clear_flag(TIM1, TIM_SR_CC1IF); // Сбрасываем флаг 
        
    }
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

void send_double(double value)
{
    char buf[32];
    int int_part = (int)value;
    int frac_part = (int)fabs((value - int_part) * 1000); // 3 знака после запятой

    if (value < 0 && int_part == 0)
        snprintf(buf, sizeof(buf), "-%d.%03d", int_part, frac_part);
    else
        snprintf(buf, sizeof(buf), "%d.%03d", int_part, frac_part);

    uart_putln(buf);
}

void uart_send_int(int value) {
    char buf[32];                // достаточно для int32
    snprintf(buf, sizeof(buf), "00%d", value); //два нуля в начале для устранения пропадающих чисел

    usart_send_blocking(USART2, '<');
    uart_puts(buf);
    usart_send_blocking(USART2, '>');
}

//==============================================================================
volatile int data;

int main() {
    clock_setup();
    gpio_setup();
    tim1_setup();
    usart_setup();

    rcc_periph_clock_enable(RCC_GPIOE);

    gpio_mode_setup(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15 | GPIO12 | GPIO10);

    // Добавляем задержку после включения
    for (volatile uint32_t i = 0; i < 250000; i++); 

    char status = 'i';

        
    bool cFlag = false; // collect flag
    
    while (true) {
        
        char getCommand = usart_recv(USART2);

        gpio_set(GPIOE, GPIO12);
        switch(status)
        {
            case 'i':
                if (getCommand == 'c') {
                    status = 'c';
                    usart_send_blocking(USART2, '>');
                }
                break;
            case 'c':
                gpio_set(GPIOE, GPIO15);
                for (volatile uint32_t i = 0; i<2'000'000; ++i);
                gpio_clear(GPIOE, GPIO15);
                for (volatile uint32_t i = 0; i<2'000'000; ++i);

                cFlag = true;

                while (cFlag) {
                    if (capture_done) {
                        gpio_set(GPIOE, GPIO10);

                        // Игнорируем или используем предыдущее корректное значение
                        if (range_output > MAX_RANGE_MM) {
                            range_output = last_valid_range;
                        } else {
                            last_valid_range = range_output;
                        }

                        cFlag = false;   
                        data = range_output;
                        for (volatile uint32_t i = 0; i<1'000'000; ++i);
                        gpio_clear(GPIOE, GPIO10);
                    }   
                }
                status = 't';
                break;
            case 't':
                // < передаётся сам. > закрывает это дело
                uart_send_int(data);
                //send_float(6.39f);
                status = 'i';

                break;

            default:
                break;
        }

        for (volatile uint32_t i = 0; i<500'000; ++i);
        gpio_clear(GPIOE, GPIO12);
        for (volatile uint32_t i = 0; i<500'000; ++i);

    
        // if (capture_done) {
        //     gpio_set(GPIOE, GPIO12);

        //     // Игнорируем или используем предыдущее корректное значение
        //     if (range_output > MAX_RANGE_M) {
        //         range_output = last_valid_range;
        //     } else {
        //         last_valid_range = range_output;
        //     }

        //     uart_send_int(range_output);
        //     for (volatile uint32_t i = 0; i<1'000'000; ++i);
        //     gpio_clear(GPIOE, GPIO12);
        // }


        // gpio_set(GPIOE, GPIO15);
        // for (volatile uint32_t i = 0; i<500'000; ++i);

        // gpio_clear(GPIOE, GPIO15);
        // for (volatile uint32_t i = 0; i<500'000; ++i);
    }

}

