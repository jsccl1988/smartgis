/*
File:    sta_math_api.h

Desc:    SmartGis 基本数学统计量 API

Version: Version 1.0

Writter:  陈春亮

Date:    2012.8.22

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _STA_MATH_API_H
#define _STA_MATH_API_H

#include "smt_core.h"
enum eSmtSortOrderType
{//排序方法
	SO_ASC	,					//升序
	SO_DESC						//降序
};

//////////////////////////////////////////////////////////////////////////
static const double								sc_dbf_esp		= 1E-5;
static const double								sc_dbf_pi		= 3.1415926;

static const double								sc_invlid_double_value = 1E+10;
static const unsigned int						sc_invlid_uint_value = 999999;
static const int								sc_invlid_int_value = 99999999;
static const unsigned long						sc_invlid_ulong_value = 999999;
static const long								sc_invlid_long_value = 99999999;
static const float								sc_invlid_float_value = 1E+10;

//////////////////////////////////////////////////////////////////////////
//排序
//////////////////////////////////////////////////////////////////////////
long	SMT_EXPORT_API  SmtSort(double * pDbfVals,long lNum ,char so = SO_ASC);				//排序
long	SMT_EXPORT_API  SmtSort(vector<double> &vVals,char so = SO_ASC);					//排序

long	SMT_EXPORT_API  SmtGetValueByPercent(double &dbfResultValue,const double * pDbfVals,long lNum,double dbfPercent);			//异常下限 pDbfVals 以升序排序,dbfPercent%	
long	SMT_EXPORT_API  SmtGetValueByPercent(double &dbfResultValue,const vector<double> &vVals,double dbfPercent);					//异常下限 pDbfVals 以升序排序,dbfPercent%	

//////////////////////////////////////////////////////////////////////////
//统计
//////////////////////////////////////////////////////////////////////////
long	SMT_EXPORT_API  SmtSum(double &dbfResultValue,const double * pDbfVals,long lNum);				//求和
long	SMT_EXPORT_API  SmtSum(double &dbfResultValue,const vector<double> &vVals);						//求和

long	SMT_EXPORT_API  SmtMax(double &dbfResultValue,const double * pDbfVals,long lNum);				//最大值
long	SMT_EXPORT_API  SmtMax(double &dbfResultValue,const vector<double> &vVals);						//最大值

long	SMT_EXPORT_API  SmtMin(double &dbfResultValue,const double * pDbfVals,long lNum);				//最小值
long	SMT_EXPORT_API  SmtMin(double &dbfResultValue,const vector<double> &vVals);						//最小值

long	SMT_EXPORT_API  SmtAvg(double &dbfResultValue,const double * pDbfVals,long lNum);				//平均值
long	SMT_EXPORT_API  SmtAvg(double &dbfResultValue,const vector<double> &vVals);						//平均值

long	SMT_EXPORT_API  SmtMedian(double &dbfResultValue,const double * pDbfVals,long lNum);			//中位数
long	SMT_EXPORT_API  SmtMedian(double &dbfResultValue,const vector<double> &vVals);					//中位数

long	SMT_EXPORT_API  SmtVariance(double &dbfResultValue,const double * pDbfVals,long lNum);				//方差
long	SMT_EXPORT_API  SmtVariance(double &dbfResultValue,const vector<double> &vVals);					//方差

long	SMT_EXPORT_API  SmtStdDeviation(double &dbfResultValue,const double * pDbfVals,long lNum);			//标准差
long	SMT_EXPORT_API  SmtStdDeviation(double &dbfResultValue,const vector<double> &vVals);				//标准差

long	SMT_EXPORT_API  SmtLogAvg(double &dbfResultValue,const double * pDbfVals,long lNum);				//几何平均值
long	SMT_EXPORT_API  SmtLogAvg(double &dbfResultValue,const vector<double> &vVals);						//几何平均值

//////////////////////////////////////////////////////////////////////////
//标准化
//////////////////////////////////////////////////////////////////////////
long	SMT_EXPORT_API  SmtStandardization(double * pDbfVals,long lNum,double dbfStd);					//指定值标准化
long	SMT_EXPORT_API  SmtStandardization(vector<double> &vVals,double dbfStd);						//指定值标准化

long	SMT_EXPORT_API  SmtStdBySum(double * pDbfVals,long lNum);										//总和标准化
long	SMT_EXPORT_API  SmtStdBySum(vector<double> &vVals);												//总和标准化

long	SMT_EXPORT_API  SmtStdByBackVal(double * pDbfVals,long lNum,long lBVTye,double dbfPercent);		//背景值标准化,dbfPercent,当lBVType = BV_FREQUENCY有效
long	SMT_EXPORT_API  SmtStdByBackVal(vector<double> &vVals,long lBVTye,double dbfPercent);			//背景值标准化,dbfPercent,当lBVType = BV_FREQUENCY有效

long	SMT_EXPORT_API  SmtStdByDeviation(double * pDbfVals,long lNum);									//标准差标准化：各要素的平均值为0，标准差为1
long	SMT_EXPORT_API  SmtStdByDeviation(vector<double> &vVals);										//标准差标准化：各要素的平均值为0，标准差为1

long	SMT_EXPORT_API  SmtStdByMaxVal(double * pDbfVals,long lNum);									//极大值标准化
long	SMT_EXPORT_API  SmtStdByMaxVal(vector<double> &vVals);											//极大值标准化

long	SMT_EXPORT_API  SmtStdByMaxMin(double * pDbfVals,long lNum);									//极差标准化
long	SMT_EXPORT_API  SmtStdByMaxMin(vector<double> &vVals);											//极差标准化

long	SMT_EXPORT_API  SmtStdByMinVal(double * pDbfVals,long lNum);									//极小值标准化
long	SMT_EXPORT_API  SmtStdByMinVal(vector<double> &vVals);											//极小值标准化


//////////////////////////////////////////////////////////////////////////
//计算距离
//////////////////////////////////////////////////////////////////////////
//距离计算方法
long	SMT_EXPORT_API  SmtAbsoluteDistance(double &dbfDis,const double * AValPtr,const double * BValPtr,long lNum);				//绝对值距离
long	SMT_EXPORT_API  SmtAbsoluteDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals);				//绝对值距离

long	SMT_EXPORT_API  SmtEuclideanDistance(double &dbfDis,const double * AValPtr,const double * BValPtr,long lNum);               //欧氏距离
long	SMT_EXPORT_API  SmtEuclideanDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals);				//欧氏距离

long	SMT_EXPORT_API  SmtEuclideanDistanceExt(double &dbfDis,const double * AValPtr,const double * BValPtr,long lNum);            //扩展欧氏距离
long	SMT_EXPORT_API  SmtEuclideanDistanceExt(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals);			//扩展欧氏距离

long	SMT_EXPORT_API  SmtSquaredEuclideanDistance(double &dbfDis,const double * AValPtr,const double * BValPtr,long lNum);		//欧氏距离平方和
long	SMT_EXPORT_API  SmtSquaredEuclideanDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals);		//欧氏距离平方和

long	SMT_EXPORT_API  SmtChebychevDistance(double &dbfDis,const double * AValPtr,const double * BValPtr,long lNum);               //切比雪夫距离
long	SMT_EXPORT_API  SmtChebychevDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals);				//切比雪夫距离

long	SMT_EXPORT_API  SmtMinkowskiDistance(double &dbfDis,const double * AValPtr,const double * BValPtr,long lNum, long lPower);  //明科夫斯基距离
long	SMT_EXPORT_API  SmtMinkowskiDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals, long lPower);//明科夫斯基距离

//////////////////////////////////////////////////////////////////////////
//aux 
bool	SMT_EXPORT_API	SmtIsRealEqual(double a, double b, double eps = sc_dbf_esp);
long	SMT_EXPORT_API  SmtFilter(vector<double> &vVals,double dbfMin,double dbfMax);												//过滤数据，保留下限上限之间的数据
long	SMT_EXPORT_API  SmtExtractDoubleDat(double &dbfValue,short sPos);


#if !defined(Export_SmtStaDiagram)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtStaDiagramD.lib")
#       else
#          pragma comment(lib,"SmtStaDiagram.lib")
#	    endif  
#endif

#endif //_STA_API_H
