/*
File:   gcmre_expression.h 

Desc:   Smt_StaCore,表达式计算

Version: Version 1.0

Writter:  陈春亮

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
		描述：	计算表达式
		输入:	string				strExpression			表达式
		输出:	
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	形如：[C] = ([A]/8+[B])*9;
		--------------------------------------------------------------------*/
		long					Calculate(string strExpression);

		/*--------------------------------------------------------------------
		描述：	计算表达式组
		输入:	vector<string>		&vExpressions			表达式组
		输出:	
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	
		--------------------------------------------------------------------*/
		long					Calculate(const vector<string> &vExpressions);

		/*--------------------------------------------------------------------
		描述：	解析表达式
		输入:	string				&strResult				表达式名称
				string				&strExp					表达式内容
				string				strExpression			表达式
		输出:	string				&strResult				表达式名称
				string				&strExp					表达式内容
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	如：strExpression 为[name]=[content],则 strResult为[name]，strContent为[content]
		--------------------------------------------------------------------*/
		static		long		ParseExpression(string &strResult,string &strContent,const string &strExpression);

		/*--------------------------------------------------------------------
		描述：	组合表达式
		输入:	string				&strExpression			表达式
				string				strResult				表达式名称
				string				strExp					表达式内容
		输出:	string				&strExpression			表达式
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	如：strResult为[name]，strExp为[content]，则strExpression 为[name]=[content]
		--------------------------------------------------------------------*/
		static		long		ToExpression(string &strExpression,const string &strResult,const string &strContent);

	protected:
		/*--------------------------------------------------------------------
		描述：	计算单个表达式
		输入:	string				strResult				表达式名称
				string				strExp					表达式内容
		输出:	
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	
		--------------------------------------------------------------------*/
		long					Calculate(const string &strResult,const string &strExp);

		/*--------------------------------------------------------------------
		描述：	准备表达式数据
		输入:	string				strExp					表达式内容
		输出:	
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	
		--------------------------------------------------------------------*/
		long					PrepareExpOperandVals(const string &strExp);

		/*--------------------------------------------------------------------
		描述：	准备数据集数据
		输入:	string				strOperandName			数据集名称
		输出:	
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	
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