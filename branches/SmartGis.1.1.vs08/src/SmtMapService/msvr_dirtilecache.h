#ifndef _MSVR_DIR_TITLECACHE_H
#define _MSVR_DIR_TITLECACHE_H

#include "msvr_mapservice.h"

namespace Smt_MapService
{
	//service part
	class SMT_EXPORT_CLASS SmtDirTileCache:public SmtTileCache
	{
	public:
		SmtDirTileCache();
		virtual ~SmtDirTileCache();

	public:
		virtual long				Init(void);
		virtual long				Create(void);
		virtual long				Destroy(void);

		virtual long				Connect(std::string strCacheFileName);

		virtual	bool				Open(void);
		virtual	bool				Close(void);

	protected:
		SmtDirTileCache(const SmtDirTileCache& other) {;}
		SmtDirTileCache &			operator=(const SmtDirTileCache& other) {return *this;}

	public:
		virtual bool				IsTileExist(long lCol,long lRow,long lZoom);

		virtual long				AppendTile(const char *pTileBuf,long lTileBufSize,long lCol,long lRow,long lZoom);
		virtual long				GetTile(char *&pTileBuf,long &lTileBufSize,long lCol,long lRow,long lZoom);
		virtual long				ReleaseTile(char *&pTileBuf);
	};
}
#endif //_MSVR_DIR_TITLECACHE_H