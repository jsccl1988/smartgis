/*
File:   sta_calculator.h 

Desc:   Smt_StaCore,计算器

Version: Version 1.0

Writter:  陈春亮

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
	{//计算器
	public:
		SmtCalculator(void);
		~SmtCalculator(void);

	protected:
		const SmtCalculator				&operator =( const SmtCalculator&other);

	public:
		//////////////////////////////////////////////////////////////////////////
		//计算式
		void 							SetExpression(const char* szExp) { m_strExpression = szExp; }
		const char *					GetExpression(void) const { return m_strExpression.c_str(); }

		////////////////////////////////////////////////////////////////////
		//操作数管理
		//获取所有操作数名称
		long							GetOperandNames(vector<string> &vOperandNames);

		//添加操作数
		long							AddOperValsByName(double * &pDbfVals ,long lNum,const char*szName);

		//获取操作数
		long							GetOperValsByName(double *&pDbfVals ,long &lNum,const char*szName);

		//移除操作数,自行负责释放
		long							RemoveOperValsByName(double *&pDbfVals ,long lNum,const char*szName);

		//删除操作数
		long							DeleteOperValsByName(const char*szName);
		//////////////////////////////////////////////////////////////////////////
		//检查表达式是否正确
		long							Check(const char* szExp);

		//////////////////////////////////////////////////////////////////////////
		//执行
		long							Run(SmtOperand &resVal);

		//////////////////////////////////////////////////////////////////////////
		//清理
		long							Clear(void);

		//将[]去除
		static	long					ParseOperand(string &strOpernd,string strOperndName);

	protected:
		void							SplitExpression();

		int 							Isp( char ch );						//栈内优先数
		int 							Icp( char ch );						//栈外优先数
		void 							Postfix ();							//将中缀表达式转化为后缀表达式

		void							AddOperand (SmtOperand &operVal );	//进入操作数栈
		bool							Get2Operands (SmtOperand &left ,SmtOperand &right );//从栈中退出两个操作数
		void							DoOperator( char op );				//形成运算指令，进行运算

	protected:
		string							m_strExpression;
		stack <SmtOperand>				m_gcOperands;						//栈对像定义
		vector<string>					m_vStrSplitExp;
		vector<string>					m_vStrPostfixSplitExp;

		vector<double *>				m_vDbfPtrs;							//操作数
		map<string,double *>			m_nameToOperdbfValsPtrs;			//操作数索引
		map<string,long>				m_nameToOperdbfValsNums;			//操作数索引
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