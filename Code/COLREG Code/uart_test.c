#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

// --- Configuration ---
#define BAUD 115200
#define MYUBRR ((F_CPU / (BAUD * 8UL)) - 1) 

// Filter Configuration (Smooths out the jitter)
#define FILTER_SIZE 8 

// LED Pins (Adjusted thresholds for longer range)
#define RED PD4
#define YELLOW PD5
#define GREEN PD6

// Sensor Pins
#define TRIGGER PD2
#define ECHO PD3

// --- Function Prototypes ---
void UART_init(unsigned int ubrr);
void UART_transmit(unsigned char data);
void UART_put_uint16(uint16_t n);
void pulse_trigger(void);
uint16_t measure_distance(void);
uint16_t get_smoothed_distance(void);

// --- UART Implementation ---
void UART_init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0A |= (1 << U2X0);   // Double Speed Mode for 115200
    UCSR0B = (1 << TXEN0);   // Enable TX
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

// --- Sensor & Filtering Logic ---
void pulse_trigger(void) {
    PORTD |= (1 << TRIGGER);
    _delay_us(10);
    PORTD &= ~(1 << TRIGGER);
}

uint16_t measure_distance(void) {
    pulse_trigger();

    // 1. Wait for Echo to go HIGH (with safety timeout)
    uint32_t safety = 0;
    while (!(PIND & (1 << ECHO))) {
        if (++safety > 100000) return 0; 
    }

    // 2. Measure pulse width
    uint32_t count = 0;
    // Increased timeout to 40000 to allow for ~6.5 meters
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

    // Outlier Rejection & Blind Zone Handling
    if (raw > 0 && raw < 2) {
        last_valid = 2; // Treat "too close" as the minimum
    } else if (raw >= 2 && raw <= 400) {
        last_valid = raw; // Use valid readings up to 4m
    }
    // (If raw is 0 or > 400, it's a timeout error; we keep last_valid)

    history[index] = last_valid;
    index = (index + 1) % FILTER_SIZE;

    for (uint8_t i = 0; i < FILTER_SIZE; i++) {
        sum += history[i];
    }
    return (uint16_t)(sum / FILTER_SIZE);
}

// --- Main Program ---
int main(void) {
    UART_init(MYUBRR);
    
    // Setup Pins: D2, D4, D5, D6 as Output; D3 as Input
    DDRD |= (1 << RED) | (1 << YELLOW) | (1 << GREEN) | (1 << TRIGGER);
    DDRD &= ~(1 << ECHO);
    
    // Enable D8 (B0) as requested for your specific board logic
    DDRB |= (1 << DDB0);
    PORTB |= (1 << PORTB0);

    while (1) {
        uint16_t dist = get_smoothed_distance();

        UART_put_uint16(dist);

        // Updated LED thresholds for the longer range
        PORTD &= ~((1 << RED) | (1 << YELLOW) | (1 << GREEN)); 
        if (dist < 15) {
            PORTD |= (1 << RED);    // Stop/Danger
        } else if (dist < 40) {
            PORTD |= (1 << YELLOW); // Warning
        } else {
            PORTD |= (1 << GREEN);  // Clear
        }

        // 60ms delay ensures "ghost echoes" from the previous ping 
        // have time to vanish before we start again.
        _delay_ms(60); 
    }
}