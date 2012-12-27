#include "msvr_dirtilecache.h"
#include "ximage.h"
#include "smt_api.h"

namespace Smt_MapService
{
	//////////////////////////////////////////////////////////////////////////
	//SmtDirTileCache
	//////////////////////////////////////////////////////////////////////////
	SmtDirTileCache::SmtDirTileCache()
	{

	}

	SmtDirTileCache::~SmtDirTileCache()
	{
		Destroy();
	}

	long SmtDirTileCache::Init()
	{
		return SMT_ERR_NONE;
	}

	long SmtDirTileCache::Create()
	{
		if (SMT_ERR_NONE != Destroy())
			return SMT_ERR_FAILURE;
		 
		if (NULL == m_pMSvr)
			return SMT_ERR_FAILURE;

		CreateAllPathDirectory(m_strCacheDir);

		return SMT_ERR_NONE;
	}

	long SmtDirTileCache::Connect(std::string strCacheFileName)
	{
		if (SMT_ERR_NONE != Destroy())
			return SMT_ERR_FAILURE;

		if (NULL == m_pMSvr)
			return SMT_ERR_FAILURE;

		char path[_MAX_PATH];
		char fileName[_MAX_PATH];
		char title[_MAX_PATH];
		char ext[_MAX_PATH];

		SplitFileName(strCacheFileName.c_str(),path,fileName,title,ext);
	
		m_strCacheDir = path;

		return SMT_ERR_NONE;
	}

	long SmtDirTileCache::Destroy()
	{
		return SMT_ERR_NONE;
	}

	bool SmtDirTileCache::Open(void)
	{
		m_bOpen = true;
		return m_bOpen;
	}

	bool SmtDirTileCache::Close(void)
	{
		m_bOpen = false;
		return !m_bOpen;
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtDirTileCache::AppendTile(const char *pTileBuf,long lTileBufSize,long lCol,long lRow,long lZoom)
	{
		if (NULL == pTileBuf || 0 == lTileBufSize)
			return SMT_ERR_INVALID_PARAM;

		char    szTileFile[MAX_FILE_PATH];
		sprintf_s(szTileFile,MAX_FILE_PATH,"%s%d_%d_%d%s",m_strCacheDir.c_str(),lZoom,lCol,lRow,".jpg");

		CxImage titleImage((BYTE*)pTileBuf,lTileBufSize,m_pMSvr->GetImageCode());
		titleImage.Save(szTileFile,m_pMSvr->GetImageCode());

		return SMT_ERR_NONE;
	}

	long SmtDirTileCache::GetTile(char *&pTileBuf,long &lTileBufSize,long lCol,long lRow,long lZoom)
	{
		char    szTileFile[MAX_FILE_PATH];
		sprintf_s(szTileFile,MAX_FILE_PATH,"%s%d_%d_%d%s",m_strCacheDir.c_str(),lZoom,lCol,lRow,".jpg");

		CxImage titleImage;
		titleImage.Load(szTileFile,GetImageTypeByFileExt(szTileFile));

		if (!titleImage.IsValid())
			return SMT_ERR_FAILURE;

		BYTE *pBuf = NULL;
		long lBufSize = 0;

		if (!titleImage.Encode(pBuf,lBufSize,m_pMSvr->GetImageCode()))
			return SMT_ERR_FAILURE;

		pTileBuf = (char *)pBuf;
		lTileBufSize = lBufSize;

		return SMT_ERR_NONE;
	}

	long SmtDirTileCache::ReleaseTile(char *&pTileBuf)
	{
		SMT_SAFE_DELETE_A(pTileBuf);

		return SMT_ERR_NONE;
	}

	bool SmtDirTileCache::IsTileExist(long lCol,long lRow,long lZoom)
	{
		char    szTileFile[MAX_FILE_PATH];
		sprintf_s(szTileFile,MAX_FILE_PATH,"%s%d_%d_%d%s",m_strCacheDir.c_str(),lZoom,lCol,lRow,".jpg");

		return (NULL == fopen(szTileFile,"rb"));
	}
}