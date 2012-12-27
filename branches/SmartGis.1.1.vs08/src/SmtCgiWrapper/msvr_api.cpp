#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <io.h>
#include "msvr_api.h"

long msvrPrintText(const char * szText)
{
	if (strlen(szText) == 0)
		return SMT_ERR_FAILURE;
	 
	fprintf(stdout,"Content-Type:TEXT;\n\n"); 
	fprintf(stdout,szText);

	return SMT_ERR_NONE;
}

long msvrPrintMime(const char * pBuffer, long lBufferSize)
{
	int result = 0;
	fprintf(stdout,"Content-Type:MIME;\n\n"); 
	result = _setmode(_fileno(stdout),_O_BINARY); 

	if( result == -1)
	{
		fprintf(stdout,"模式输出转换出错，程序将退出");
		exit(1);
	}
	else
		fwrite(pBuffer,1,lBufferSize,stdout);

	fflush(stdout);
	_setmode(_fileno(stdout),_O_TEXT);

	return SMT_ERR_NONE;
}

long msvrPrintBin(const char * pBuffer, long lBufferSize)
{
	int result = 0;
	result = _setmode(_fileno(stdout),_O_BINARY); 

	if( result == -1)
	{
		fprintf(stdout,"模式输出转换出错，程序将退出");
		exit(1);
	}
	else
		fwrite(pBuffer,1,lBufferSize,stdout);

	fflush(stdout);
	_setmode(_fileno(stdout),_O_TEXT);

	return SMT_ERR_NONE;
}