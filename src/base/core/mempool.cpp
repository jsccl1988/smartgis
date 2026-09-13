#include "base/core/mempool.h"

namespace base
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

		init_mem_pool(); // Init the Memory Pool
	}

	SmtMemPool::~SmtMemPool()
	{
		clear_mem_pool(); // Clear the node in list

		if (m_pPoolEntry)
		{
			delete[] m_pPoolEntry; // Free the memory
			m_pPoolEntry = NULL; // Free the Pointer
		}
	}

	byte* SmtMemPool::get_memory(unsigned int size)
	{
		assert(size > 0);
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif
		LPMEMBLOCK pMemoryBlock = request_block(size);
#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
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

	LPMEMBLOCK SmtMemPool::request_block(unsigned int blocksize)
	{
		// Request from m_vMemUnitPtr for Memory Block
		if (m_curUnitPos != m_vMemUnitPtr.end() &&
			(*m_curUnitPos)->unSize >= blocksize)
		{
			LPMEMBLOCK pMemoryBlock = *m_curUnitPos;
			unsigned int unitsize  = calc_block_size(blocksize);

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

	void SmtMemPool::free_memory()
	{
#ifdef SMT_THREAD_SAFE
		m_cslock.lock();
#endif

		m_curUnitPos = m_vMemUnitPtr.begin();

#ifdef SMT_THREAD_SAFE
		m_cslock.unlock();
#endif

		m_nCurUsedSize = 0; // Reset the value of m_nCurUsedSize
	}

	unsigned int SmtMemPool::calc_block_size(unsigned int blocksize)
	{
		unsigned int size = 0;

		size = blocksize / m_nBlockUnitSize;
		if (blocksize % m_nBlockUnitSize)
		{
			size++;
		}

		return size;
	}

	void SmtMemPool::init_mem_pool()
	{
		try
		{
			unsigned int blocksize = calc_block_size(m_nPoolSize);
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

	void SmtMemPool::clear_mem_pool()
	{
		SMT_SAFE_DELETE_A(*(m_vMemUnitPtr.begin()))
		m_vMemUnitPtr.clear();
	}

	//byte* SmtMemPool::GetMemPoolEntry() const
	//{
	//	return m_pPoolEntry;
	//}

	unsigned int SmtMemPool::get_mem_pool_size() const
	{
		return m_nPoolSize;
	}

	unsigned int SmtMemPool::get_cur_used_size() const
	{
	    return m_nCurUsedSize;
	}

	bool SmtMemPool::is_free() const
	{
	    return m_curUnitPos == m_vMemUnitPtr.begin();
	}
};