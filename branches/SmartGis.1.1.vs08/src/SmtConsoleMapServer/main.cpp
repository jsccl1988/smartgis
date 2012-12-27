#include <iostream>

#include "msvr_mapserver.h"
using namespace Smt_MapServer;

int main(int argc, char* argv[])
{
	if( FAILED(::CoInitialize(NULL)) ) 
		return -1;

	cout << "SmtMapServer Console:Initial... \n";
	SmtMapServer mapSvr;
	if (SMT_ERR_NONE == mapSvr.Init() &&
		SMT_ERR_NONE == mapSvr.Create())
	{
		cout << "SmtMapServer Console:Running... \n";
		mapSvr.Run();

		mapSvr.Destroy();
	}

	cout << "SmtMapServer Console:Destory... \n";

	::CoUninitialize();

	return 0;
}