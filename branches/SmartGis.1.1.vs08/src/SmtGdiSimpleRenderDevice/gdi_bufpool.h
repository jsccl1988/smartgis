//////////////////////////////////////////////////////////////////////////
//���ڴ�� 
//�����̰߳�ȫ
/*
File:    gdi_bufpool.h

Desc:    SmtBufDC,�����ڴ�� 

Version: Version 1.0

Writter:  �´���

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
	//bufռ��״ֵ̬
	enum{
		BufFree = 0,//����
		BufUsed = 1 //��ʹ����
	};

	const u_short	BUF_COUNT		= 64;				//
	const u_short	BUF_SIZE		= 1024*4;			//4k
	class SmtBufPool
	{
	public:
		//////////////////////////////////////////////////////////////////////////
		//���캯��
		//������   nBufCount   ��������
		//          nSizePerBuf ÿ������Ĵ�С
		//����ֵ������ɹ�������ָ������bufָ�룬���򷵻�NULL
		SmtBufPool(u_short nBufCount = BUF_COUNT,u_short nSizePerBuf = BUF_SIZE);

		//////////////////////////////////////////////////////////////////////////
		//��������
		//��������
		//����ֵ����
		~SmtBufPool(void);

		//////////////////////////////////////////////////////////////////////////
		//�������һ��buf
		//��������
		//����ֵ������ɹ�������ָ������bufָ�룬���򷵻�NULL
		char				*NewBuf();

		//////////////////////////////////////////////////////////////////////////
		//�ͷ�һ��buf
		//������buf��ָ��
		//����ֵ����
		void				FreeBuf(char * pBuf);

		//////////////////////////////////////////////////////////////////////////
		//�ͷ�����buf
		//��������
		//����ֵ����
		void				FreeAllBuf();

		//////////////////////////////////////////////////////////////////////////
		//��ȡÿ��buf�Ĵ�С
		//��������
		//����ֵ��ÿ��buf�Ĵ�С
		u_short				GetSizePerBuf() const{return m_nSizePerBuf;}

		//////////////////////////////////////////////////////////////////////////
		//��ȡbuf����
		//��������
		//����ֵ��buf����
		u_short				GetBufCount() const{return m_nBufCount;};

		//////////////////////////////////////////////////////////////////////////
		//��ȡpool�ܴ�С
		//��������
		//����ֵ��pool�ܴ�С
		u_long				GetPoolSize() const{return m_ulPoolSize;}

	private:
		u_short             m_nBufCount;        //��������
		u_short             m_nSizePerBuf;      //ÿ������Ĵ�С
		u_long				m_ulPoolSize;		//�ڴ���ܴ�С
		char *              m_pBuf;             //����ָ��
		PBYTE               m_bBuf;             //����ռ�ö���ָ��
#ifdef SMT_THREAD_SAFE
		SmtCSLock			m_cslock;			//���̰߳�ȫ
#endif
	};
}

#endif //_GDI_BUFPOOL_H
