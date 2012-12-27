#include "smt_mempool.h"

namespace Smt_Core
{
#ifdef _DEBUG
	SmtMemPool::SmtMemPool(int code, unsigned int poolsize, unsigned int unitsize)// Constructor
#else
	SmtMemPool::SmtMemPool(unsigned int poolsize, unsigned int unitsize)
#endif	
	{
#ifdef _DEBUG
		m_nCode = code;
#endif
		m_nPoolSize      = poolsize;
		m_nBlockUnitSize = unitsize;
		m_nCurUsedSize   = 0;

		InitMemPool(); // Init the Memory Pool
	}

	SmtMemPool::~SmtMemPool()
	{
		ClearMemPool(); // Clear the node in list

		if (m_pPoolEntry)
		{
			delete[] m_pPoolEntry; // Free the memory
			m_pPoolEntry = NULL; // Free the Pointer
		}
	}

	byte* SmtMemPool::GetMemory(unsigned int size)
	{
		assert(size > 0);
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif
		LPMEMBLOCK pMemoryBlock = RequestBlock(size);
#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif
		if (pMemoryBlock)
		{
			return pMemoryBlock->pData;
		}
		else
		{
			return NULL;
		}
	}

	LPMEMBLOCK SmtMemPool::RequestBlock(unsigned int blocksize)
	{
		// Request from m_vMemUnitPtr for Memory Block
		if (m_curUnitPos != m_vMemUnitPtr.end() &&
			(*m_curUnitPos)->unSize >= blocksize)
		{
			LPMEMBLOCK pMemoryBlock = *m_curUnitPos;
			unsigned int unitsize  = CalcBlockSize(blocksize);

			vector<SmtMemBlock*>::iterator pos = m_curUnitPos;
			for (unsigned int i = 0; i < unitsize; i++)
			{
				if (m_curUnitPos == m_vMemUnitPtr.end())
				{
					m_curUnitPos = pos; // This value scroll back
					return NULL;
				}

				m_curUnitPos++;
			}

			m_nCurUsedSize += unitsize * m_nBlockUnitSize; // Increase the value of m_nCurUsedSize

			return pMemoryBlock;
		}
		// return NULL if Current Memory Size can not satisfy the Request
		return NULL;
	}

	void SmtMemPool::FreeMemory()
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.Lock();
#endif

		m_curUnitPos = m_vMemUnitPtr.begin();

#ifdef SMT_THREAD_SAFE
		m_cslock.Unlock();
#endif

		m_nCurUsedSize = 0; // Reset the value of m_nCurUsedSize
	}

	unsigned int SmtMemPool::CalcBlockSize(unsigned int blocksize)
	{
		unsigned int size = 0;

		size = blocksize / m_nBlockUnitSize;
		if (blocksize % m_nBlockUnitSize)
		{
			size++;
		}

		return size;
	}

	void SmtMemPool::InitMemPool()
	{
		try
		{
			unsigned int blocksize = CalcBlockSize(m_nPoolSize);
			LPMEMBLOCK pMemBlock = new SmtMemBlock[blocksize];
			unsigned int i = 0;

			for (i = 0; i < blocksize; i++)
			{
				m_vMemUnitPtr.push_back(&pMemBlock[i]);
			}

			m_pPoolEntry = (byte*) new byte[m_nPoolSize];
			vector<SmtMemBlock*>::iterator iterMU = m_vMemUnitPtr.begin();
			while(iterMU != m_vMemUnitPtr.end())
			{
				(*iterMU)->pData = (byte*)(m_pPoolEntry + i * m_nBlockUnitSize);
				(*iterMU)->unSize = m_nPoolSize - i * m_nBlockUnitSize;
				iterMU++;
			}	
		}
		catch (...)
		{
			// TODO : Catch the Exception of Memory allocation
		}

		m_curUnitPos = m_vMemUnitPtr.begin();
	}

	void SmtMemPool::ClearMemPool()
	{
		SMT_SAFE_DELETE_A(*(m_vMemUnitPtr.begin()))
		m_vMemUnitPtr.clear();
	}

	//byte* SmtMemPool::GetMemPoolEntry() const
	//{
	//	return m_pPoolEntry;
	//}

	unsigned int SmtMemPool::GetMemPoolSize() const
	{
		return m_nPoolSize;
	}

	unsigned int SmtMemPool::GetCurUsedSize() const
	{
	    return m_nCurUsedSize;
	}

	bool SmtMemPool::IsFree() const
	{
	    return m_curUnitPos == m_vMemUnitPtr.begin();
	}
};