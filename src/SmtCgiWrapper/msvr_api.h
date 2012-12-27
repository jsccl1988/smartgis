#ifndef _MSVR_API_H
#define _MSVR_API_H

#include "smt_core.h"

long	 msvrPrintText(const char * szText) ;
long	 msvrPrintMime(const char * pBuffer, long lBufferSize) ;
long	 msvrPrintBin(const char * pBuffer, long lBufferSize) ;

#endif //_MSVR_API_H
