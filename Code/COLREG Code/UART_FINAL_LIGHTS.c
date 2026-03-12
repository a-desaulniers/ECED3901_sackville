/*
 * Four-Sensor Ultrasonic Array - SEQUENTIAL MODE
 * 
 * FOR TEAM 3, LICENSE PENDING
 */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define BAUD 115200
#define MYUBRR ((F_CPU / (BAUD * 8UL)) - 1)
#define FILTER_SIZE 8
#define NUM_SENSORS 4

#define RED PB2
#define YELLOW PB3
#define GREEN PB4

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
void pulse_trigger(void);
uint16_t measure_single_sensor(uint8_t echo_pin);
void measure_all_distances(uint16_t* d1, uint16_t* d2, uint16_t* d3, uint16_t* d4);
uint16_t get_smoothed_distance(uint16_t new_raw, uint8_t sensor_idx);

// Filter storage
static uint16_t history[NUM_SENSORS][FILTER_SIZE];
static uint8_t hist_idx[NUM_SENSORS] = {0};
static uint16_t last_valid[NUM_SENSORS] = {2,2,2,2};

// UART functions
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

// Trigger pulse
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

    // Wait for rising edge (up to 40 ms)
    while (!(PIND & (1 << echo_pin)) && timeout < 40000) {
        _delay_us(1);
        timeout++;
    }
    if (timeout >= 40000) return 0;

    // Measure pulse width
    while (PIND & (1 << echo_pin)) {
        _delay_us(1);
        count++;
        if (count > 40000) break;
    }

    // Convert to cm
    uint16_t distance = (count + 14) / 29;
    if (distance < 2) distance = 2;
    if (distance > 400) distance = 400;
    return distance;
}

// Measure all four sensors sequentially (10 ms gap)
void measure_all_distances(uint16_t* d1, uint16_t* d2, uint16_t* d3, uint16_t* d4) {
    *d1 = measure_single_sensor(ECHO1);
    _delay_ms(10);
    *d2 = measure_single_sensor(ECHO2);
    _delay_ms(10);
    *d3 = measure_single_sensor(ECHO3);
    _delay_ms(10);
    *d4 = measure_single_sensor(ECHO4);
}

// Smoothing filter – accepts 0 (no object) as valid
uint16_t get_smoothed_distance(uint16_t new_raw, uint8_t sensor_idx) {
    uint32_t sum = 0;

    if (new_raw == 0 || (new_raw >= 2 && new_raw <= 400)) {
        last_valid[sensor_idx] = new_raw;
    }

    history[sensor_idx][hist_idx[sensor_idx]] = last_valid[sensor_idx];
    hist_idx[sensor_idx] = (hist_idx[sensor_idx] + 1) % FILTER_SIZE;

    for (uint8_t i = 0; i < FILTER_SIZE; i++) {
        sum += history[sensor_idx][i];
    }
    return (uint16_t)(sum / FILTER_SIZE);
}

// Main
int main(void) {
    
    UART_init(MYUBRR);

    // Short delay to let everything settle and avoid initial UART garbage
    _delay_ms(200);

    // Send a blank line to flush any partial line from the host buffer
    UART_transmit('\r');
    UART_transmit('\n');

    // Pin setup
    DDRD |= (1 << TRIGGER);
    DDRD &= ~((1<<ECHO1)|(1<<ECHO2)|(1<<ECHO3)|(1<<ECHO4));
    PORTD &= ~((1<<ECHO1)|(1<<ECHO2)|(1<<ECHO3)|(1<<ECHO4));
    //turn LED ON 
	//COLREGS from pin D8
	DDRB |= (1 << DDB0);
	PORTB |= (1 << PORTB0);
	DDRB |= (1 << RED) | (1 << YELLOW) | (1 << GREEN); //define the lights here 

    // Initialize filter histories
    for (uint8_t s = 0; s < NUM_SENSORS; s++) {
        for (uint8_t i = 0; i < FILTER_SIZE; i++) {
            history[s][i] = 2;
        }
    }

    // Continuous data output
    while (1) {
        uint16_t raw[4], smooth[4];

        measure_all_distances(&raw[0], &raw[1], &raw[2], &raw[3]);

        for (uint8_t i = 0; i < 4; i++) {
            smooth[i] = get_smoothed_distance(raw[i], i);
        }
		// looking at the lowest value read by the john 
		uint16_t min = 401; //since the max distance read is 400 
		for (uint8_t i = 0; i < 4; i++) {
			if (smooth[i] != 0 && smooth[i] < min) {
				min = smooth[i];
			}
		}
		PORTB &= ~((1 << RED) | (1 << YELLOW) | (1 << GREEN)); //turn the john on 
		if (min >= 0 && min <= 5) {
			PORTB |= (1 << RED);       // Red ON
		}
		if(min > 5 && min <= 10){
			PORTB |= (1 << YELLOW); //yellow on bruh
		}if(min > 10 && min <= 400){
			PORTB |= (1 << GREEN); //Green on bruh
		}
		
        // Output CSV line: S1,S2,S3,S4
        UART_put_uint16(smooth[0]); UART_transmit(','); //the values i need to look at pull from them and check which one is the lowest and then base calc
        UART_put_uint16(smooth[1]); UART_transmit(',');
        UART_put_uint16(smooth[2]); UART_transmit(',');
        UART_put_uint16(smooth[3]); UART_transmit('\r'); UART_transmit('\n');
		

        _delay_ms(50);
    }
}
