/*
File:    smt_mempoolmgr.h

Desc:    SmartGis �ڴ�ع�����

Version: Version 1.0

Writter:  

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_MEMPOOLMGR
#define _SMT_MEMPOOLMGR

#include "base/core/mempool.h"

namespace base
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

	class CORE_EXPORT SmtMemUnit
	{
	public:
		SmtMemUnit(SmtMemPoolMgr *pMemPoolMgr, IDENTIFIER * pIdentifier);
		virtual ~SmtMemUnit();

		virtual byte		*get_memory(unsigned int size);
		unsigned int		free_all_mem_pool();
		unsigned int		get_cur_used_size();
		IDENTIFIER			*get_identifier() const;
		void				add_tail(SmtMemPool * pMemPool);
#ifdef  _DEBUG
		void				process_log();
#endif

	private:
		SmtMemPoolMgr		*m_pMemPoolMgr;
		IDENTIFIER			*m_pIdentifier;                   // Pointer to Identifier
		vector<SmtMemPool*>	m_vMemPoolPtr;					  // Memory Pool List
	};

	class CORE_EXPORT SmtMemPoolMgr
	{
	public:
        // Constructor and Destructor
		SmtMemPoolMgr(unsigned int poolsize = DEFAULT_POOLSIZE,
					  unsigned int poolunitsize     = DEFAULT_POOLUNITSIZE,
			          unsigned int blockunitsize    = DEFAULT_BLOCKUNITSIZE);
		virtual ~SmtMemPoolMgr();

		// Attribute
	public:
		unsigned int		get_cur_used_size(IDENTIFIER* pIdentifier) const;
		unsigned int		get_mem_pool_unit_size() const;

		// Operation
	public:
		virtual byte		*get_memory(IDENTIFIER* pIdentifier, unsigned int size);     // Get Memory from the Memory Pool
		virtual void		free_memory(IDENTIFIER* pIdentifier);                       // Free Memory to Memory Pool
		virtual void		check_to_release();										  // Release the Memory Pool Dynamic

#ifdef _DEBUG
		void				process_log();
#endif
        
	protected:
		virtual SmtMemPool	*get_mem_pool(SmtMemUnit *pMemoryUnit, int poolsize);
		virtual void		clear_mem_pool_mgr();

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

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"base_d.lib")
#       else
#          pragma comment(lib,"base.lib")
#	    endif  
#endif

#endif // _SMT_MEMPOOLMGR