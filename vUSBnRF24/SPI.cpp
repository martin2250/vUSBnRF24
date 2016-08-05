#include <avr/io.h>

#include "SPI.h"

namespace SPI
{

	uint8_t transfer(uint8_t data)
	{
		USIDR = data;
		USISR = (1<< USIOIF); //Clear the USI counter overflow flag

		do
		{
			USICR = (1<< USIWM0) | (1<< USICLK) | (1<< USICS1) | (1<< USITC);
		} while((USISR & (1<< USIOIF)) == 0);

		return USIDR;
	}

}