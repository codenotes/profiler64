#include <windows.h>
#include "SymServer.h"
#include "MyStaticLib.h"
#include <iostream>
using namespace std;

extern "C" void _penter();
extern "C" void _pexit();

//*****************************************************************
void Foo1( int a )
{
	a = a * a;
	//Calling function from the static library
	int result = Add(a,a);
}
//*****************************************************************
void Foo(int a, int b)
{
	int c = a-b;
	//cout<<" c = "<<c<<endl;
	Foo1(c);
}
struct test
{
	void boob()
	{

	}
};
//*****************************************************************
void main()
{
	test t;

	t.boob();
	::Sleep(500);
	Foo(5,6);	
}
//*****************************************************************
