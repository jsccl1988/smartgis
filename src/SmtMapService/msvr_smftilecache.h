#ifndef _MSVR_SMF_TITLECACHE_H
#define _MSVR_SMF_TITLECACHE_H

#include "msvr_mapservice.h"
#include "msvr_smftile.h"
#include "smt_cslock.h"
#include "smt_srwlock.h"

namespace Smt_MapService
{
	/*
		//TI File struct
		MSvrTIHead................sizeof(MSvrTIHead)
		ZoomFisrtNodeLoc..........sizeof(ulong)*(MSvrTIHead.ulMaxZoom - MSvrTIHead.ulMinZoom+1)
		TINodes...................
	*/

	/*
		//TC File struct
		MSvrTCHead................sizeof(MSvrTCHead)
		TBlock....................
	*/

	//service part
	class SMT_EXPORT_CLASS SmtSmfTileCache:public SmtTileCache
	{
	public:
		SmtSmfTileCache();
		virtual ~SmtSmfTileCache();

	public:
		virtual long				Init(void);
		virtual long				Create(void);
		virtual long				Destroy(void);

		virtual long				Connect(std::string strCacheFileName);

		virtual	bool				Open(void);
		virtual	bool				Close(void);

	protected:
		SmtSmfTileCache(const SmtSmfTileCache& other) {;}
		SmtSmfTileCache &			operator=(const SmtSmfTileCache& other) {return *this;}

	public:
		virtual bool				IsTileExist(long lCol,long lRow,long lZoom);

		virtual long				AppendTile(const char *pTileBuf,long lTileBufSize,long lCol,long lRow,long lZoom);
		virtual long				GetTile(char *&pTileBuf,long &lTileBufSize,long lCol,long lRow,long lZoom);
		virtual long				ReleaseTile(char *&pTileBuf);

	protected:
		long						SetTIHead(void);
		long						SetTCHead(void);

		long						WriteTIHead(void);
		long						WriteTCHead(void);

		long						WriteTIZoomFisrtNodeLocs(void);
		long						WriteTIZoomFisrtNodeLoc(long lZoom,ulong ulLoc);
		long						WriteTINodes(void);
		long						LocateTINode(long lCol,long lRow,long lZoom,ulong &ulTILocation);

		long						WriteTI(long lCol,long lRow,long lZoom,const MSvrTINode &tiNode);
		long						WriteTC(const char *pTileBuf,long lTileBufSize,long lCol,long lRow,long lZoom,MSvrTINode &tiNode);

		long						ReadTIHead(void);
		long						ReadTCHead(void);

		long						ReadTIZoomFisrtNodeLoc(long lZoom,ulong &ulLoc);

		long						ReadTI(long lCol,long lRow,long lZoom,MSvrTINode &tiNode);
		long						ReadTC(char *&pTileBuf,long &lTileBufSize,long lCol,long lRow,long lZoom,const MSvrTINode &tiNode);

	protected:
		Smt_Core::SmtSRWLock		m_srwlock;
		std::fstream				m_tifile;
		std::fstream				m_tcfile;

		MSvrTIHead					m_tiHead;
		MSvrTCHead					m_tcHead;
	};
}
#endif //_MSVR_DIR_TITLECACHE_H