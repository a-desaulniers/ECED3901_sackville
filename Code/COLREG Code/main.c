/*
 * GccApplication9.c
 *
 * Created: 2026-01-31 10:03:05 PM
 * Author : azzie
 */ 
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#define RED PD4
#define YELLOW PD5
#define GREEN PD6

void pulse_trigger() {
	PORTD |= (1 << PD2);
	_delay_us(10);
	PORTD &= ~(1 << PD2);
}

uint16_t measure_distance() {
	pulse_trigger();

	// Wait for echo HIGH
	while (!(PIND & (1 << PD3)));

	// Measure pulse width
	uint16_t count = 0;
	while (PIND & (1 << PD3)) {
		_delay_us(1);
		count++;
		if (count > 30000) break; 
	}
	uint16_t distance = count / 58; // cm
	return distance;
}


int main(void)
{
    //COLREGS from pin D8
    DDRB |= (1 << DDB0);
    PORTB |= (1 << PORTB0);
	//test pin
		
	//Distance stuff using ultrasonic sensor for now
	DDRD |= (1 << RED) | (1 << YELLOW) | (1 << GREEN);
	DDRD |= (1 << PD2);  // Trigger out
	DDRD &= ~(1 << PD3); // Echo in
    while (1) 
    {
		uint16_t dist = measure_distance();

		// Turn off all LEDs
		PORTD &= ~((1 << RED) | (1 << YELLOW) | (1 << GREEN));

		if (dist < 5){
			PORTD |= (1 << RED);
			} else if (dist < 10){
			PORTD |= (1 << YELLOW);
			} else {
			PORTD |= (1 << GREEN);
		}

		_delay_ms(200);
    }
}

