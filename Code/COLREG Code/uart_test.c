#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

// Config
#define BAUD 115200
#define MYUBRR ((F_CPU / (BAUD * 8UL)) - 1) 

// filter configuration, helps with jitter
#define FILTER_SIZE 8 

// LED Pin def 
#define RED PD4
#define YELLOW PD5
#define GREEN PD6

// Sensor Pin def
#define TRIGGER PD2
#define ECHO PD3

void UART_init(unsigned int ubrr);
void UART_transmit(unsigned char data);
void UART_put_uint16(uint16_t n);
void pulse_trigger(void);
uint16_t measure_distance(void);
uint16_t get_smoothed_distance(void);

// UART config + enable
void UART_init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0A |= (1 << U2X0);   // double speed mode for 115200
    UCSR0B = (1 << TXEN0);   // enable TX
    UCSR0C = (3 << UCSZ00);  // 8-bit data
}

void UART_transmit(unsigned char data) {
    while (!(UCSR0A & (1 << UDRE0))); 
    UDR0 = data;
}

void UART_put_uint16(uint16_t n) {
    char buf[5]; 
    int8_t i = 0;
    if (n == 0) {
        UART_transmit('0');
    } else {
        while (n > 0) {
            buf[i++] = (n % 10) + '0';
            n /= 10;
        }
        while (--i >= 0) UART_transmit(buf[i]);
    }
    UART_transmit('\r'); 
    UART_transmit('\n'); 
}

// sensor and filtering logic
void pulse_trigger(void) {
    PORTD |= (1 << TRIGGER);
    _delay_us(10);
    PORTD &= ~(1 << TRIGGER);
}

uint16_t measure_distance(void) {
    pulse_trigger();

    // wait for echo to go high (with safety timeout)
    uint32_t safety = 0;
    while (!(PIND & (1 << ECHO))) {
        if (++safety > 100000) return 0; 
    }

    // measure pulse width
    uint32_t count = 0;
    // Increased timeout to 40000 to allow for 6.5ish meters
    while (PIND & (1 << ECHO)) {   
        _delay_us(1);
        count++;
        if (count > 40000) break;  
    }
    return (uint16_t)(count / 58);
}

uint16_t get_smoothed_distance(void) {
    static uint16_t history[FILTER_SIZE];
    static uint8_t index = 0;
    static uint16_t last_valid = 0;
    uint32_t sum = 0;

    uint16_t raw = measure_distance();

    // outlier rejection + blind zone handling
    if (raw > 0 && raw < 2) {
        last_valid = 2; // treat too close as the minimum
    } else if (raw >= 2 && raw <= 400) {
        last_valid = raw; // use readings up to 4m
    }
    // If raw is 0 or > 400, chuck it)

    history[index] = last_valid;
    index = (index + 1) % FILTER_SIZE;

    for (uint8_t i = 0; i < FILTER_SIZE; i++) {
        sum += history[i];
    }
    return (uint16_t)(sum / FILTER_SIZE);
}

// main
int main(void) {
    UART_init(MYUBRR);
    
    // setup pins: D2, D4, D5, D6 as output, D3 as input
    DDRD |= (1 << RED) | (1 << YELLOW) | (1 << GREEN) | (1 << TRIGGER);
    DDRD &= ~(1 << ECHO);
    
    // Enable D8 (B0) 
    DDRB |= (1 << DDB0);
    PORTB |= (1 << PORTB0);

    while (1) {
        uint16_t dist = get_smoothed_distance();

        UART_put_uint16(dist);

        // updated LED thresholds for the longer range
        PORTD &= ~((1 << RED) | (1 << YELLOW) | (1 << GREEN)); 
        if (dist < 15) {
            PORTD |= (1 << RED);    // stop/danger
        } else if (dist < 40) {
            PORTD |= (1 << YELLOW); // warning
        } else {
            PORTD |= (1 << GREEN);  // clear
        }

        // have time to vanish before we start again.
        _delay_ms(60); 
    }
}