/*
 * Four-Sensor Ultrasonic Array - SEQUENTIAL MODE (FINAL)
 * Eliminates crosstalk, corrects timeout & filter bugs.
 * Trigger: PD2 (shared)
 * Echoes: PD3, PD4, PD5, PD6
 */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define BAUD 115200
#define MYUBRR ((F_CPU / (BAUD * 8UL)) - 1)
#define FILTER_SIZE 8
#define NUM_SENSORS 4

// Pin definitions
#define TRIGGER PD2
#define ECHO1   PD3
#define ECHO2   PD4
#define ECHO3   PD5
#define ECHO4   PD6

// Function prototypes
void UART_init(unsigned int ubrr);
void UART_transmit(unsigned char data);
void UART_put_uint16(uint16_t n);
void UART_put_string(const char* str);
void pulse_trigger(void);
uint16_t measure_single_sensor(uint8_t echo_pin);
void measure_all_distances(uint16_t* d1, uint16_t* d2, uint16_t* d3, uint16_t* d4);
uint16_t get_smoothed_distance(uint16_t new_raw, uint8_t sensor_idx);

// Filter storage
static uint16_t history[NUM_SENSORS][FILTER_SIZE];
static uint8_t hist_idx[NUM_SENSORS] = {0};
static uint16_t last_valid[NUM_SENSORS] = {2,2,2,2};

// UART functions (unchanged)
void UART_init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0A |= (1 << U2X0);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (3 << UCSZ00);
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
}

void UART_put_string(const char* str) {
    while (*str) UART_transmit(*str++);
}

// Trigger pulse (10 µs)
void pulse_trigger(void) {
    PORTD |= (1 << TRIGGER);
    _delay_us(10);
    PORTD &= ~(1 << TRIGGER);
}

// Measure one sensor (returns 0 if no object)
uint16_t measure_single_sensor(uint8_t echo_pin) {
    uint32_t count = 0;
    uint16_t timeout = 0;

    pulse_trigger();

    // Wait for rising edge – allow up to 40 ms (full range)
    while (!(PIND & (1 << echo_pin)) && timeout < 40000) {
        _delay_us(1);
        timeout++;
    }
    if (timeout >= 40000) return 0;   // No echo

    // Measure pulse width
    while (PIND & (1 << echo_pin)) {
        _delay_us(1);
        count++;
        if (count > 40000) break;     // Max range
    }

    // Convert to cm (58 µs per cm, rounding)
    uint16_t distance = (count + 29) / 58;
    if (distance < 2) distance = 2;
    if (distance > 400) distance = 400;
    return distance;
}

// Measure all four sensors sequentially (10 ms gap to kill crosstalk)
void measure_all_distances(uint16_t* d1, uint16_t* d2, uint16_t* d3, uint16_t* d4) {
    *d1 = measure_single_sensor(ECHO1);
    _delay_ms(10);
    *d2 = measure_single_sensor(ECHO2);
    _delay_ms(10);
    *d3 = measure_single_sensor(ECHO3);
    _delay_ms(10);
    *d4 = measure_single_sensor(ECHO4);
}

// Smoothing filter – now accepts 0 (no object) as a valid reading
uint16_t get_smoothed_distance(uint16_t new_raw, uint8_t sensor_idx) {
    uint32_t sum = 0;

    // Accept 0 (no object) OR valid 2–400 cm readings
    if (new_raw == 0 || (new_raw >= 2 && new_raw <= 400)) {
        last_valid[sensor_idx] = new_raw;
    }

    // Update circular buffer
    history[sensor_idx][hist_idx[sensor_idx]] = last_valid[sensor_idx];
    hist_idx[sensor_idx] = (hist_idx[sensor_idx] + 1) % FILTER_SIZE;

    // Average
    for (uint8_t i = 0; i < FILTER_SIZE; i++) {
        sum += history[sensor_idx][i];
    }
    return (uint16_t)(sum / FILTER_SIZE);
}

// Main
int main(void) {
    UART_init(MYUBRR);

    // Pin setup
    DDRD |= (1 << TRIGGER);                       // Trigger output
    DDRD &= ~((1<<ECHO1)|(1<<ECHO2)|(1<<ECHO3)|(1<<ECHO4)); // Echo inputs
    PORTD &= ~((1<<ECHO1)|(1<<ECHO2)|(1<<ECHO3)|(1<<ECHO4)); // No pull-ups

    // Initialise filter histories to 2 cm (minimum safe)
    for (uint8_t s = 0; s < NUM_SENSORS; s++) {
        for (uint8_t i = 0; i < FILTER_SIZE; i++) {
            history[s][i] = 2;
        }
    }

    UART_put_string("4-Sensor Ultrasonic - SEQUENTIAL MODE (FIXED)\r\n");
    UART_put_string("Format: S1,S2,S3,S4\r\n");

    while (1) {
        uint16_t raw[4], smooth[4];

        measure_all_distances(&raw[0], &raw[1], &raw[2], &raw[3]);

        for (uint8_t i = 0; i < 4; i++) {
            smooth[i] = get_smoothed_distance(raw[i], i);
        }

        // Output CSV
        UART_put_uint16(smooth[0]); UART_transmit(',');
        UART_put_uint16(smooth[1]); UART_transmit(',');
        UART_put_uint16(smooth[2]); UART_transmit(',');
        UART_put_uint16(smooth[3]); UART_transmit('\r'); UART_transmit('\n');

        _delay_ms(50);   // Overall cycle cooldown
    }
}