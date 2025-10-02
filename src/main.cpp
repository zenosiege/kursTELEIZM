#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

// ЗЕЛЕНЫЙ TX, БЕЛЫЙ RX

//gpio - general purpose input-output

// макросы для удобства
#define BAUDRATE 115200
#define DATABITS 8

static void clock_setup(void) {
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_HSI_64MHZ]);
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_USART2);
}

static void gpio_setup(void) {
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2 | GPIO3);
    gpio_set_af(GPIOA, GPIO_AF7, GPIO2 | GPIO3);
}

void usart_setup(void) {
    usart_set_baudrate(USART2, BAUDRATE);
    usart_set_databits(USART2, DATABITS);
    usart_set_stopbits(USART2, USART_STOPBITS_1);
    usart_set_parity(USART2, USART_PARITY_NONE);
    usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);
    usart_set_mode(USART2, USART_MODE_TX_RX);
    usart_enable(USART2);
}

/*
void usart_send (char *message ){
    while (*message) {
        usart_send_blocking(USART2, *message++);
    }
}
*/
char f = 'F';
char u = 'U';
char n = 'N';
char y = 'Y';
char newline = '\n';

int main() {
    clock_setup();
    gpio_setup();
    usart_setup();

    
    //uint32_t i;
    //uint16_t j =0, c = 0;
    while(true) {
        if (usart_get_flag(USART2, USART_ISR_RXNE)) {
            uint16_t data = usart_recv(USART2);
            usart_send_blocking(USART2, data);
        } 
        
        //for (volatile uint32_t i = 0; i < 500000; ++i); // задержки в попугаях
    }
}

