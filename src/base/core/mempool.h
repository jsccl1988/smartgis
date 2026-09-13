/*
File:    smt_mempool.h

Desc:    SmartGis �ڴ��

Version: Version 1.0

Writter:  

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_MEMPOOL_H
#define _SMT_MEMPOOL_H

#include "base/core/core.h"
#include "base/core/cslock.h"

namespace base
{
	struct SmtMemBlock
	{
		byte*			pData;			// Pointer to the actual Data
		unsigned int	unSize;			// Size of the Data Block
	};
	
	typedef SmtMemBlock*	LPMEMBLOCK;

	class CORE_EXPORT SmtMemPool
	{
	public:
        // the default unit size is 11K which is bigger than most packet size
#ifdef _DEBUG
		SmtMemPool(int code, unsigned int poolsize = 1024 * 1024 * 5, unsigned int unitsize = 1024 * 11); // Constructor
#else
		SmtMemPool(unsigned int poolsize = 1024 * 1024 * 5, unsigned int unitsize = 1024 * 11); // Constructor
#endif
		
		virtual ~SmtMemPool(); // Destructor
	
		// Attribute
	public:

		// byte*				GetMemPoolEntry() const;
		unsigned int			get_mem_pool_size() const;
		unsigned int			get_cur_used_size() const;
		bool					is_free() const; 

#ifdef _DEBUG
		int						m_nCode;
#endif
		// Operation
	public:

		virtual byte			*get_memory(unsigned int size);      // Get Memory from the Memory Pool
		virtual void			free_memory();                      // Free Memory to Memory Pool

	protected:

		virtual void			init_mem_pool(); // Init the Memory Pool
		virtual void			clear_mem_pool();
		virtual LPMEMBLOCK		request_block(unsigned int blocksize);
		virtual unsigned int	calc_block_size(unsigned int blocksize);

	private:
		vector<SmtMemBlock*>	m_vMemUnitPtr;		//
		vector<SmtMemBlock*>::iterator	m_curUnitPos;       // The Current Position of the Memory Unit List
		byte*					m_pPoolEntry;       // The Pool Entry

		unsigned int			m_nBlockUnitSize;   // Unit block size
		unsigned int			m_nPoolSize;        // The Memory Pool size

		unsigned int			m_nCurUsedSize;     // Current Used Size

#ifdef SMT_THREAD_SAFE
		SmtCSLock				m_cslock;
#endif
	};
};

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"base_d.lib")
#       else
#          pragma comment(lib,"base.lib")
#	    endif  
#endif

#endif  // _SMT_MEMPOOL_H