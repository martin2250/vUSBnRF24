#include <avr/io.h>

#include "GPIO.h"

#define SS_REG PORTA
#define SS_PIN PA2

#define CE_REG PORTA
#define CE_PIN PA1

#define IRQ_REG PINA
#define IRQ_PIN PA0

void GPIO::SS::high()
{
	SS_REG |= _BV(SS_PIN);
}

void GPIO::SS::low()
{
	SS_REG &= ~_BV(SS_PIN);
}

bool GPIO::SS::get()
{
	return (SS_REG >> SS_PIN) & 0x01;
}

void GPIO::CE::high()
{
	CE_REG |= _BV(CE_PIN);
}

void GPIO::CE::low()
{
	CE_REG &= ~_BV(CE_PIN);
}

bool GPIO::CE::get()
{
	return (CE_REG >> CE_PIN) & 0x01;
}

bool GPIO::IRQ::get()
{
	return (IRQ_REG >> IRQ_PIN) & 0x01;
}

void GPIO::init()
{
	PORTA = 0;
	PORTB = 0;

	DDRA = _BV(PA1) | _BV(PA2);
	DDRB = _BV(PB1) | _BV(PB2);

	PORTA = _BV(PA0) | _BV(PA2);	//IRQ pullup, SS

	USICR = _BV(USIWM0);
}