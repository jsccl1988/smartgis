#include "msvr_smftilecache.h"
#include "ximage.h"
#include "smt_api.h"

namespace Smt_MapService
{
	//////////////////////////////////////////////////////////////////////////
	//SmtSmfTileCache
	//////////////////////////////////////////////////////////////////////////
	SmtSmfTileCache::SmtSmfTileCache()
	{

	}

	SmtSmfTileCache::~SmtSmfTileCache()
	{
		Destroy();
	}

	long SmtSmfTileCache::Init()
	{
		string strAllPath = m_strCacheDir+m_pMSvr->GetName();
		string strTIFile = strAllPath+".ti";
		string strTCFile = strAllPath+".tc";

		//可读 可写 二进制
		locale loc = locale::global(locale(".936"));
		m_tifile.open(strTIFile.c_str(),ios::out|ios::binary|ios::app);
		m_tcfile.open(strTCFile.c_str(),ios::out|ios::binary|ios::app);
		locale::global(std::locale(loc));

		if (m_tifile.is_open() && m_tcfile.is_open())
		{
			m_tifile.close();
			m_tcfile.close();

			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtSmfTileCache::Create()
	{
		if (SMT_ERR_NONE != Destroy())
			return SMT_ERR_FAILURE;

		if (!Open())
			return SMT_ERR_FAILURE;
	
		if (SMT_ERR_NONE == SetTIHead() &&
			SMT_ERR_NONE == SetTCHead() &&
			SMT_ERR_NONE == WriteTIHead() &&
			SMT_ERR_NONE == WriteTCHead() &&
			SMT_ERR_NONE == WriteTIZoomFisrtNodeLocs() &&
			SMT_ERR_NONE == WriteTINodes())
		{
			Close();

			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtSmfTileCache::Connect(std::string strCacheFileName)
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

		if (!IsOpen())
			Open();

		long lTWidth = 0,lTHeight = 0;
		m_pMSvr->GetTileSize(lTWidth,lTHeight);

		if (SMT_ERR_NONE == ReadTIHead() &&
			SMT_ERR_NONE == ReadTCHead())
		{
			if (strcmp(m_tiHead.szSvrName,m_pMSvr->GetName().c_str()) != 0 ||
				strcmp(m_tiHead.szSvrProvider,m_pMSvr->GetProvider().c_str()) != 0 ||
				m_tiHead.lTileWidth != lTWidth ||m_tiHead.lTileHeight != lTHeight ||
				m_tiHead.lMinZoom > m_pMSvr->GetZoomMin() ||m_tiHead.lMaxZoom < m_pMSvr->GetZoomMax())
			{
				Close();

				return SMT_ERR_FAILURE;
			}

			Close();

			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtSmfTileCache::Destroy()
	{
		return SMT_ERR_NONE;
	}

	bool SmtSmfTileCache::Open(void)
	{
		if (m_bOpen)
			Close();

		m_bOpen = true;

		string strAllPath = m_strCacheDir+m_pMSvr->GetName();
		string strTIFile = strAllPath+".ti";
		string strTCFile = strAllPath+".tc";

		//可读 可写 二进制
		locale loc = locale::global(locale(".936"));
		m_tifile.open(strTIFile.c_str(),ios::in|ios::out|ios::binary);
		m_tcfile.open(strTCFile.c_str(),ios::in|ios::out|ios::binary);
		locale::global(std::locale(loc));

		m_bOpen &= m_tifile.is_open();
		m_bOpen &= m_tcfile.is_open();

		if (m_bOpen)
		{
			m_tifile.clear();
			m_tcfile.clear();
		}
		
		return m_bOpen;
	}

	bool SmtSmfTileCache::Close(void)
	{
		/*m_tifile.clear();
		m_tcfile.clear();*/

		m_tifile.close();
		m_tcfile.close();

		m_bOpen = false;

		return !m_bOpen;
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtSmfTileCache::AppendTile(const char *pTileBuf,long lTileBufSize,long lCol,long lRow,long lZoom)
	{
		if (NULL == pTileBuf || 0 == lTileBufSize)
			return SMT_ERR_INVALID_PARAM;
		if (!IsOpen())
			Open();

		if (IsTileExist(lCol,lRow,lZoom))
			return SMT_ERR_NONE;
		 
		MSvrTINode tiNode;

		m_srwlock.LockExclusive();

		if (SMT_ERR_NONE == WriteTC(pTileBuf,lTileBufSize,lCol,lRow,lZoom,tiNode) &&
			SMT_ERR_NONE == WriteTI(lCol,lRow,lZoom,tiNode))
		{
			m_srwlock.UnlockExclusive();

			return SMT_ERR_NONE;
		}

		m_srwlock.UnlockExclusive();
		
		return SMT_ERR_FAILURE;
	}

	long SmtSmfTileCache::GetTile(char *&pTileBuf,long &lTileBufSize,long lCol,long lRow,long lZoom)
	{
		if (!IsOpen())
			Open();

		if (!IsTileExist(lCol,lRow,lZoom))
			return SMT_ERR_FAILURE;

		MSvrTINode tiNode;

		m_srwlock.LockShared();

		if (SMT_ERR_NONE == ReadTI(lCol,lRow,lZoom,tiNode) &&
			SMT_ERR_NONE == ReadTC(pTileBuf,lTileBufSize,lCol,lRow,lZoom,tiNode))
		{
			m_srwlock.UnlockShared();

			return SMT_ERR_NONE;
		}

		m_srwlock.UnlockShared();

		return SMT_ERR_FAILURE;
	}

	long SmtSmfTileCache::ReleaseTile(char *&pTileBuf)
	{
		SMT_SAFE_DELETE_A(pTileBuf);

		return SMT_ERR_NONE;
	}

	bool SmtSmfTileCache::IsTileExist(long lCol,long lRow,long lZoom)
	{
		MSvrTINode tiNode;
		if (SMT_ERR_NONE == ReadTI(lCol,lRow,lZoom,tiNode))
		{
			return (tiNode.ulDBlockSize != 0);
		}
		
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtSmfTileCache::SetTIHead(void)
	{
		strcpy_s(m_tiHead.szHead,4,"TI");
		strcpy_s(m_tiHead.szSvrName,MSVR_SVR_NAME_LENGTH,m_pMSvr->GetName().c_str());
		strcpy_s(m_tiHead.szSvrVersion,MSVR_SVR_VERSION_LENGTH,MSVR_SVR_VERSION);
		strcpy_s(m_tiHead.szSvrProvider,MSVR_SVR_NAME_LENGTH,m_pMSvr->GetProvider().c_str());

		m_tiHead.lMinZoom = m_pMSvr->GetZoomMin();
		m_tiHead.lMaxZoom = m_pMSvr->GetZoomMax();
		m_tiHead.lImageCode = m_pMSvr->GetImageCode();
		m_pMSvr->GetTileSize(m_tiHead.lTileWidth,m_tiHead.lTileHeight);

		memset(m_tiHead.szExt,0,MSVR_SVR_EXT);

		m_tiHead.ulHeadBegin = 0;
		m_tiHead.ulZoomFisrtNodeInfoBegin = m_tiHead.ulHeadBegin + sizeof(MSvrTIHead);
		m_tiHead.ulZoomFisrtNodeInfoSize  = sizeof(ulong)*(m_pMSvr->GetZoomMax()-m_pMSvr->GetZoomMin()+1);
		m_tiHead.ulTINodeBegin = m_tiHead.ulZoomFisrtNodeInfoBegin + m_tiHead.ulZoomFisrtNodeInfoSize;

		return SMT_ERR_NONE;
	}

	long SmtSmfTileCache::SetTCHead(void)
	{
		strcpy_s(m_tcHead.szHead,4,"TC");

		memset(m_tcHead.szExt,0,MSVR_SVR_EXT);

		m_tcHead.ulHeadBegin = 0;
		m_tcHead.ulDataBegin = m_tcHead.ulHeadBegin+sizeof(MSvrTCHead);
		m_tcHead.ulDataSize = 0;

		return SMT_ERR_NONE;
	}

	long SmtSmfTileCache::WriteTIHead(void)
	{
		ulong ulPos = 0;

		m_srwlock.LockExclusive();

		m_tifile.seekp(m_tiHead.ulHeadBegin, ios::beg );
		ulPos = m_tifile.tellp();

		m_tifile.write((char *)(&m_tiHead),sizeof(MSvrTIHead));
		ulPos = m_tifile.tellp();

		m_srwlock.UnlockExclusive();

		return SMT_ERR_NONE;
	}

	long SmtSmfTileCache::WriteTCHead(void)
	{
		m_srwlock.LockExclusive();

		m_tcfile.seekp(m_tcHead.ulHeadBegin, ios::beg ); 
		m_tcfile.write((char *)(&m_tcHead),sizeof(MSvrTCHead));

		m_srwlock.UnlockExclusive();
		
		return SMT_ERR_NONE;
	}

	long SmtSmfTileCache::WriteTIZoomFisrtNodeLocs(void)
	{
		long lMinZoom = m_pMSvr->GetZoomMin();
		long lMaxZoom = m_pMSvr->GetZoomMax();

		long lMinCol = -1,lMinRow = -1,lMaxCol = -1,lMaxRow = -1;
		Envelope envMap;

		m_pMSvr->GetEnvelope(envMap);

		ulong ulLoc = m_tiHead.ulTINodeBegin;
		ulong ulPos = 0;

		m_srwlock.LockExclusive();

		m_tcfile.seekp(m_tiHead.ulZoomFisrtNodeInfoBegin, ios::beg ); 
		for (int iZoom = lMinZoom;iZoom <= lMaxZoom; iZoom++)
		{
			m_pMSvr->GetTileRange(lMinCol,lMinRow,lMaxCol,lMaxRow ,envMap,iZoom);

			ulPos = m_tifile.tellp();
			m_tifile.write((char *)(&ulLoc),sizeof(ulong));
			ulPos = m_tifile.tellp();

			for (int iCol = lMinCol; iCol <= lMaxCol;iCol++)
			{
				for(int jRow = lMinRow;jRow <= lMaxRow;jRow++)
				{
					ulLoc += sizeof(MSvrTINode);
				}
			}
		}	

		ulPos = m_tifile.tellp();
		m_srwlock.UnlockExclusive();

		return SMT_ERR_NONE;
	}

	long SmtSmfTileCache::WriteTIZoomFisrtNodeLoc(long lZoom,ulong ulLoc)
	{
		ulong ulPos = 0;

		m_srwlock.LockExclusive();

		m_tifile.seekp(m_tiHead.ulZoomFisrtNodeInfoBegin + (lZoom - m_tiHead.lMinZoom)*sizeof(ulong), ios::beg ); 
		ulPos = m_tifile.tellp();
		m_tifile.write((char *)(&ulLoc),sizeof(ulong));
		ulPos = m_tifile.tellp();

		m_srwlock.UnlockExclusive();
		
		return SMT_ERR_NONE;
	}

	long SmtSmfTileCache::WriteTINodes(void)
	{
		MSvrTINode tiNodes;

		long lMinZoom = m_pMSvr->GetZoomMin();
		long lMaxZoom = m_pMSvr->GetZoomMax();

		long lMinCol = -1,lMinRow = -1,lMaxCol = -1,lMaxRow = -1;
		Envelope envMap;

		m_pMSvr->GetEnvelope(envMap);

		ulong ulLoc = m_tiHead.ulTINodeBegin;
		ulong ulPos = 0;
		
		m_srwlock.LockExclusive();

		m_tcfile.seekp(m_tiHead.ulTINodeBegin, ios::beg );
		for (int iZoom = lMinZoom;iZoom <= lMaxZoom; iZoom++)
		{
			m_pMSvr->GetTileRange(lMinCol,lMinRow,lMaxCol,lMaxRow ,envMap,iZoom);

		/*	WriteTIZoomFisrtNodeLoc(iZoom,ulLoc);

			m_tcfile.seekp(ulLoc, ios::beg ); */

			for (int iCol = lMinCol; iCol <= lMaxCol;iCol++)
			{
				for(int jRow = lMinRow;jRow <= lMaxRow;jRow++)
				{
					tiNodes.lCol = iCol;
					tiNodes.lRow = jRow;
					tiNodes.lZoom = iZoom;
					tiNodes.ulDBlockBegin = 0;
					tiNodes.ulDBlockSize = 0;

					ulPos = m_tifile.tellp();
					m_tifile.write((char *)(&tiNodes),sizeof(MSvrTINode));
					ulPos = m_tifile.tellp();

					ulLoc += sizeof(MSvrTINode);
				}
			}
		}

		m_srwlock.UnlockExclusive();

		return SMT_ERR_NONE;
	}

	long SmtSmfTileCache::LocateTINode(long lCol,long lRow,long lZoom,ulong &ulTILocation)
	{
		ulong ulMinZoom = m_pMSvr->GetZoomMin();
		ulong ulMaxZoom = m_pMSvr->GetZoomMax();

		long lMinCol = -1,lMinRow = -1,lMaxCol = -1,lMaxRow = -1;
		Envelope envMap;

		m_pMSvr->GetEnvelope(envMap);

		m_pMSvr->GetTileRange(lMinCol,lMinRow,lMaxCol,lMaxRow ,envMap,lZoom);

		if (lCol < lMinCol || lCol > lMaxCol ||
			lRow < lMinRow || lRow > lMaxRow)
			return SMT_ERR_FAILURE;

		ReadTIZoomFisrtNodeLoc(lZoom,ulTILocation);

		for (int iCol = lMinCol; iCol <= lMaxCol;iCol++)
		{
			for(int jRow = lMinRow;jRow <= lMaxRow;jRow++)
			{
				ulTILocation += sizeof(MSvrTINode);

				if (lCol == iCol && lRow ==  jRow)
				{
					return SMT_ERR_NONE;
				}
			}
		}

		return SMT_ERR_FAILURE;
	}

	long SmtSmfTileCache::WriteTI(long lCol,long lRow,long lZoom,const MSvrTINode &tiNode)
	{
		ulong ulTILocation = 0;
		ulong ulPos = 0;

		if (SMT_ERR_NONE != LocateTINode(lCol,lRow,lZoom,ulTILocation))
			return SMT_ERR_FAILURE;
		 
		m_srwlock.LockExclusive();

		m_tifile.seekp(ulTILocation,ios::beg);
		ulPos = m_tifile.tellp();

		m_tifile.write((char *)(&tiNode),sizeof(MSvrTINode));
		ulPos = m_tifile.tellp();

		m_srwlock.UnlockExclusive();

		return SMT_ERR_NONE;
	}

	long SmtSmfTileCache::WriteTC(const char *pTileBuf,long lTileBufSize,long lCol,long lRow,long lZoom,MSvrTINode &tiNode)
	{
		m_srwlock.LockExclusive();

		m_tcfile.seekp(m_tcHead.ulDataBegin+m_tcHead.ulDataSize, ios::beg); 
		m_tcfile.write(pTileBuf,lTileBufSize);

		m_srwlock.UnlockExclusive();
		
		tiNode.lCol = lCol;
		tiNode.lRow = lRow;
		tiNode.lZoom = lZoom;
		tiNode.ulDBlockBegin = m_tcHead.ulDataBegin + m_tcHead.ulDataSize;
		tiNode.ulDBlockSize  = lTileBufSize;

		m_tcHead.ulDataSize += lTileBufSize;

		return WriteTCHead();
	}

	long SmtSmfTileCache::ReadTIHead(void)
	{
		ulong ulPos = 0;

		m_srwlock.LockShared();

		ulPos = m_tifile.tellg();
		m_tifile.seekg(m_tiHead.ulHeadBegin, ios::beg ); 
		m_tifile.read((char *)(&m_tiHead),sizeof(MSvrTIHead));
		ulPos = m_tifile.tellg();

		m_srwlock.UnlockShared();

		return SMT_ERR_NONE;
	}

	long SmtSmfTileCache::ReadTCHead(void)
	{
		ulong ulPos = 0;

		m_srwlock.LockShared();

		ulPos = m_tifile.tellg();
		m_tcfile.seekg(m_tcHead.ulHeadBegin, ios::beg); 
		m_tcfile.read((char *)(&m_tcHead),sizeof(MSvrTCHead));
		ulPos = m_tifile.tellg();

		m_srwlock.UnlockShared();

		return SMT_ERR_NONE;
	}

	long SmtSmfTileCache::ReadTIZoomFisrtNodeLoc(long lZoom,ulong &ulLoc)
	{
		ulong ulPos = 0;

		m_srwlock.LockShared();

		m_tifile.seekg(m_tiHead.ulZoomFisrtNodeInfoBegin + (lZoom - m_tiHead.lMinZoom)*sizeof(ulong), ios::beg ); 
		m_tifile.read((char *)(&ulLoc),sizeof(ulong));
		ulPos = m_tifile.tellg();

		m_srwlock.UnlockShared();

		return SMT_ERR_NONE;
	}

	long SmtSmfTileCache::ReadTI(long lCol,long lRow,long lZoom,MSvrTINode &tiNode)
	{
		ulong ulPos = 0;
		ulong ulTILocation = 0;

		if (SMT_ERR_NONE != LocateTINode(lCol,lRow,lZoom,ulTILocation))
			return SMT_ERR_FAILURE;

		m_srwlock.LockShared();

		m_tifile.seekg(ulTILocation,ios::beg);
		m_tifile.read((char *)(&tiNode),sizeof(MSvrTINode));
		ulPos = m_tifile.tellg();

		m_srwlock.UnlockShared();

		return SMT_ERR_NONE;
	}

	long SmtSmfTileCache::ReadTC(char *&pTileBuf,long &lTileBufSize,long lCol,long lRow,long lZoom,const MSvrTINode &tiNode)
	{
		lTileBufSize = tiNode.ulDBlockSize;
		pTileBuf = new char[tiNode.ulDBlockSize];

		m_srwlock.LockShared();

		m_tcfile.seekg(tiNode.ulDBlockBegin, ios::beg); 
		m_tcfile.read(pTileBuf,lTileBufSize);

		m_srwlock.UnlockShared();

		return SMT_ERR_NONE;
	}	
}