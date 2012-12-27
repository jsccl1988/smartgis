#include "smt_mempoolmgr.h"
#include "smt_logmanager.h"

namespace Smt_Core
{
	SmtMemPoolMgr::SmtMemPoolMgr(unsigned int poolsize         /* = DEFAULT_POOLSIZE */,
		unsigned int poolunitsize     /* = DEFAULT_POOLUNITSIZE */,
		unsigned int blockunitsize    /* = DEFAULT_BLOCKUNITSIZE */)
	{
		m_nPoolSize         = poolsize;
		m_nPoolUnitSize     = poolunitsize;
		m_nBlockUnitSize    = blockunitsize;

		m_nTotalMemPool     = 0;
		m_nUsingMemPool     = 0;

		m_TimeTick          = GetTickCount();

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = NULL;
		if ((pLog = pLogMgr->GetLog(DEFAULT_LOG)) == NULL)
			pLog = pLogMgr->CreateLog(DEFAULT_LOG.c_str());

#ifdef _DEBUG
		m_nCode             = 0;
#endif
	}

	SmtMemPoolMgr::~SmtMemPoolMgr()
	{
		ClearMemPoolMgr();
	}
	
#ifdef _DEBUG
	void SmtMemPoolMgr::ProcessLog()
	{
		vector<SmtMemUnit*> ::iterator iterMU = m_vMemUnitPtr.begin();
		while(iterMU != m_vMemUnitPtr.end())
		{
			if (NULL != (*iterMU))
			{
				(*iterMU)->ProcessLog();
			}
			iterMU++;
		}
	}
#endif

	void SmtMemPoolMgr::ClearMemPoolMgr()
	{
		vector<SmtMemPool*> ::iterator iterMP = m_vMemPoolPtr.begin();
		while(iterMP != m_vMemPoolPtr.end())
		{
			SMT_SAFE_DELETE(*iterMP);
			iterMP++;
		}

		m_vMemPoolPtr.clear();
	
		////////////////////////////////////////////////////
		vector<SmtMemUnit*> ::iterator iterMU = m_vMemUnitPtr.begin();
		while(iterMU != m_vMemUnitPtr.end())
		{
			SMT_SAFE_DELETE(*iterMU);
			iterMU++;
		}

		m_vMemUnitPtr.clear();
	}

	byte* SmtMemPoolMgr::GetMemory(IDENTIFIER* pIdentifier, unsigned int size)
	{
		byte* pData;

		SmtMemUnit *pMemoryUnit = NULL;

		// First Check Whether the PartFile is in m_vMemUnitPtr or not
		vector<SmtMemUnit*> ::iterator iterMU = m_vMemUnitPtr.end();
		while(iterMU != m_vMemUnitPtr.begin())
		{
			if ((*iterMU)->GetIdentifier() == pIdentifier)
			{
				pMemoryUnit = (*iterMU);
				break;
			}
			iterMU--;
		}

		if (iterMU == m_vMemUnitPtr.begin())
		{
			if ((*iterMU)->GetIdentifier() == pIdentifier)
			{
				pMemoryUnit = (*iterMU);
			}
		}

		if (pMemoryUnit)
		{
			if ((pData = pMemoryUnit->GetMemory(size)) != NULL)
			{
				return pData;
			}
			else
			{
                SmtMemPool * pMemPool = GetMemPool(pMemoryUnit, m_nPoolUnitSize);
			    return pMemPool->GetMemory(size);
			}
		}
		else
		{
			pMemoryUnit = new SmtMemUnit(this, pIdentifier);
			m_vMemUnitPtr.push_back(pMemoryUnit);
			SmtMemPool * pMemPool = GetMemPool(pMemoryUnit, m_nPoolUnitSize);
			return pMemPool->GetMemory(size);
		}
	}

	void SmtMemPoolMgr::FreeMemory(IDENTIFIER* pIdentifier)
	{
		SmtMemUnit *pMemoryUnit = NULL;

		// First find the MemoryUnit of the given Identifier
		vector<SmtMemUnit*> ::iterator iterMU = m_vMemUnitPtr.end();
		while(iterMU != m_vMemUnitPtr.begin())
		{
			if ((*iterMU)->GetIdentifier() == pIdentifier)
			{
				pMemoryUnit = (*iterMU);
				break;
			}
			iterMU--;
		}

		if (iterMU == m_vMemUnitPtr.begin())
		{
			if ((*iterMU)->GetIdentifier() == pIdentifier)
			{
				pMemoryUnit = (*iterMU);
			}
		}
        // Free the Memory Pool and Remove them
		if (pMemoryUnit)
		{
			// Free All Memory Pool
			m_nUsingMemPool -= pMemoryUnit->FreeAllMemPool();
		}
	}

	SmtMemPool* SmtMemPoolMgr::GetMemPool(SmtMemUnit *pMemoryUnit, int poolsize)
	{
		// VC-linhai[2007-08-06]: warning C4701: 局部变量“pMemPool”可能尚未初始化即被使用
		// 对 pMemPool 初始化为NULL
		SmtMemPool * pMemPool = NULL;
		vector<SmtMemPool*> ::iterator iterMP = m_vMemPoolPtr.begin();
		while(iterMP != m_vMemPoolPtr.end())
		{
			if ((*iterMP)->IsFree())
			{
				pMemPool = (*iterMP);
				pMemoryUnit->AddTail(pMemPool);
				m_nUsingMemPool++;

				return pMemPool;
			}
			
			iterMP++;
		}

		try
		{
#ifdef _DEBUG
			pMemPool = new SmtMemPool(++m_nCode, poolsize, m_nBlockUnitSize);
#else
			pMemPool = new SmtMemPool(poolsize, m_nBlockUnitSize);
#endif
			m_vMemPoolPtr.push_back(pMemPool);
			m_nTotalMemPool++;

			pMemoryUnit->AddTail(pMemPool);
			m_nUsingMemPool++;

			return pMemPool;
		}
		catch(...)
		{
			// 当 pMenPool 初始化失败，前面对 pMenPool 的初始化便
			SMT_SAFE_DELETE(pMemPool);
		    return NULL;
		}
	}

	unsigned int SmtMemPoolMgr::GetCurUsedSize(IDENTIFIER* pIdentifier) const
	{
		SmtMemUnit *pMemoryUnit = NULL;

		// First Check Whether the PartFile is in m_vMemUnitPtr or not
		vector<SmtMemUnit*>::const_iterator iterMU = m_vMemUnitPtr.end();
		while(iterMU != m_vMemUnitPtr.begin())
		{
			if ((*iterMU)->GetIdentifier() == pIdentifier)
			{
				pMemoryUnit = (*iterMU);
				break;
			}
			iterMU--;
		}
		
		if (iterMU == m_vMemUnitPtr.begin())
		{
			if ((*iterMU)->GetIdentifier() == pIdentifier)
			{
				pMemoryUnit = (*iterMU);
			}
		}

		if (pMemoryUnit)
		{
			return pMemoryUnit->GetCurUsedSize();
		}
		else
		{
			return 0;
		}
	}

	unsigned int SmtMemPoolMgr::GetMemPoolUnitSize() const
	{
	    return m_nPoolUnitSize;
	}

	void SmtMemPoolMgr::CheckToRelease()
	{
		if ((m_nTotalMemPool - m_nUsingMemPool) > 5)
		{
			if ((GetTickCount() - m_TimeTick) > MIN2MS(30))
			{
				vector<SmtMemPool*> ::iterator iterMP = m_vMemPoolPtr.begin();
				while(iterMP != m_vMemPoolPtr.end())
				{
					if ((*iterMP)->IsFree())
					{
						SMT_SAFE_DELETE(*iterMP);
						m_nTotalMemPool--;
						m_vMemPoolPtr.erase(iterMP++);
					}
					else
						iterMP++;
				}
			}
		}
		else
		{
			m_TimeTick = GetTickCount();
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////

	SmtMemUnit::SmtMemUnit(SmtMemPoolMgr * pMemPoolMgr, IDENTIFIER* pIdentifier)
	{
		m_pMemPoolMgr  = pMemPoolMgr; 
		m_pIdentifier  = pIdentifier;
	}

	SmtMemUnit:: ~SmtMemUnit()
	{
	}

	byte* SmtMemUnit::GetMemory(unsigned int size)
	{
		byte* pData;

		if (m_vMemPoolPtr.size() < 1)
		{
			return NULL; // The Pool List Should not be Empty
		}

		vector<SmtMemPool*> ::iterator iterMP = m_vMemPoolPtr.end();
		iterMP--;
		if ((pData =(*iterMP)->GetMemory(size)) != NULL)
		{
			return pData;
		}
		else
		{
			return NULL;
		}
	}

	unsigned int SmtMemUnit::FreeAllMemPool()
	{
		unsigned int number = 0;
		for (int i = m_vMemPoolPtr.size()-1;i > 0; i++)
		{
			m_vMemPoolPtr[i]->FreeMemory();
		}

		number = m_vMemPoolPtr.size();

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = NULL;
		if ((pLog = pLogMgr->GetLog(DEFAULT_LOG)) != NULL)
		{
			pLog->LogMessage("%d Free Memory, PoolList Size is : %d\n", m_pIdentifier, m_vMemPoolPtr.size());
		}
		
		m_vMemPoolPtr.clear();

		return number;
	}

	unsigned int SmtMemUnit::GetCurUsedSize() 
	{
		int CurUsedSize = 0;

		if (m_vMemPoolPtr.size() > 0)
		{
			CurUsedSize  = (m_vMemPoolPtr.size() - 1) * m_pMemPoolMgr->GetMemPoolUnitSize();
			CurUsedSize += m_vMemPoolPtr[(m_vMemPoolPtr.size() - 1)]->GetCurUsedSize();
			return CurUsedSize;
		}
		else 
		{
			return 0;
		}
	}

	IDENTIFIER * SmtMemUnit::GetIdentifier() const
	{
		return m_pIdentifier;
	}

	void SmtMemUnit::AddTail(SmtMemPool * pMemPool)
	{
	    m_vMemPoolPtr.push_back(pMemPool);
	}

#ifdef _DEBUG
	void SmtMemUnit::ProcessLog()
	{
		vector<SmtMemPool*> ::iterator iterMP = m_vMemPoolPtr.begin();
		TRACE("PartFile %d : MemPool Code : ", m_pIdentifier);

		while(iterMP != m_vMemPoolPtr.end())
		{
			TRACE(" %d", (*iterMP)->m_nCode);

			iterMP++;
		}
		
		TRACE("\n");
	}
#endif
};