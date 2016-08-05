#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <inttypes.h>
#include <avr/pgmspace.h>

#include "GPIO.h"
#include "Radio.h"
#include "requests.h"

#include "SPI.h"

extern "C" {
	#include "usbdrv/usbdrv.h"
}

struct Flags
{
	unsigned int RadioReset : 1;
}flags;

uint8_t *writePointer;
uint8_t writeCount = 0;
BufferState *writeBufferState;

uchar usbFunctionWrite(uchar *data, uchar len)
{
	if(*writeBufferState != BufferState_Receiving)
	return 1;
	
	while(len > 0 && writeCount > 0)
	{
		*writePointer = *data;
		
		data++;
		writePointer++;
		
		len--;
		writeCount--;
	}
	
	if(writeCount == 0)
	{
		*writeBufferState = BufferState_Ready;
	}
	
	return writeCount == 0;
}

uint8_t *readPointer;
uint8_t readCount = 0;
BufferState *readBufferState;

uchar usbFunctionRead(uchar *data, uchar len)
{
	if(*readBufferState != BufferState_Ready)
		return 0;
	
	uchar transferred = 0;
	
	while(len > 0 && readCount > 0)
	{
		*data = *readPointer;
		
		data++;
		readPointer++;
		transferred++;
		
		len--;
		readCount--;
	}

	if(readCount == 0)
	{
		*readBufferState = BufferState_Empty;
	}
	
	return transferred;
}

uint8_t staticReplyBuffer[16];

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
	usbRequest_t *rq = (usbRequest_t *)data;
	
	if(writeCount > 0)
	*writeBufferState = BufferState_Empty;
	
	if(readCount > 0)
	*readBufferState = BufferState_Empty;
	
	switch(rq->bRequest)
	{
		case REQUEST_CONFIGURE:
		{
			if(Radio::configState != BufferState_Empty)
			return 0;
			
			Radio::configState = BufferState_Receiving;
			
			writePointer = (uint8_t *)&Radio::config;
			writeCount = sizeof(Radio::config);
			writeBufferState = &Radio::configState;
			
			return USB_NO_MSG;
		}
		
		case REQUEST_RESET:
		{
			flags.RadioReset = 1;
			return 0;
		}
		
		case REQUEST_SEND:
		{
			if(Radio::txBufferState != BufferState_Empty)
			return 0;
			
			if(rq->wLength.word > sizeof(Radio::txBuffer) || rq->wLength.word < (sizeof(Radio::txBuffer) - 32))
			return 0;
			
			Radio::txBufferState = BufferState_Receiving;
			
			writePointer = (uint8_t *)&Radio::txBuffer;
			writeCount = (uint8_t)rq->wLength.word;
			writeBufferState = &Radio::txBufferState;
			
			return USB_NO_MSG;
		}
		
		case REQUEST_GET_STATUS:
		{
			staticReplyBuffer[0] = 0;
			
			if(Radio::rxBufferState == BufferState_Ready)
			{
				staticReplyBuffer[0] = (1 << 6) | Radio::rxBuffer[1];	//Length
			}
			
			usbMsgPtr = staticReplyBuffer;
			return 1;
		}
		
		case REQUEST_RECEIVE:
		{
			readPointer = Radio::rxBuffer;
			readCount = Radio::rxBuffer[1] + 2;
			readBufferState = &Radio::rxBufferState;
			
			return USB_NO_MSG;
		}
	}
	return 0;
}

int main(void)
{
	wdt_disable();
	
	GPIO::init();
	Radio::init();
	
	usbInit();

	cli();
	usbDeviceDisconnect();
	_delay_ms(100);
	usbDeviceConnect();
	sei();
	
	while (1)
	{
		if(flags.RadioReset)
		{
			Radio::reset();
			flags.RadioReset = 0;
		}
		
		
		Radio::poll();
		usbPoll();
	}
}

