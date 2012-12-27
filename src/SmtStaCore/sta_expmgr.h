/*
File:   gcmre_expmgr.h 

Desc:   Smt_StaCore,表达式管理

Version: Version 1.0

Writter:  陈春亮

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
		//表达式管理
		/*--------------------------------------------------------------------
		描述：	获取所有表达式名称
		输入:	vector<string>		&vExpNames				表达式名称
		输出:	vector<string>		&vExpNames				表达式名称
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	
		--------------------------------------------------------------------*/
		long									GetAllExpNames(vector<string> &vExpNames)  const;

		/*--------------------------------------------------------------------
		描述：	获取所有表达式名称及表达式内容
		输入:	vector<SmtExpDesc> &vExpDescs				表达式
		输出:	vector<SmtExpDesc> &vExpDescs				表达式
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	
		--------------------------------------------------------------------*/
		long									GetAllExpDescs(vector<SmtExpDesc> &vExpDescs)  const;

		/*--------------------------------------------------------------------
		描述：	获取所有组合表达式
		输入:	vector<string>			&vExpression		表达式组合
		输出:	vector<string>			&vExpression		表达式组合
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	如："name = content"
		--------------------------------------------------------------------*/
		long									GetAllExpressions(vector<string> &vExpression)  const;

		/*--------------------------------------------------------------------
		描述：	添加表达式
		输入:	string			strName					表达式名称
				SmtExpDesc		&expDesc				表达式描述
				bool			bOnlyMem				仅仅添加至内存
		输出:
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	
		--------------------------------------------------------------------*/
		long									AddExpDescByName(string strName,const SmtExpDesc &expDesc,bool bOnlyMem = false);

		/*--------------------------------------------------------------------
		描述：	添加表达式
		输入:	string			strName					表达式名称
				SmtExpDesc		&expDesc				表达式
		输出:
		返回:	long									错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	
		--------------------------------------------------------------------*/
		long									UpdateExpDescByName(string strName,const SmtExpDesc &expDesc);

		/*--------------------------------------------------------------------
		描述：	获取表达式
		输入:	string			strName					表达式名称
				SmtExpDesc		&expDesc				表达式
		输出:
		返回:	long									错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	
		--------------------------------------------------------------------*/
		long									GetExpDescByName(string strName,SmtExpDesc &expDesc)  const;

		/*--------------------------------------------------------------------
		描述：	删除表达式
		输入:	string			strName					表达式名称
				SmtExpDesc		&expDesc				表达式
		输出:	SmtExpDesc		&expDesc				删除的表达式
		返回:	long									错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	
		--------------------------------------------------------------------*/
		long									DeleteExpDescByName(string strName,SmtExpDesc &expDesc);

	protected:
		//禁用赋值操作
		const SmtExpMgr&	operator =(const SmtExpMgr& other)
		{
			return *this;
		}

	protected:
		map<string,SmtExpDesc>					m_nameToExpDescs;			//表达式索引

	private:
		string									m_strExpTable;				//图幅数据表
		vector<string>							m_vExpTblFldStrings;		//图幅数据表字段结构

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