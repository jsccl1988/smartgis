#ifndef _MSVR_SMFTITLE_H
#define _MSVR_SMFTITLE_H

#include "smt_bas_struct.h"

namespace Smt_MapService
{
	//0000 00000000000000 00000000000000
	//zoom col			  row
	//目前最多支持		  14级

#define		MSVR_SVR_NAME_LENGTH		MAX_NAME_LENGTH
#define		MSVR_SVR_VERSION_LENGTH		20
#define		MSVR_SVR_PROVIDER_LENGTH	MAX_NAME_LENGTH
#define		MSVR_SVR_EXT				50
#define		MSVR_SVR_VERSION			"MSVR 1.0"

	struct MSvrTIHead
	{
		char	szHead[4];				//TI(Tile Index)

		char	szSvrName[MSVR_SVR_NAME_LENGTH];
		char	szSvrVersion[MSVR_SVR_VERSION_LENGTH];
		char	szSvrProvider[MSVR_SVR_PROVIDER_LENGTH];

		long	lMinZoom;
		long	lMaxZoom;
		long	lImageCode;
		long	lTileWidth;
		long	lTileHeight;

		char	szExt[MSVR_SVR_EXT];	//扩展字段

		//
		ulong	ulHeadBegin;
		ulong	ulZoomFisrtNodeInfoBegin;
		ulong	ulZoomFisrtNodeInfoSize;
		ulong	ulTINodeBegin;

		MSvrTIHead()
		{
			strcpy_s(szSvrName,MSVR_SVR_NAME_LENGTH,"");
			strcpy_s(szSvrVersion,MSVR_SVR_VERSION_LENGTH,MSVR_SVR_VERSION);
			strcpy_s(szSvrProvider,MSVR_SVR_NAME_LENGTH,"");

			strcpy_s(szHead,4,"TIO");
			memset(szExt,0,MSVR_SVR_EXT);

			lMinZoom = 0;
			lMaxZoom = 0;
			lImageCode = -1;
			lTileWidth = 0;
			lTileHeight = 0;

			ulHeadBegin = 0;
			ulZoomFisrtNodeInfoBegin = ulHeadBegin + sizeof(MSvrTIHead);
			ulZoomFisrtNodeInfoSize  = 0;
			ulTINodeBegin = ulZoomFisrtNodeInfoBegin + ulZoomFisrtNodeInfoSize;
		}
	};

	struct MSvrTINode 
	{
		long		lZoom;
		long		lCol;
		long		lRow;
		ulong		ulDBlockBegin;
		ulong		ulDBlockSize;

		MSvrTINode():lZoom(-1)
			,lCol(-1),lRow(-1)
			,ulDBlockBegin(0),ulDBlockSize(0)
		{

		}
	};

	//////////////////////////////////////////////////////////////////////////
	struct MSvrTCHead
	{
		char	szHead[4];				//TC(Tile Cache)
		char	szExt[MSVR_SVR_EXT];	//扩展字段

		ulong	ulHeadBegin;
		ulong	ulDataBegin;
		ulong	ulDataSize;

		MSvrTCHead():ulHeadBegin(0)
			,ulDataBegin(0)
			,ulDataSize(0)
		{
			strcpy_s(szHead,4,"TCO");
			memset(szExt,0,MSVR_SVR_EXT);
		}
	};
}
#endif //_MSVR_SMFTITLE_H