#include "base/core/mempoolmgr.h"
#include "base/core/logmanager.h"

namespace base
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

		SmtLogManager * pLogMgr = SmtLogManager::get_singleton_ptr();
		SmtLog *pLog = NULL;
		if ((pLog = pLogMgr->get_log(DEFAULT_LOG)) == NULL)
			pLog = pLogMgr->create_log(DEFAULT_LOG.c_str());

#ifdef _DEBUG
		m_nCode             = 0;
#endif
	}

	SmtMemPoolMgr::~SmtMemPoolMgr()
	{
		clear_mem_pool_mgr();
	}
	
#ifdef _DEBUG
	void SmtMemPoolMgr::process_log()
	{
		vector<SmtMemUnit*> ::iterator iterMU = m_vMemUnitPtr.begin();
		while(iterMU != m_vMemUnitPtr.end())
		{
			if (NULL != (*iterMU))
			{
				(*iterMU)->process_log();
			}
			iterMU++;
		}
	}
#endif

	void SmtMemPoolMgr::clear_mem_pool_mgr()
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

	byte* SmtMemPoolMgr::get_memory(IDENTIFIER* pIdentifier, unsigned int size)
	{
		byte* pData;

		SmtMemUnit *pMemoryUnit = NULL;

		// First Check Whether the PartFile is in m_vMemUnitPtr or not
		vector<SmtMemUnit*> ::iterator iterMU = m_vMemUnitPtr.end();
		while(iterMU != m_vMemUnitPtr.begin())
		{
			if ((*iterMU)->get_identifier() == pIdentifier)
			{
				pMemoryUnit = (*iterMU);
				break;
			}
			iterMU--;
		}

		if (iterMU == m_vMemUnitPtr.begin())
		{
			if ((*iterMU)->get_identifier() == pIdentifier)
			{
				pMemoryUnit = (*iterMU);
			}
		}

		if (pMemoryUnit)
		{
			if ((pData = pMemoryUnit->get_memory(size)) != NULL)
			{
				return pData;
			}
			else
			{
                SmtMemPool * pMemPool = get_mem_pool(pMemoryUnit, m_nPoolUnitSize);
			    return pMemPool->get_memory(size);
			}
		}
		else
		{
			pMemoryUnit = new SmtMemUnit(this, pIdentifier);
			m_vMemUnitPtr.push_back(pMemoryUnit);
			SmtMemPool * pMemPool = get_mem_pool(pMemoryUnit, m_nPoolUnitSize);
			return pMemPool->get_memory(size);
		}
	}

	void SmtMemPoolMgr::free_memory(IDENTIFIER* pIdentifier)
	{
		SmtMemUnit *pMemoryUnit = NULL;

		// First find the MemoryUnit of the given Identifier
		vector<SmtMemUnit*> ::iterator iterMU = m_vMemUnitPtr.end();
		while(iterMU != m_vMemUnitPtr.begin())
		{
			if ((*iterMU)->get_identifier() == pIdentifier)
			{
				pMemoryUnit = (*iterMU);
				break;
			}
			iterMU--;
		}

		if (iterMU == m_vMemUnitPtr.begin())
		{
			if ((*iterMU)->get_identifier() == pIdentifier)
			{
				pMemoryUnit = (*iterMU);
			}
		}
        // Free the Memory Pool and Remove them
		if (pMemoryUnit)
		{
			// Free All Memory Pool
			m_nUsingMemPool -= pMemoryUnit->free_all_mem_pool();
		}
	}

	SmtMemPool* SmtMemPoolMgr::get_mem_pool(SmtMemUnit *pMemoryUnit, int poolsize)
	{
		// VC-linhai[2007-08-06]: warning C4701: �ֲ�������pMemPool��������δ��ʼ������ʹ��
		// �� pMemPool ��ʼ��ΪNULL
		SmtMemPool * pMemPool = NULL;
		vector<SmtMemPool*> ::iterator iterMP = m_vMemPoolPtr.begin();
		while(iterMP != m_vMemPoolPtr.end())
		{
			if ((*iterMP)->is_free())
			{
				pMemPool = (*iterMP);
				pMemoryUnit->add_tail(pMemPool);
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

			pMemoryUnit->add_tail(pMemPool);
			m_nUsingMemPool++;

			return pMemPool;
		}
		catch(...)
		{
			// �� pMenPool ��ʼ��ʧ�ܣ�ǰ��� pMenPool �ĳ�ʼ����
			SMT_SAFE_DELETE(pMemPool);
		    return NULL;
		}
	}

	unsigned int SmtMemPoolMgr::get_cur_used_size(IDENTIFIER* pIdentifier) const
	{
		SmtMemUnit *pMemoryUnit = NULL;

		// First Check Whether the PartFile is in m_vMemUnitPtr or not
		vector<SmtMemUnit*>::const_iterator iterMU = m_vMemUnitPtr.end();
		while(iterMU != m_vMemUnitPtr.begin())
		{
			if ((*iterMU)->get_identifier() == pIdentifier)
			{
				pMemoryUnit = (*iterMU);
				break;
			}
			iterMU--;
		}
		
		if (iterMU == m_vMemUnitPtr.begin())
		{
			if ((*iterMU)->get_identifier() == pIdentifier)
			{
				pMemoryUnit = (*iterMU);
			}
		}

		if (pMemoryUnit)
		{
			return pMemoryUnit->get_cur_used_size();
		}
		else
		{
			return 0;
		}
	}

	unsigned int SmtMemPoolMgr::get_mem_pool_unit_size() const
	{
	    return m_nPoolUnitSize;
	}

	void SmtMemPoolMgr::check_to_release()
	{
		if ((m_nTotalMemPool - m_nUsingMemPool) > 5)
		{
			if ((GetTickCount() - m_TimeTick) > MIN2MS(30))
			{
				vector<SmtMemPool*> ::iterator iterMP = m_vMemPoolPtr.begin();
				while(iterMP != m_vMemPoolPtr.end())
				{
					if ((*iterMP)->is_free())
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

	byte* SmtMemUnit::get_memory(unsigned int size)
	{
		byte* pData;

		if (m_vMemPoolPtr.size() < 1)
		{
			return NULL; // The Pool List Should not be Empty
		}

		vector<SmtMemPool*> ::iterator iterMP = m_vMemPoolPtr.end();
		iterMP--;
		if ((pData =(*iterMP)->get_memory(size)) != NULL)
		{
			return pData;
		}
		else
		{
			return NULL;
		}
	}

	unsigned int SmtMemUnit::free_all_mem_pool()
	{
		unsigned int number = 0;
		for (int i = m_vMemPoolPtr.size()-1;i > 0; i++)
		{
			m_vMemPoolPtr[i]->free_memory();
		}

		number = m_vMemPoolPtr.size();

		SmtLogManager * pLogMgr = SmtLogManager::get_singleton_ptr();
		SmtLog *pLog = NULL;
		if ((pLog = pLogMgr->get_log(DEFAULT_LOG)) != NULL)
		{
			pLog->log_message("%d Free Memory, PoolList Size is : %d\n", m_pIdentifier, m_vMemPoolPtr.size());
		}
		
		m_vMemPoolPtr.clear();

		return number;
	}

	unsigned int SmtMemUnit::get_cur_used_size() 
	{
		int CurUsedSize = 0;

		if (m_vMemPoolPtr.size() > 0)
		{
			CurUsedSize  = (m_vMemPoolPtr.size() - 1) * m_pMemPoolMgr->get_mem_pool_unit_size();
			CurUsedSize += m_vMemPoolPtr[(m_vMemPoolPtr.size() - 1)]->get_cur_used_size();
			return CurUsedSize;
		}
		else 
		{
			return 0;
		}
	}

	IDENTIFIER * SmtMemUnit::get_identifier() const
	{
		return m_pIdentifier;
	}

	void SmtMemUnit::add_tail(SmtMemPool * pMemPool)
	{
	    m_vMemPoolPtr.push_back(pMemPool);
	}

#ifdef _DEBUG
	void SmtMemUnit::process_log()
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