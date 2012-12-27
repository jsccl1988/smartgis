/*
File:   gcmre_valsetmgr.h 

Desc:   Smt_StaCore,ʵ��������

Version: Version 1.0

Writter:  �´���

Date:    2011.9.19

Copyright (c) Zondy. All rights reserved.
*/
#ifndef _STA_VALSETMGR_H
#define _STA_VALSETMGR_H

#include "sta_bas_struct.h"

namespace Smt_StaCore
{
	class SMT_EXPORT_CLASS SmtValSetMgr
	{
	public:
		SmtValSetMgr(void);
		~SmtValSetMgr(void);

	public:
		/*--------------------------------------------------------------------
		������	������������
		����:	
		���:	
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	
		--------------------------------------------------------------------*/
		long									Clear();

	public:
		////////////////////////////////////////////////////////////////////
		//���ݼ�����
		/*--------------------------------------------------------------------
		������	��ȡ�������ݼ�����
		����:	vector<string>		&vValSetNames			Ҫ�ؼ�����
		���:	vector<string>		&vValSetNames			Ҫ�ؼ�����
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	׷����vValSetNames
		--------------------------------------------------------------------*/
		long									GetAllValSetNames(vector<string> &vValSetNames);

		/*--------------------------------------------------------------------
		������	������ݼ�
		����:	const double		*&valsPtr				���ݼ�ָ��
				long				lNum					���ݼ����ݸ���
				const char			*szName					���ݼ�����
		���:	
		����:	long										������룬
															SMT_ERR_NONE			�ɹ�
															SMT_WRN_ALREADYEXIST	���ݼ��Ѵ��ڣ���Ϊ��ͨ���ݼ��򸲸����ݣ���ΪԪ�����ݼ���ԭ���ݲ���
		˵��:	
		--------------------------------------------------------------------*/
		long									AddValSetByName(double *&valsPtr ,long lNum,const char*szName);

		//��ȡ���ݼ�
		/*--------------------------------------------------------------------
		������	��ȡ���ݼ�
		����:	const double		*&valsPtr				���ݼ�ָ��
				long				lNum					���ݼ����ݸ���
				const char			*szName					���ݼ�����
		���:	const double		*&valsPtr				���ݼ�ָ��
				long				lNum					���ݼ����ݸ���
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	
		--------------------------------------------------------------------*/
		long									GetValSetByName(double *&valsPtr ,long &lNum,const char*szName);
		long									GetValSetByName(const double *&valsPtr ,long &lNum,const char*szName) const;

		/*--------------------------------------------------------------------
		������	�Ƴ����ݼ�
		����:	const double		*&valsPtr				���ݼ�ָ��
				long				lNum					���ݼ����ݸ���
				const char			*szName					���ݼ�����
		���:	const double		*&valsPtr				���ݼ�ָ��
				long				lNum					���ݼ����ݸ���
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	���ص��ڴ������ͷ�
		--------------------------------------------------------------------*/
		long									RemoveValSetByName(double *&valsPtr ,long &lNum,const char *szName);

		/*--------------------------------------------------------------------
		������	ɾ�����ݼ�
		����:	const char			*szName					���ݼ�����
		���:	
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	���ص��ڴ������ͷ�
		--------------------------------------------------------------------*/
		long									DeleteValSetByName(const char *szName);

	protected:
		//���ø�ֵ����
		const SmtValSetMgr						&operator = (const SmtValSetMgr	&other)
		{
			return *this;
		}

	protected:
		map<string,double*>						m_nameToDbfValSetPtrs;						//����������
		map<string,long>						m_nameToDbfValSetNums;						//��ÿ��������ʵ����������
	};
}

#if !defined(Export_SmtStaCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtStaCoreD.lib")
#       else
#          pragma comment(lib,"SmtStaCore.lib")
#	    endif  
#endif

#endif //_STA_VALSETMGR_H