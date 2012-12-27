/*
File:   sta_calculator.h 

Desc:   Smt_StaCore,������

Version: Version 1.0

Writter:  �´���

Date:    2012.8.22

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _STA_CALCULATOR_H
#define _STA_CALCULATOR_H

#include "smt_core.h"
#include "sta_bas_struct.h"
#include "sta_operand.h"
#include <cmath>
#include <stack>
#include <sstream>

namespace Smt_StaCore
{
	class SMT_EXPORT_CLASS SmtCalculator
	{//������
	public:
		SmtCalculator(void);
		~SmtCalculator(void);

	protected:
		const SmtCalculator				&operator =( const SmtCalculator&other);

	public:
		//////////////////////////////////////////////////////////////////////////
		//����ʽ
		void 							SetExpression(const char* szExp) { m_strExpression = szExp; }
		const char *					GetExpression(void) const { return m_strExpression.c_str(); }

		////////////////////////////////////////////////////////////////////
		//����������
		//��ȡ���в���������
		long							GetOperandNames(vector<string> &vOperandNames);

		//��Ӳ�����
		long							AddOperValsByName(double * &pDbfVals ,long lNum,const char*szName);

		//��ȡ������
		long							GetOperValsByName(double *&pDbfVals ,long &lNum,const char*szName);

		//�Ƴ�������,���и����ͷ�
		long							RemoveOperValsByName(double *&pDbfVals ,long lNum,const char*szName);

		//ɾ��������
		long							DeleteOperValsByName(const char*szName);
		//////////////////////////////////////////////////////////////////////////
		//�����ʽ�Ƿ���ȷ
		long							Check(const char* szExp);

		//////////////////////////////////////////////////////////////////////////
		//ִ��
		long							Run(SmtOperand &resVal);

		//////////////////////////////////////////////////////////////////////////
		//����
		long							Clear(void);

		//��[]ȥ��
		static	long					ParseOperand(string &strOpernd,string strOperndName);

	protected:
		void							SplitExpression();

		int 							Isp( char ch );						//ջ��������
		int 							Icp( char ch );						//ջ��������
		void 							Postfix ();							//����׺���ʽת��Ϊ��׺���ʽ

		void							AddOperand (SmtOperand &operVal );	//���������ջ
		bool							Get2Operands (SmtOperand &left ,SmtOperand &right );//��ջ���˳�����������
		void							DoOperator( char op );				//�γ�����ָ���������

	protected:
		string							m_strExpression;
		stack <SmtOperand>				m_gcOperands;						//ջ������
		vector<string>					m_vStrSplitExp;
		vector<string>					m_vStrPostfixSplitExp;

		vector<double *>				m_vDbfPtrs;							//������
		map<string,double *>			m_nameToOperdbfValsPtrs;			//����������
		map<string,long>				m_nameToOperdbfValsNums;			//����������
	};
}

#if !defined(Export_SmtStaCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtStaCoreD.lib")
#       else
#          pragma comment(lib,"SmtStaCore.lib")
#	    endif  
#endif

#endif //_STA_CALCULATOR_H