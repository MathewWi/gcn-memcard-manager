#ifndef _MCM__
#define _MCM__

void __Log(int logNumber, const char* text, ...)
	{(void)logNumber; (void)text;}
void __Logv(int log, int v, const char *format, ...)
	{(void)log; (void)v; (void)format;}

#include "GUI/MemcardManager.h"
#include "Timer.h"
#include "IPLTime.h"

class MCMApp
	: public wxApp
{
	public:
		bool OnInit();
		
};
#endif
