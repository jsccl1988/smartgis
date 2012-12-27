/*
File:    sta_bas_struct.h

Desc:    SmartGis 统计模块 基础结构体

Version: Version 1.0

Writter:  陈春亮

Date:    2012.8.22

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _STA_BAS_STRUCT_H
#define _STA_BAS_STRUCT_H

#include "smt_core.h"

namespace Smt_StaCore
{
	typedef vector<float>			vfVals;
	typedef vector<double>			vdbfVals;
	typedef vector<int>				vnVals;
	typedef vector<unsigned int>	vunVals;
	typedef vector<string>			vStringVals;

	typedef double *				dbfValsPtr;
	typedef vector<double *>		vdbfValsPtrs;

	typedef map<string,double *>	mapNameTodbfValsPtr;
	typedef map<string,long>		mapNameTodbfValsNum;


	enum eSmtDataTreatMtd
	{//数据正则化处理
		//标准化
		DTRT_STD_SUM,										//总和标准化
		DTRT_STD_MAX,										//最大值标准化
		DTRT_STD_MIN,										//最小值标准化
		DTRT_STD_MEDIAN,									//中位数标准化
		DTRT_STD_BGV,										//背景值标准化
		DTRT_STD_DEVIATION,									//标准差标准化
		DTRT_STD_MAXMIN,									//极差标准化

		//单元素运算
		DTRT_LN,											//取自然对数
		DTRT_LOG10,											//取对数
		DTRT_SIN,											//取正弦
		DTRT_COS,											//取余弦
		DTRT_TAN,											//取正切
		DTRT_ASIN,											//取反正弦
		DTRT_ACOS,											//取反余弦
		DTRT_ATAN,											//取反正切
		DTRT_LINEAR,										//线性变换
	};

	enum eSmtDistMtd
	{//距离公式
		DIS_ABSOLUTE,										//绝对值距离
		DIS_EUCLIDEAN,										//欧氏距离
		DIS_EUCLIDEAN_EXT,									//欧氏距离(扩展)
		DIS_SQUAREDEUCLIDEAN,								//欧氏距离平方和
		DIS_CHEBYCHEV,										//切比雪夫距离
		DIS_MINKOWSKI,										//明科夫斯基距离
	};

	enum eSmtStaticMtd
	{//统计类型
		DTSTA_MAX				= 1<<0,						//最大值
		DTSTA_MIN				= 1<<1,						//最小值
		DTSTA_AVG				= 1<<2,						//平均值
		DTSTA_MEDIAN			= 1<<3,						//中位数
		DTSTA_STDDEV			= 1<<4,						//标准差
		DTSTA_VARCOEF			= 1<<5,						//变异系数
		DTSTA_VARIANCE			= 1<<6,						//方差
	};

	const	long c_long_dtsta_all = DTSTA_MAX|DTSTA_MIN|DTSTA_AVG|DTSTA_MEDIAN|\
		DTSTA_STDDEV|DTSTA_VARCOEF;

	const	long c_long_dtsta_basic = DTSTA_MAX|DTSTA_MIN|DTSTA_AVG|DTSTA_MEDIAN|DTSTA_VARIANCE|DTSTA_STDDEV;


	//统计结果
	struct SmtStaticResult
	{
		double						dbfMax;					//最大值
		double						dbfMin;					//最小值
		double						dbfAvg;					//平均值
		double						dbfMedian;				//中位数
		double						dbfVariance;			//方差
		double						dbfStdDeviation;		//标准差

		double						dbfVarcoeff;			//变异系数

		eSmtStaticMtd				dtstaType;				//统计项
		long						lStaDataNum;			//数据个数

		SmtStaticResult():dtstaType((eSmtStaticMtd)c_long_dtsta_basic)
			,lStaDataNum(0)
		{
			dbfMax = dbfMin = dbfAvg = dbfMedian = dbfVariance = dbfStdDeviation = 0;
			dbfVarcoeff = 0;
		}
	};

	struct SmtExpDesc 
	{//表达式描述
		string		strName;								//表达式名称，即结果名称
		string		strContent;								//表达式内容
		string		strInfo;								//表达式信息
		int			nPri;									//表达式优先级 越小优先级越高

		/*--------------------------------------------------------------------
		描述：	解析表达式
		输入:	
		输出:
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	如：strExpression 为"name=content",则 strName = "name"，strContent = "content"
		--------------------------------------------------------------------*/
		static		long		ParseExpression(string &strName,string &strContent,string strExpression)
		{
			int nPos = strExpression.find('=');
			if (nPos == string::npos)
				return SMT_ERR_FAILURE;

			strName = strExpression.substr(0,nPos);
			strContent = strExpression.substr(nPos+1,strExpression.size()-nPos-1);

			return SMT_ERR_NONE;
		}

		/*--------------------------------------------------------------------
		描述：	组合表达式
		输入:	
		输出:	
		返回:	long										错误编码，宏 SMT_ERR_NONE，表示成功
		说明:	如：strName = "name"，strContent = "content"，strExpression 为"name=content"
		--------------------------------------------------------------------*/
		static		long		ToExpression(string &strExpression,string strName,string strContent)
		{
			strExpression = "";
			strExpression += strName + "=" + strContent;

			return SMT_ERR_NONE;
		}
	};

}

#if !defined(Export_SmtStaCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtStaCoreD.lib")
#       else
#          pragma comment(lib,"SmtStaCore.lib")
#	    endif  
#endif

#endif //_STA_BAS_STRUCT_H