/*
File:   gcmre_expmgr.h 

Desc:   Smt_StaCore,���ʽ����

Version: Version 1.0

Writter:  �´���

Date:    2012.8.22

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _STA_EXPMGR_H
#define _STA_EXPMGR_H

#include "sta_bas_struct.h"

namespace Smt_StaCore
{
	class SMT_EXPORT_CLASS SmtExpMgr
	{
	public:
		SmtExpMgr(void);
		virtual ~SmtExpMgr(void);

	public:
		//////////////////////////////////////////////////////////////////////////
		//���ʽ����
		/*--------------------------------------------------------------------
		������	��ȡ���б��ʽ����
		����:	vector<string>		&vExpNames				���ʽ����
		���:	vector<string>		&vExpNames				���ʽ����
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	
		--------------------------------------------------------------------*/
		long									GetAllExpNames(vector<string> &vExpNames)  const;

		/*--------------------------------------------------------------------
		������	��ȡ���б��ʽ���Ƽ����ʽ����
		����:	vector<SmtExpDesc> &vExpDescs				���ʽ
		���:	vector<SmtExpDesc> &vExpDescs				���ʽ
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	
		--------------------------------------------------------------------*/
		long									GetAllExpDescs(vector<SmtExpDesc> &vExpDescs)  const;

		/*--------------------------------------------------------------------
		������	��ȡ������ϱ��ʽ
		����:	vector<string>			&vExpression		���ʽ���
		���:	vector<string>			&vExpression		���ʽ���
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	�磺"name = content"
		--------------------------------------------------------------------*/
		long									GetAllExpressions(vector<string> &vExpression)  const;

		/*--------------------------------------------------------------------
		������	��ӱ��ʽ
		����:	string			strName					���ʽ����
				SmtExpDesc		&expDesc				���ʽ����
				bool			bOnlyMem				����������ڴ�
		���:
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	
		--------------------------------------------------------------------*/
		long									AddExpDescByName(string strName,const SmtExpDesc &expDesc,bool bOnlyMem = false);

		/*--------------------------------------------------------------------
		������	��ӱ��ʽ
		����:	string			strName					���ʽ����
				SmtExpDesc		&expDesc				���ʽ
		���:
		����:	long									������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	
		--------------------------------------------------------------------*/
		long									UpdateExpDescByName(string strName,const SmtExpDesc &expDesc);

		/*--------------------------------------------------------------------
		������	��ȡ���ʽ
		����:	string			strName					���ʽ����
				SmtExpDesc		&expDesc				���ʽ
		���:
		����:	long									������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	
		--------------------------------------------------------------------*/
		long									GetExpDescByName(string strName,SmtExpDesc &expDesc)  const;

		/*--------------------------------------------------------------------
		������	ɾ�����ʽ
		����:	string			strName					���ʽ����
				SmtExpDesc		&expDesc				���ʽ
		���:	SmtExpDesc		&expDesc				ɾ���ı��ʽ
		����:	long									������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	
		--------------------------------------------------------------------*/
		long									DeleteExpDescByName(string strName,SmtExpDesc &expDesc);

	protected:
		//���ø�ֵ����
		const SmtExpMgr&	operator =(const SmtExpMgr& other)
		{
			return *this;
		}

	protected:
		map<string,SmtExpDesc>					m_nameToExpDescs;			//���ʽ����

	private:
		string									m_strExpTable;				//ͼ�����ݱ�
		vector<string>							m_vExpTblFldStrings;		//ͼ�����ݱ��ֶνṹ

	};
}

#if !defined(Export_SmtStaCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtStaCoreD.lib")
#       else
#          pragma comment(lib,"SmtStaCore.lib")
#	    endif  
#endif

#endif  //_STA_EXPMGR_H