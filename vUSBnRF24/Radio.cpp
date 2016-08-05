#include <inttypes.h>
#include <string.h>
#include <avr/io.h>
#include "Radio.h"
#include "GPIO.h"
#include "SPI.h"
#include "registers.h"

#define CONFIG_REG (_BV(EN_CRC) | _BV(CRCO) | _BV(PWR_UP) | _BV(PRIM_RX))

namespace Radio
{
	const uint8_t convert_br_reg[] = {0x10, 0x00,0x08};
	
	uint8_t lastTXaddress[5];
	uint8_t rxDataAvailable = 0;
	
	RadioState radioState = RadioState_RX;
	
	uint8_t config[16];
	uint8_t txBuffer[38];
	uint8_t rxBuffer[34];
	
	BufferState configState = BufferState_Empty;
	BufferState txBufferState = BufferState_Empty;
	BufferState rxBufferState = BufferState_Empty;
	
	Ack lastAck = Ack_Empty;
	
	void readRX(RadioStatus status)
	{
		memset(&rxBuffer, 0, sizeof(rxBuffer));
		
		rxBuffer[0] = status.rRX_P_NO;
		rxBuffer[1] = getPayloadWidth();
		
		GPIO::SS::low();
		
		SPI::transfer(R_RX_PAYLOAD);
		
		for(uint8_t i = 0; i < rxBuffer[1]; i++)
		rxBuffer[i + 2] = SPI::transfer(0x00);
		
		GPIO::SS::high();
		
		rxBufferState = BufferState_Ready;
		
		rxDataAvailable = getStatus().rRX_P_NO != 0x07;
	}
	
	uint16_t pollRXcounter = 0;
	
	void poll()
	{
		if(configState == BufferState_Ready && radioState == RadioState_RX)
		{
			configure();
			configState = BufferState_Empty;
		}
		
		if(txBufferState == BufferState_Ready && radioState == RadioState_RX)
		{
			struct RadioStatus status = getStatus();
			
			if(!status.rTX_FULL)
			{
				GPIO::CE::low();
				
				if(memcmp(txBuffer + 1, lastTXaddress, 5))		//memcmp returns 0 on match
				{
					memcpy(lastTXaddress, txBuffer + 1, 5);
					
					setReg5(TX_ADDR, lastTXaddress);
					setReg5(RX_ADDR_P0, lastTXaddress);
				}
				
				setReg(CONFIG, CONFIG_REG & ~_BV(PRIM_RX));
				
				GPIO::SS::low();
				
				if(txBuffer[0] & (1 << 6))
				SPI::transfer(W_TX_PAYLOAD);
				else
				SPI::transfer(W_TX_PAYLOAD_NOACK);
				
				for(uint8_t i = 0; i < (txBuffer[0] & 0x3F); i++)
				SPI::transfer(txBuffer[i + 6]);
				
				GPIO::SS::high();
				GPIO::CE::high();
				
				lastAck = Ack_Empty;
				radioState = RadioState_TX;
			}
		}
		
		if(!GPIO::IRQ::get() || pollRXcounter-- == 0)
		{
			struct RadioStatus status = getStatus();
			
			if(status.rMAX_RT)
			{
				txBufferState = BufferState_Empty;
				lastAck = Ack_NAK;
				
				GPIO::CE::low();
				
				setReg(CONFIG, CONFIG_REG);
				
				GPIO::CE::high();
				
				setReg(STATUS, _BV(MAX_RT));
				
				radioState = RadioState_RX;
			}
			
			if(status.rTX_DS)
			{
				txBufferState = BufferState_Empty;
				lastAck = Ack_Received;

				GPIO::CE::low();

				setReg(CONFIG, CONFIG_REG);

				GPIO::CE::high();

				setReg(STATUS, _BV(TX_DS));

				radioState = RadioState_RX;
			}
			
			if(status.rRX_DR)
			{
				rxDataAvailable = 1;
				
				setReg(STATUS, _BV(RX_DR));
			}
			
			if(status.rRX_P_NO != 0x07 )
			{
				rxDataAvailable = 1;
			}
		}
		
		if(rxDataAvailable && rxBufferState == BufferState_Empty)
			readRX(getStatus());
	}
	
	void reset()
	{
		memset(&config, 0, 11);		//clear pipes, channel
		config[11] = 0xBF;
		memset(config + 12, 0x55, 4);
		
		memset(rxBuffer, 0, sizeof(rxBuffer));
		memset(txBuffer, 0, sizeof(txBuffer));
		
		memset(lastTXaddress, 0, 5);
		
		
		configState = BufferState_Empty;
		txBufferState = BufferState_Empty;
		rxBufferState = BufferState_Empty;
		
		lastAck = Ack_Empty;
		
		rxDataAvailable = 0;

		flushTX();
		flushRX();
		configure();
	}
	
	void flushTX()
	{
		GPIO::SS::low();
		
		SPI::transfer(FLUSH_TX);
		
		GPIO::SS::high();
	}
	
	void flushRX()
	{
		GPIO::SS::low();
		
		SPI::transfer(FLUSH_RX);
		
		GPIO::SS::high();
	}
	
	void init()
	{
		GPIO::CE::low();
		GPIO::SS::high();
		
		reset();
		
		GPIO::CE::high();
	}
	
	void setReg(uint8_t reg, uint8_t value)
	{
		GPIO::SS::low();
		
		SPI::transfer(reg | W_REGISTER);
		SPI::transfer(value);
		
		GPIO::SS::high();
	}
	
	void setReg5(uint8_t reg, uint8_t *value)
	{
		GPIO::SS::low();
		
		SPI::transfer(reg | W_REGISTER);
		
		for(uint8_t i = 0; i < 5; i++)
		{
			SPI::transfer(value[4-i]);		//write LSB first
		}
		
		GPIO::SS::high();
	}
	
	uint8_t getReg(uint8_t reg)
	{
		GPIO::SS::low();
		
		SPI::transfer(reg | R_REGISTER);
		uint8_t val = SPI::transfer(0x00);
		
		GPIO::SS::high();
		
		return val;
	}
	
	uint8_t getPayloadWidth()
	{
		GPIO::SS::low();
		
		SPI::transfer(R_RX_PL_WID);
		uint8_t val = SPI::transfer(0x00);
		
		GPIO::SS::high();
		
		return val;
	}
	
	struct RadioStatus getStatus()
	{
		GPIO::SS::low();
		
		uint8_t val = SPI::transfer(NOP);
		
		GPIO::SS::high();
		
		return *((struct RadioStatus*)&val);
	}
	
	void configure()
	{
		GPIO::CE::low();
		
		setReg(CONFIG, CONFIG_REG);
		
		uint8_t en_aa = 1;
		uint8_t en_rxaddr = 1;
		uint8_t dynpd = 1;
		
		struct Pipe *pipecfg = (struct Pipe*)config;
		
		for(uint8_t pipe = 0; pipe < 5; pipe++)
		{
			if(!pipecfg[pipe].enabled)		//enabled
			continue;
			
			en_rxaddr |= 1 << (pipe + 1);
			
			en_aa |= pipecfg[pipe].autoAA << (pipe + 1);
			
			dynpd |= (pipecfg[pipe].payloadwidth == PAYLOAD_DYNPD) << (pipe + 1);
			
			
			if(pipe > 0)	//only for pipe 2 and up
				setReg(RX_ADDR_P1 + pipe, pipecfg[pipe].address);
			
			if(!(pipecfg[pipe].payloadwidth == PAYLOAD_DYNPD))
				setReg(RX_PW_P1 + pipe, pipecfg[pipe].payloadwidth);
		}
		
		setReg(EN_AA, en_aa);
		setReg(EN_RXADDR, en_rxaddr);
		setReg(DYNPD, dynpd);
	
		
		setReg(SETUP_AW, 3);
		setReg(SETUP_RETR, config[11] | (1 << 4));
		setReg(RF_CH, config[10] & 0x7F);
		setReg(RF_SETUP, convert_br_reg[config[11] >> 6] | (config[11] >> 3));
							// Bitrate						RF Power
		
		uint8_t rxAddress[5];
		
		for(uint8_t i = 0; i < 4; i++)
			rxAddress[i] = *(config + 12 + i);
		rxAddress[4] = pipecfg[0].address;
		
		setReg5(RX_ADDR_P1, rxAddress);
		
		setReg(FEATURE, _BV(EN_DPL) | _BV(EN_DYN_ACK));
		
		GPIO::CE::high();
	}
}
