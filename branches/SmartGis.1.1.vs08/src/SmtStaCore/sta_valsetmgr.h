/*
File:   gcmre_valsetmgr.h 

Desc:   Smt_StaCore,实数集管理

Version: Version 1.0

Writter:  陈春亮

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
		描述：	清理所有数据
		输入:	
		输出:	
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	
		--------------------------------------------------------------------*/
		long									Clear();

	public:
		////////////////////////////////////////////////////////////////////
		//数据集管理
		/*--------------------------------------------------------------------
		描述：	获取所有数据集名称
		输入:	vector<string>		&vValSetNames			要素集名称
		输出:	vector<string>		&vValSetNames			要素集名称
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	追加至vValSetNames
		--------------------------------------------------------------------*/
		long									GetAllValSetNames(vector<string> &vValSetNames);

		/*--------------------------------------------------------------------
		描述：	添加数据集
		输入:	const double		*&valsPtr				数据集指针
				long				lNum					数据集数据个数
				const char			*szName					数据集名称
		输出:	
		返回:	long										错误编码，
															SMT_ERR_NONE			成功
															SMT_WRN_ALREADYEXIST	数据集已存在，若为普通数据集则覆盖数据，若为元素数据集则原数据不变
		说明:	
		--------------------------------------------------------------------*/
		long									AddValSetByName(double *&valsPtr ,long lNum,const char*szName);

		//获取数据集
		/*--------------------------------------------------------------------
		描述：	获取数据集
		输入:	const double		*&valsPtr				数据集指针
				long				lNum					数据集数据个数
				const char			*szName					数据集名称
		输出:	const double		*&valsPtr				数据集指针
				long				lNum					数据集数据个数
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	
		--------------------------------------------------------------------*/
		long									GetValSetByName(double *&valsPtr ,long &lNum,const char*szName);
		long									GetValSetByName(const double *&valsPtr ,long &lNum,const char*szName) const;

		/*--------------------------------------------------------------------
		描述：	移除数据集
		输入:	const double		*&valsPtr				数据集指针
				long				lNum					数据集数据个数
				const char			*szName					数据集名称
		输出:	const double		*&valsPtr				数据集指针
				long				lNum					数据集数据个数
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	返回的内存自行释放
		--------------------------------------------------------------------*/
		long									RemoveValSetByName(double *&valsPtr ,long &lNum,const char *szName);

		/*--------------------------------------------------------------------
		描述：	删除数据集
		输入:	const char			*szName					数据集名称
		输出:	
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	返回的内存自行释放
		--------------------------------------------------------------------*/
		long									DeleteValSetByName(const char *szName);

	protected:
		//禁用赋值操作
		const SmtValSetMgr						&operator = (const SmtValSetMgr	&other)
		{
			return *this;
		}

	protected:
		map<string,double*>						m_nameToDbfValSetPtrs;						//操作数索引
		map<string,long>						m_nameToDbfValSetNums;						//操每个作数含实数个数索引
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