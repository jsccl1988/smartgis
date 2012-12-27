/*
File:    smt_mempoolmgr.h

Desc:    SmartGis ÄÚ´æ³Ø¹ÜÀíÆ÷

Version: Version 1.0

Writter:  

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_MEMPOOLMGR
#define _SMT_MEMPOOLMGR

#include "smt_mempool.h"

namespace Smt_Core
{
	const string	   DEFAULT_LOG				= "SmtMemPool";
	const unsigned int DEFAULT_POOLSIZE			= 1024 * 1024 * 10;
	const unsigned int DEFAULT_POOLUNITSIZE		= 1024 * 1024;
	const unsigned int DEFAULT_BLOCKUNITSIZE	= 1024 * 11; // the default unit size is 11K which is bigger than most packet size

	#define	SEC2MS(sec)							((sec)*1000)
	#define	MIN2MS(min)							SEC2MS((min)*60)
	#define	HR2MS(hr)							MIN2MS((hr)*60)
	#define DAY2MS(day)							HR2MS((day)*24)
	#define SEC(sec)							(sec)
	#define	MIN2S(min)							((min)*60)
	#define	HR2S(hr)							MIN2S((hr)*60)
	#define DAY2S(day)							HR2S((day)*24)

	typedef VOID								IDENTIFIER;
	typedef LPVOID								LPIDENTIFIER;

	class SmtMemPoolMgr;

	class SMT_EXPORT_CLASS SmtMemUnit
	{
	public:
		SmtMemUnit(SmtMemPoolMgr *pMemPoolMgr, IDENTIFIER * pIdentifier);
		virtual ~SmtMemUnit();

		virtual byte		*GetMemory(unsigned int size);
		unsigned int		FreeAllMemPool();
		unsigned int		GetCurUsedSize();
		IDENTIFIER			*GetIdentifier() const;
		void				AddTail(SmtMemPool * pMemPool);
#ifdef  _DEBUG
		void				ProcessLog();
#endif

	private:
		SmtMemPoolMgr		*m_pMemPoolMgr;
		IDENTIFIER			*m_pIdentifier;                   // Pointer to Identifier
		vector<SmtMemPool*>	m_vMemPoolPtr;					  // Memory Pool List
	};

	class SMT_EXPORT_CLASS SmtMemPoolMgr
	{
	public:
        // Constructor and Destructor
		SmtMemPoolMgr(unsigned int poolsize = DEFAULT_POOLSIZE,
					  unsigned int poolunitsize     = DEFAULT_POOLUNITSIZE,
			          unsigned int blockunitsize    = DEFAULT_BLOCKUNITSIZE);
		virtual ~SmtMemPoolMgr();

		// Attribute
	public:
		unsigned int		GetCurUsedSize(IDENTIFIER* pIdentifier) const;
		unsigned int		GetMemPoolUnitSize() const;

		// Operation
	public:
		virtual byte		*GetMemory(IDENTIFIER* pIdentifier, unsigned int size);     // Get Memory from the Memory Pool
		virtual void		FreeMemory(IDENTIFIER* pIdentifier);                       // Free Memory to Memory Pool
		virtual void		CheckToRelease();										  // Release the Memory Pool Dynamic

#ifdef _DEBUG
		void				ProcessLog();
#endif
        
	protected:
		virtual SmtMemPool	*GetMemPool(SmtMemUnit *pMemoryUnit, int poolsize);
		virtual void		ClearMemPoolMgr();

	private:
		vector<SmtMemUnit*>	m_vMemUnitPtr;	  // Memory Unit List
		vector<SmtMemPool*> m_vMemPoolPtr;    // Memory Pool List

		unsigned int		m_nPoolSize;      // The total size of the Memory Pools
		unsigned int		m_nPoolUnitSize;  // The Unit Size of Memory Pool
		unsigned int		m_nBlockUnitSize; // The Memory Block Unit size of Each Memory Pool

		unsigned int		m_nTotalMemPool;  // The Total Number of MemPool
		unsigned int		m_nUsingMemPool;  // The Number of Using MemPool

		DWORD				m_TimeTick;       // The begining time of Check to Release 

#ifdef _DEBUG
		int					m_nCode;
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

#endif // _SMT_MEMPOOLMGR