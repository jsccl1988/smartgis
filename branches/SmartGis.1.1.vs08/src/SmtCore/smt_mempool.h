/*
File:    smt_mempool.h

Desc:    SmartGis ÄÚ´æ³Ø

Version: Version 1.0

Writter:  

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_MEMPOOL_H
#define _SMT_MEMPOOL_H

#include "smt_core.h"
#include "smt_cslock.h"

namespace Smt_Core
{
	struct SmtMemBlock
	{
		byte*			pData;			// Pointer to the actual Data
		unsigned int	unSize;			// Size of the Data Block
	};
	
	typedef SmtMemBlock*	LPMEMBLOCK;

	class SMT_EXPORT_CLASS SmtMemPool
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
		unsigned int			GetMemPoolSize() const;
		unsigned int			GetCurUsedSize() const;
		bool					IsFree() const; 

#ifdef _DEBUG
		int						m_nCode;
#endif
		// Operation
	public:

		virtual byte			*GetMemory(unsigned int size);      // Get Memory from the Memory Pool
		virtual void			FreeMemory();                      // Free Memory to Memory Pool

	protected:

		virtual void			InitMemPool(); // Init the Memory Pool
		virtual void			ClearMemPool();
		virtual LPMEMBLOCK		RequestBlock(unsigned int blocksize);
		virtual unsigned int	CalcBlockSize(unsigned int blocksize);

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

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif  // _SMT_MEMPOOL_H