#ifndef __RADIO_H__
#define __RADIO_H__

#define PAYLOAD_DYNPD 0x3F

enum BufferState
{
	BufferState_Empty,
	BufferState_Receiving,
	BufferState_Ready
};

namespace Radio
{
	enum RadioState
	{
		RadioState_RX,
		RadioState_TX
	};
	
	struct Pipe
	{
		unsigned int payloadwidth : 6;
		unsigned int autoAA : 1;
		unsigned int enabled : 1;
		
		uint8_t address;
	};
	
	struct RadioStatus
	{
		unsigned int rTX_FULL : 1;
		unsigned int rRX_P_NO : 3;
		unsigned int rMAX_RT : 1;
		unsigned int rTX_DS : 1;
		unsigned int rRX_DR : 1;
		
		unsigned int : 1;		
	};
	
	extern uint8_t config[16];
	extern uint8_t txBuffer[38];
	extern uint8_t rxBuffer[34];
	
	extern BufferState configState;
	extern BufferState txBufferState;
	extern BufferState rxBufferState;
	
	void init();
	void reset();
	void flushTX();
	void flushRX();
	void configure();
	void poll();
	void setReg(uint8_t reg, uint8_t value);
	void setReg5(uint8_t reg, uint8_t *value);
	uint8_t getReg(uint8_t reg);
	uint8_t getPayloadWidth();
	struct RadioStatus getStatus();
};
#endif
