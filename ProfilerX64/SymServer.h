#ifndef __SYMSERVERDLL__
#define __SYMSERVERDLL__


#ifdef SYMBOLSERVER_EXPORTS
	#define MYEXPORT __declspec( dllexport )
#else
	#define MYEXPORT __declspec( dllimport )
#endif


extern"C" void MYEXPORT FindSymbol( void* pa );
extern"C" void MYEXPORT FindSymbol_1( void* pa);

struct ProfileInfo
{
	ProfileInfo()
	{
		bFilled = false;
	}

	//Name of the function
	char	m_sFunName[256];

	//Thread executig the function
	DWORD   m_dwThreadID;

	//Start time
	LARGE_INTEGER	m_liStartTime;

	//End time
	LARGE_INTEGER	m_liEndTime;

	bool	bFilled;

	void Display();
};

#endif
