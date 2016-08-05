#ifndef __GPIO_H__
#define __GPIO_H__

namespace GPIO
{
	namespace SS
	{
		void high();
		void low();
		bool get();
	};
	namespace CE
	{
		void high();
		void low();
		bool get();
	};
	namespace IRQ
	{
		bool get();
	};
	
	void init();
};

#endif
