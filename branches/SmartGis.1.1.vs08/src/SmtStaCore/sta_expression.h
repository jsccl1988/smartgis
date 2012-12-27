/*
File:   gcmre_expression.h 

Desc:   Smt_StaCore,���ʽ����

Version: Version 1.0

Writter:  �´���

Date:    2012.8.22

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _STA_EXPRESSION_H
#define _STA_EXPRESSION_H

#include "sta_calculator.h"

namespace Smt_StaCore
{
	class SMT_EXPORT_CLASS SmtExpression
	{
	public:
		SmtExpression(void);
		~SmtExpression(void);

	public:
		/*--------------------------------------------------------------------
		������	������ʽ
		����:	string				strExpression			���ʽ
		���:	
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	���磺[C] = ([A]/8+[B])*9;
		--------------------------------------------------------------------*/
		long					Calculate(string strExpression);

		/*--------------------------------------------------------------------
		������	������ʽ��
		����:	vector<string>		&vExpressions			���ʽ��
		���:	
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	
		--------------------------------------------------------------------*/
		long					Calculate(const vector<string> &vExpressions);

		/*--------------------------------------------------------------------
		������	�������ʽ
		����:	string				&strResult				���ʽ����
				string				&strExp					���ʽ����
				string				strExpression			���ʽ
		���:	string				&strResult				���ʽ����
				string				&strExp					���ʽ����
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	�磺strExpression Ϊ[name]=[content],�� strResultΪ[name]��strContentΪ[content]
		--------------------------------------------------------------------*/
		static		long		ParseExpression(string &strResult,string &strContent,const string &strExpression);

		/*--------------------------------------------------------------------
		������	��ϱ��ʽ
		����:	string				&strExpression			���ʽ
				string				strResult				���ʽ����
				string				strExp					���ʽ����
		���:	string				&strExpression			���ʽ
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	�磺strResultΪ[name]��strExpΪ[content]����strExpression Ϊ[name]=[content]
		--------------------------------------------------------------------*/
		static		long		ToExpression(string &strExpression,const string &strResult,const string &strContent);

	protected:
		/*--------------------------------------------------------------------
		������	���㵥�����ʽ
		����:	string				strResult				���ʽ����
				string				strExp					���ʽ����
		���:	
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	
		--------------------------------------------------------------------*/
		long					Calculate(const string &strResult,const string &strExp);

		/*--------------------------------------------------------------------
		������	׼�����ʽ����
		����:	string				strExp					���ʽ����
		���:	
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	
		--------------------------------------------------------------------*/
		long					PrepareExpOperandVals(const string &strExp);

		/*--------------------------------------------------------------------
		������	׼�����ݼ�����
		����:	string				strOperandName			���ݼ�����
		���:	
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	
		--------------------------------------------------------------------*/
		long					PrepareOperandVal(const string &strOperandName);

	protected:
		SmtCalculator			m_gcCalculator;

	};
}

#if !defined(Export_SmtStaCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtStaCoreD.lib")
#       else
#          pragma comment(lib,"SmtStaCore.lib")
#	    endif  
#endif

#endif //_STA_EXPRESSION_H