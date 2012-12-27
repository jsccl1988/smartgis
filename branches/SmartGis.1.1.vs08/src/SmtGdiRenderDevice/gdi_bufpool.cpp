#include "gdi_bufpool.h"

namespace Smt_Rd
{
	SmtBufPool::SmtBufPool(u_short nBufCount,u_short nSizePerBuf)
		: m_nBufCount(nBufCount)
		, m_nSizePerBuf(nSizePerBuf)
	{
		//判断参数是否合法
		assert(m_nBufCount);
		assert(m_nSizePerBuf);
		//分配内存
		m_pBuf = new char[m_nBufCount * m_nSizePerBuf];
		assert(m_pBuf);
		//分配内存池占用队列
		m_bBuf = new byte[m_nBufCount];
		assert(m_bBuf);
		//初始化内存池占用队列
		memset(m_bBuf,BufFree,m_nBufCount);

		m_ulPoolSize = m_nBufCount * m_nSizePerBuf*sizeof(char);
	}

	SmtBufPool::~SmtBufPool(void)
	{
		//释放缓存
		SMT_SAFE_DELETE_A(m_bBuf);

		//释放缓存占用队列
		SMT_SAFE_DELETE_A(m_pBuf);
	}

	//////////////////////////////////////////////////////////////////////////
	//请求分配一个buf
	//参数：无
	//返回值：分配成功，返回指向分配的buf指针，否则返回NULL
	char * SmtBufPool::NewBuf()
	{
		char * pRet = NULL;
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		for (int i = 0;i<m_nBufCount;++i)
		{
			if (m_bBuf[i] == BufFree)
			{
				m_bBuf[i] = BufUsed;
				pRet = m_pBuf + m_nSizePerBuf * i;
				break;
			}
		}
#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		return pRet;
	}
	//////////////////////////////////////////////////////////////////////////
	//释放一个buf
	//参数：buf的指针
	//返回值：无
	void SmtBufPool::FreeBuf(char * pBuf)
	{
		if (!pBuf)
			return;

#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		char * pFree = m_pBuf;
		for (int i = 0;i<m_nBufCount;++i)
		{
			if (pFree == pBuf)
			{
				m_bBuf[i] = BufFree;
				break;
			}
			pFree += m_nSizePerBuf;
		}
#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
	}
	//////////////////////////////////////////////////////////////////////////
	//释放所有buf
	//参数：无
	//返回值：无
	void SmtBufPool::FreeAllBuf()
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		memset(m_bBuf,BufFree,m_nBufCount);
#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
	}
}