//////////////////////////////////////////////////////////////////////////
//简单内存池 
//具有线程安全
/*
File:    gdi_bufpool.h

Desc:    SmtBufDC,简易内存池 

Version: Version 1.0

Writter:  陈春亮

Date:    2011.11.16

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GDI_BUFPOOL_H
#define _GDI_BUFPOOL_H

#define		GDI_USE_BUFPOOL

#include "smt_core.h"
#include "smt_cslock.h"
using namespace  Smt_Core;
namespace Smt_Rd
{
	//buf占用状态值
	enum{
		BufFree = 0,//空闲
		BufUsed = 1 //在使用中
	};

	const u_short	BUF_COUNT		= 64;				//
	const u_short	BUF_SIZE		= 1024*4;			//4k
	class SmtBufPool
	{
	public:
		//////////////////////////////////////////////////////////////////////////
		//构造函数
		//参数：   nBufCount   缓存总数
		//          nSizePerBuf 每个缓存的大小
		//返回值：分配成功，返回指向分配的buf指针，否则返回NULL
		SmtBufPool(u_short nBufCount = BUF_COUNT,u_short nSizePerBuf = BUF_SIZE);

		//////////////////////////////////////////////////////////////////////////
		//析构函数
		//参数：无
		//返回值：无
		~SmtBufPool(void);

		//////////////////////////////////////////////////////////////////////////
		//请求分配一个buf
		//参数：无
		//返回值：分配成功，返回指向分配的buf指针，否则返回NULL
		char				*NewBuf();

		//////////////////////////////////////////////////////////////////////////
		//释放一个buf
		//参数：buf的指针
		//返回值：无
		void				FreeBuf(char * pBuf);

		//////////////////////////////////////////////////////////////////////////
		//释放所有buf
		//参数：无
		//返回值：无
		void				FreeAllBuf();

		//////////////////////////////////////////////////////////////////////////
		//获取每个buf的大小
		//参数：无
		//返回值：每个buf的大小
		u_short				GetSizePerBuf() const{return m_nSizePerBuf;}

		//////////////////////////////////////////////////////////////////////////
		//获取buf个数
		//参数：无
		//返回值：buf个数
		u_short				GetBufCount() const{return m_nBufCount;};

		//////////////////////////////////////////////////////////////////////////
		//获取pool总大小
		//参数：无
		//返回值：pool总大小
		u_long				GetPoolSize() const{return m_ulPoolSize;}

	private:
		u_short             m_nBufCount;        //缓存总数
		u_short             m_nSizePerBuf;      //每个缓存的大小
		u_long				m_ulPoolSize;		//内存池总大小
		char *              m_pBuf;             //缓存指针
		PBYTE               m_bBuf;             //缓存占用队列指针
#ifdef SMT_THREAD_SAFE
		SmtCSLock			m_cslock;			//多线程安全
#endif
	};
}

#endif //_GDI_BUFPOOL_H
