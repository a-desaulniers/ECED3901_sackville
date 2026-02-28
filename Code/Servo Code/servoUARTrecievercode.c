/*
 * MG90S_Servo.c
 *
 * Created: 2/26/2026 9:34:34 PM
 * Author : Owen Melanson
 */ 
#define F_CPU 16000000
#include <avr/io.h>
#include <util/delay.h>

#define BAUD 115200
#define MYUBRR ((F_CPU / (BAUD * 8UL)) - 1)

void UART_init(unsigned int ubrr) {
	UBRR0H = (unsigned char)(ubrr >> 8);
	UBRR0L = (unsigned char)ubrr;
	UCSR0A |= (1 << U2X0);   // double speed mode for 115200
	UCSR0B = (1 << RXEN0);
	UCSR0C = (1 << UCSZ00) |  (1 << UCSZ01);  // 8-bit data
}

unsigned char UART_read(void) {
	// Wait for data to be received
	while (!(UCSR0A & (1 << RXC0))) {
		// Wait until receive complete flag is set
	}
	// Return received data
	return UDR0;
}

void servo_init(void) {
	// Set OC1A (PB1 - Arduino pin 9) as output
	DDRB |= (1 << PINB1);
	
	// Configure Timer1 ONCE
	TCCR1A = (1 << WGM11);           // WGM11 = 1, WGM10 = 0
	TCCR1B = (1 << WGM12) | (1 << WGM13);  // WGM12 = 1, WGM13 = 1
	
	// Set non-inverting PWM mode
	TCCR1A |= (1 << COM1A1);
	
	// Set prescaler to 64 and start timer
	TCCR1B |= (1 << CS11) | (1 << CS10);
	
	// Set TOP value for 20ms period
	ICR1 = 4999;
	
	// Initialize to STOP position
}
void servo_run(void) {
	// Just change OCR values - DON'T re-initialize the timer!
	
	OCR1A = 312;  // Spin one direction
	_delay_ms(260);
	
	OCR1A = 374;  // Stop
	_delay_ms(5000);
	
	OCR1A = 416;  // Spin opposite direction
	_delay_ms(240);
	
	OCR1A = 374;  // Stop
}

int main(void) {
	servo_init();
	_delay_ms(100);
	UART_init(MYUBRR);
	_delay_ms(100);
	
	while (1) {
		char val;
		_delay_ms(10);
		val = UART_read();
		_delay_ms(10);
		if(val == '1'){
			_delay_ms(10);
			servo_run();
			_delay_ms(10);
		}
	}
	
	return 0;
}

