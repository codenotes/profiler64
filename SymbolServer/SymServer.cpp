#include <windows.h>
#include <imagehlp.h>
#include <intrin.h>
#include <strsafe.h>
#include <vector>
#include <map>
#include <iostream>
#include <iomanip>
using namespace std;
#include "SymServer.h"

//Flag to indicate the result of symbol initialization
static BOOL		bInitResult = FALSE;

//Base address of the loaded module
static DWORD64  dwBaseAddr = 0;

//Critical section to protect the g_mapProfileInfo
CRITICAL_SECTION g_csProfileInfo; 

//Map between the thread id to list of profile info
map<int,vector<ProfileInfo> > g_mapProfileInfo;

//*********************************************************************************************
//Iterate through the map and for each thread id print the profile
//data strored in the vector
void DisplayProfileData( )
{
	cout<<"##########################Profile Information############################"<<endl;
	map<int,vector<ProfileInfo> >::iterator itThread = g_mapProfileInfo.begin(  );
	for( ; itThread != g_mapProfileInfo.end(); ++itThread )
	{
		vector<ProfileInfo> &vecProfileInfo =  itThread->second;
		vector<ProfileInfo>::iterator it = vecProfileInfo.begin();
		for( ; it != vecProfileInfo.end(); ++it )
		{
			it->Display();
		}
	}
}
//*********************************************************************************************
//Function to load the symbols of the module
void InitSymbols(void* pAddress )
{
	//Query the memory
	char moduleName[MAX_PATH];
	TCHAR modShortNameBuf[MAX_PATH];
	MEMORY_BASIC_INFORMATION mbi;

	//Get the module name where the address is available
	VirtualQuery((void*)pAddress,&mbi,sizeof(mbi));
	GetModuleFileNameA((HMODULE)mbi.AllocationBase, 
						moduleName, MAX_PATH );

	//Initialize the symbols
	bInitResult = SymInitialize(GetCurrentProcess(),moduleName, TRUE );

	
	//Load the module
	dwBaseAddr = SymLoadModule64(GetCurrentProcess(),
									NULL,
									(PCSTR)moduleName,
									NULL,
									(DWORD)mbi.AllocationBase,
									0);
	//Set the options
	SymSetOptions( SymGetOptions()   &~SYMOPT_UNDNAME );	
}
//*********************************************************************************************
//Dll entry point function
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		{
			//Intitalize the critical section
			::InitializeCriticalSection(&g_csProfileInfo);

			//Loads the symbols of the module
			InitSymbols( hModule);
			break;
		}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		{
			break;
		}
	case DLL_PROCESS_DETACH:
		{
			//At the end, when the dll is detached from the process
			//dump all the profiled data
			DisplayProfileData( );

			//Clean-up the symbols
			SymCleanup( GetCurrentProcess() );
			break;
		}
		break;
	}
	return TRUE;
}
//*********************************************************************************************
void FindFunction(void* pa, char* &szFuncName )
{

	DWORD64 symDisplacement = 0;

	char undName[1024];
	if( dwBaseAddr )
	{			
		//Allocate the memory for PSYMBOL_INFO
		TCHAR  buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)] ;
		memset(&buffer,0,sizeof(buffer));
		PSYMBOL_INFO    pSymbolInfo	= ( PSYMBOL_INFO)buffer;
		pSymbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO) ;
		pSymbolInfo->MaxNameLen	= MAX_SYM_NAME;

		//Get the name of the symbol using the address
		BOOL bResult = SymFromAddr( GetCurrentProcess(), (DWORD64)pa,&symDisplacement, pSymbolInfo );

		LPVOID lpMsgBuf;
		LPVOID lpDisplayBuf;

		//If symbol is found, then get its undecorated name.The name could be a decorated one
		//as the name mangling schemes are deployed
		if ( bResult )
		{
			if (  UnDecorateSymbolName( pSymbolInfo->Name, undName,
											sizeof(undName),
											UNDNAME_NO_MS_KEYWORDS |
											UNDNAME_NO_ACCESS_SPECIFIERS |
											UNDNAME_NO_FUNCTION_RETURNS |
											UNDNAME_NO_ALLOCATION_MODEL |
											UNDNAME_NO_ALLOCATION_LANGUAGE |
											UNDNAME_NO_ARGUMENTS  |
											UNDNAME_NO_SPECIAL_SYMS |
											UNDNAME_NO_MEMBER_TYPE))

			{
				//Ignore the unnecessary calls emulated from std library
				if( strstr(undName,"std::")) 
				{
					//Skip the symbol
				}
				else
				{
					strcpy_s(undName,pSymbolInfo->Name);
					szFuncName = new char [ strlen(undName) + 2 ];
					strcpy_s(szFuncName,strlen(undName)+1,undName);
				}
			}

		}
		else
		{
			DWORD lastError = GetLastError();
			FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			lastError,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &lpMsgBuf,
			0, NULL );

			lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
						(lstrlen((LPCTSTR)lpMsgBuf) + 40) * sizeof(TCHAR)); 
		

			StringCchPrintf((LPTSTR)lpDisplayBuf, 
							LocalSize(lpDisplayBuf) / sizeof(TCHAR),
							TEXT("failed with error %d: %s"), 
							lastError, lpMsgBuf); 

		}
	}
}
//*********************************************************************************************
void FindSymbol( void* pCallee )
{
	char* szCallee = NULL;
	FindFunction( pCallee, szCallee );
	if( szCallee )
	{
		cout << "In:" << szCallee << endl;
		ProfileInfo profInfo;
		strcpy_s( profInfo.m_sFunName, szCallee );

		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		profInfo.m_liStartTime = li;

		DWORD dwTid = ::GetCurrentThreadId();
		profInfo.m_dwThreadID = dwTid;

		::EnterCriticalSection(&g_csProfileInfo);
		map<int,vector<ProfileInfo> >::iterator itTID = g_mapProfileInfo.find( dwTid );
		if( itTID != g_mapProfileInfo.end() )
		{
			itTID->second.push_back( profInfo );
		}
		else
		{
			vector<ProfileInfo> vecProfInfo;
			vecProfInfo.push_back( profInfo );
			g_mapProfileInfo.insert( make_pair(dwTid,vecProfInfo) );

		}
		::LeaveCriticalSection(&g_csProfileInfo);
	}

}
//*******************************************************************************
void FindSymbol_1( void* pCallee )
{
	char* szCallee = NULL;
	FindFunction( pCallee, szCallee );


	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);

	//Find if the callee is there in the vectoar
	if( szCallee )
	{
		//Check for the thread id
		DWORD dwTid = ::GetCurrentThreadId();

		::EnterCriticalSection(&g_csProfileInfo);
		map<int,vector<ProfileInfo> >::iterator itTID = g_mapProfileInfo.find( dwTid );

		if( itTID != g_mapProfileInfo.end() )
		{
			vector<ProfileInfo>	&vecProfileInfo = itTID->second;
			vector<ProfileInfo>::iterator it = vecProfileInfo.end();
			--it;
			while( 1 )
			{
				if( strcmp(it->m_sFunName,szCallee) == 0 && (!it->bFilled) )
				{
					it->m_liEndTime = li;
					it->bFilled = true;
					break;
				}
				if( it == vecProfileInfo.begin() )
					break;
				--it;
			}
		
		}
		cout << "Out:"<<szCallee << endl;
		::LeaveCriticalSection(&g_csProfileInfo);
	}
}
//*******************************************************************************
void ProfileInfo::Display()
{
	cout<<"Function  : "<<m_sFunName<<endl;
	cout<<"Thread    : "<<m_dwThreadID<<endl;
	
	//Number of ticks
	LARGE_INTEGER elapsedTime;
	elapsedTime.QuadPart = m_liEndTime.QuadPart - m_liStartTime.QuadPart;

	//Number of ticks-per-sec
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);

	elapsedTime.QuadPart = (elapsedTime.QuadPart) * 1000 * 1000 / freq.QuadPart;

	cout<<"Elapsed time : "<<elapsedTime.QuadPart<<" microsec"<<endl;
	cout<<"***********************************************"<<endl;
}
//*******************************************************************************