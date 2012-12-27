#include "sta_math_api.h"
#include <math.h>
#include <cmath>
#include <algorithm>
#include <functional>

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////
long	SmtSort(double * pDbfVals,long lNum ,char so)		//排序
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	if (so == SO_ASC)
	{
		sort(pDbfVals,pDbfVals+lNum,less<double>());
	}
	else if (so == SO_DESC)
	{
		sort(pDbfVals,pDbfVals+lNum,greater<double>());
	}

	return SMT_ERR_NONE;
}

long	SmtSort(vector<double> &vVals,char so)		//排序
{
	if(vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	if (so == SO_ASC)
	{
		sort(vVals.begin(),vVals.end(),less<double>());
	}
	else if (so == SO_DESC)
	{
		sort(vVals.begin(),vVals.end(),greater<double>());
	}

	return SMT_ERR_NONE;
}

long	SmtGetValueByPercent(double &dbfResultValue,const double * pDbfVals,long lNum,double dbfPercent)
{//异常下限 pDbfVals 以升序排序,dbfPercent%	
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	int l = (int)lNum*dbfPercent/100.; 
	dbfResultValue =  pDbfVals[l];

	return SMT_ERR_NONE;
}

long	SmtGetValueByPercent(double &dbfResultValue,const vector<double> &vVals,double dbfPercent)
{//异常下限 pDbfVals 以升序排序,dbfPercent%	
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	int l = (int)vVals.size()*dbfPercent/100.; 
	dbfResultValue =  vVals[l];

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
//统计
//////////////////////////////////////////////////////////////////////////
long	SmtSum(double &dbfResultValue,const double * pDbfVals,long lNum)//求和
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfSum = sc_invlid_double_value;

	dbfSum = 0.;
	for (int i = 0; i < lNum ; i++)
		dbfSum += pDbfVals[i];

	dbfResultValue = dbfSum;

	return SMT_ERR_NONE;
}

long	SmtSum(double &dbfResultValue,const vector<double> &vVals)//求和
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfSum = sc_invlid_double_value;

	dbfSum = 0.;
	for (int i = 0; i < vVals.size() ; i++)
		dbfSum += vVals[i];

	dbfResultValue = dbfSum;

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long	SmtMax(double &dbfResultValue,const double * pDbfVals,long lNum)//最大值
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfMaxValue = sc_invlid_double_value;

	dbfMaxValue = pDbfVals[0];
	for (int i = 0; i < lNum ; i++)
		dbfMaxValue = (pDbfVals[i] > dbfMaxValue)?pDbfVals[i]:dbfMaxValue;

	dbfResultValue = dbfMaxValue;

	return SMT_ERR_NONE;
}

long	SmtMax(double &dbfResultValue,const vector<double> &vVals)//最大值
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfMaxValue = sc_invlid_double_value;

	dbfMaxValue = vVals[0];
	for (int i = 0; i < vVals.size() ; i++)
		dbfMaxValue = (vVals[i] > dbfMaxValue)?vVals[i]:dbfMaxValue;

	dbfResultValue = dbfMaxValue;

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long	SmtMin(double &dbfResultValue,const double * pDbfVals,long lNum)//最小值
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfMinValue = sc_invlid_double_value;

	dbfMinValue = pDbfVals[0];
	for (int i = 0; i < lNum ; i++)
		dbfMinValue = (pDbfVals[i] < dbfMinValue)?pDbfVals[i]:dbfMinValue;

	dbfResultValue = dbfMinValue;

	return SMT_ERR_NONE;
}

long	SmtMin(double &dbfResultValue,const vector<double> &vVals)//最小值
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfMinValue = sc_invlid_double_value;

	dbfMinValue = vVals[0];
	for (int i = 0; i < vVals.size() ; i++)
		dbfMinValue = (vVals[i] < dbfMinValue)?vVals[i]:dbfMinValue;

	dbfResultValue = dbfMinValue;

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long	SmtAvg(double &dbfResultValue,const double * pDbfVals,long lNum)//平均值
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfSum = sc_invlid_double_value;
	SmtSum(dbfSum,pDbfVals,lNum);
	dbfResultValue = dbfSum/lNum;
	return SMT_ERR_NONE;
}

long	SmtAvg(double &dbfResultValue,const vector<double> &vVals)//平均值
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfSum = sc_invlid_double_value;
	SmtSum(dbfSum,vVals);
	dbfResultValue = dbfSum/vVals.size();

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long	SmtMedian(double &dbfResultValue,const double * pDbfVals,long lNum)//中位数
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfMedian = sc_invlid_double_value;
	double * tmpPtr = new double[lNum];

	memcpy(tmpPtr,pDbfVals,lNum*sizeof(double));

	SmtSort(tmpPtr,lNum);

	if (lNum % 2==0)
	{
		int l1 = lNum/2-1;
		int l2 = l1+1;
		dbfMedian = (tmpPtr[l1]+tmpPtr[l2])/2;
	}
	else
	{
		int l = (lNum-1)/2;
		dbfMedian = tmpPtr[l];
	}

	SMT_SAFE_DELETE_A(tmpPtr);

	dbfResultValue =  dbfMedian;

	return SMT_ERR_NONE;
}

long	SmtMedian(double &dbfResultValue,const vector<double> &vVals)//中位数
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfMedian = sc_invlid_double_value;

	vector<double> vTmpVals(vVals);

	SmtSort(vTmpVals);

	long lNum = (long)vTmpVals.size();
	if (lNum % 2==0)
	{
		int l1 = lNum/2-1;
		int l2 = l1+1;
		dbfMedian = (vTmpVals[l1]+vTmpVals[l2])/2;
	}
	else
	{
		int l = (lNum-1)/2;
		dbfMedian = vTmpVals[l];
	}

	dbfResultValue =  dbfMedian;

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long	SmtBackByBox(double &dbfResultValue,double &dbfRsvStdDeviation,const double * pDbfVals,long lNum,double dbfK1,double dbfK2)//背景值,箱图法
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	vector<double> vRsvVals;
	vRsvVals.resize(lNum);
	for (int i = 0; i < lNum;i++)
		vRsvVals[i] = pDbfVals[i];

	int nRsv = 0;

	while(nRsv != vRsvVals.size())
	{
		double dbfAvg = 0.,dbfStdDeviation = 0.;
		double dbfMin = 0,dbfMax = 0.;

		SmtAvg(dbfAvg,vRsvVals);
		SmtStdDeviation(dbfStdDeviation,vRsvVals);

		dbfMin = dbfAvg - dbfK2*dbfStdDeviation;
		dbfMax = dbfAvg + dbfK1*dbfStdDeviation;

		nRsv = vRsvVals.size();
		dbfResultValue = dbfAvg;
		dbfRsvStdDeviation = dbfStdDeviation;

		SmtFilter(vRsvVals,dbfMin,dbfMax);
	}

	return SMT_ERR_NONE;
}

long	SmtBackByBox(double &dbfResultValue,double &dbfRsvStdDeviation,const vector<double> &vVals,double dbfK1,double dbfK2)//背景值,箱图法
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	vector<double> vRsvVals;
	vRsvVals = vVals;

	int nRsv = 0;

	while(nRsv != vRsvVals.size())
	{
		double dbfAvg = 0.,dbfStdDeviation = 0.;
		double dbfMin = 0,dbfMax = 0.;

		SmtAvg(dbfAvg,vRsvVals);
		SmtStdDeviation(dbfStdDeviation,vRsvVals);

		dbfMin = dbfAvg - dbfK2*dbfStdDeviation;
		dbfMax = dbfAvg + dbfK1*dbfStdDeviation;

		nRsv = vRsvVals.size();
		dbfResultValue = dbfAvg;
		dbfRsvStdDeviation = dbfStdDeviation;

		SmtFilter(vRsvVals,dbfMin,dbfMax);
	}
	return SMT_ERR_NONE;
}

long	SmtBackByPercent(double &dbfResultValue,const double * pDbfVals,long lNum,double dbfPercent)//背景值,频度法
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	int l = (int)lNum*dbfPercent/100.; 
	dbfResultValue =  pDbfVals[l];

	return SMT_ERR_NONE;
}

long	SmtBackByPercent(double &dbfResultValue,const vector<double> &vVals,double dbfPercent)//背景值,频度法
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	int l = (int)vVals.size()*dbfPercent/100.; 
	dbfResultValue =  vVals[l];

	return SMT_ERR_NONE;
}

long	SmtBackByFractal(double &dbfResultValue,const double * pDbfVals,long lNum)//背景值,分形法
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	return SMT_ERR_UNSUPPORTED;
}

long	SmtBackByFractal(double &dbfResultValue,const vector<double> &vVals)//背景值,分形法
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	return SMT_ERR_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////
long	SmtVariance(double &dbfResultValue,const double * pDbfVals,long lNum)//方差
{
	if(NULL == pDbfVals || lNum < 2)
		return SMT_ERR_INVALID_PARAM;

	double dbfAvg = 0.;
	SmtAvg(dbfAvg,pDbfVals,lNum);

	dbfResultValue = 0;
	for (long l = 0; l < lNum; ++ l)
	{
		dbfResultValue += (pDbfVals[l] - dbfAvg) * (pDbfVals[l] - dbfAvg);
	}

	dbfResultValue = dbfResultValue / (lNum - 1);

	return SMT_ERR_NONE;
}

long	SmtVariance(double &dbfResultValue,const vector<double> &vVals)//方差
{
	if (vVals.size() < 2)
		return SMT_ERR_INVALID_PARAM;

	double dbfAvg = 0.;
	SmtAvg(dbfAvg,vVals);

	dbfResultValue = 0;
	for (long l = 0; l < vVals.size(); ++ l)
	{
		dbfResultValue += (vVals[l] - dbfAvg) * (vVals[l] - dbfAvg);
	}

	dbfResultValue = dbfResultValue / (vVals.size() - 1);

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long	SmtStdDeviation(double &dbfResultValue,const double * pDbfVals,long lNum)//标准差
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfVariance = 0.;
	SmtVariance(dbfVariance,pDbfVals,lNum);

	dbfResultValue = sqrt(dbfVariance);

	return SMT_ERR_NONE;
}

long	SmtStdDeviation(double &dbfResultValue,const vector<double> &vVals)//标准差
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfVariance = 0.;
	SmtVariance(dbfVariance,vVals);

	dbfResultValue = sqrt(dbfVariance);

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long	SmtLogAvg(double &dbfResultValue,const double * pDbfVals,long lNum)//几何平均值
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	dbfResultValue = 0;
	for (long l = 0; l < lNum; ++ l)
	{
		dbfResultValue += log(pDbfVals[l]);
	}

	dbfResultValue /= lNum;

	return SMT_ERR_NONE;
}

long	SmtLogAvg(double &dbfResultValue,const vector<double> &vVals)//几何平均值
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	dbfResultValue = 0;
	for (long l = 0; l < vVals.size(); ++ l)
	{
		dbfResultValue += log(vVals[l]);
	}

	dbfResultValue /= vVals.size();

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
//计算异常
long	SMT_EXPORT_API  SmtAbnormity(double &dbfHightLevel,double &dbfLowLevel,double dbfBack,double dbfStdDeviation,double dbfStdDevCoeff /*= 2/1.6*/)
{
	dbfHightLevel	= dbfBack + dbfStdDevCoeff*dbfStdDeviation;
	dbfLowLevel		= dbfBack - dbfStdDevCoeff*dbfStdDeviation;
	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
//标准化
//////////////////////////////////////////////////////////////////////////
long	SmtStandardization(double * pDbfVals,long lNum,double dbfStd)//指定值标准化
{
	if(NULL == pDbfVals || lNum < 1 || SmtIsRealEqual(dbfStd,0.))
		return SMT_ERR_INVALID_PARAM;

	for (long l = 0; l < lNum; ++ l)
	{
		pDbfVals[l] = pDbfVals[l]/dbfStd;
	}

	return SMT_ERR_NONE;
}

long	SmtStandardization(vector<double> &vVals,double dbfStd)//指定值标准化
{
	if(vVals.size() < 1 || SmtIsRealEqual(dbfStd,0.))
		return SMT_ERR_INVALID_PARAM;

	for (long l = 0; l < vVals.size(); ++ l)
	{
		vVals[l] = vVals[l]/dbfStd;
	}

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long	SmtStdBySum(double * pDbfVals,long lNum)//总和标准化
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfSum = 1.;

	//求和
	SmtSum(dbfSum,pDbfVals,lNum);

	//标准化
	SmtStandardization(pDbfVals,lNum,dbfSum);

	return SMT_ERR_NONE;
}

long	SmtStdBySum(vector<double> &vVals)//总和标准化
{
	if(vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfSum = 1.;

	//求和
	SmtSum(dbfSum,vVals);

	//标准化
	SmtStandardization(vVals,dbfSum);

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
long	SmtStdByDeviation(double * pDbfVals,long lNum)//标准差标准化：各要素的平均值为0，标准差为1
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfAvg = 0;
	double dbfDev = 1.;

	SmtAvg(dbfAvg,pDbfVals,lNum);
	SmtStdDeviation(dbfDev,pDbfVals,lNum);

	if (SmtIsRealEqual(dbfDev,0.))
		return SMT_ERR_FAILURE;

	for (long l = 0; l < lNum; ++ l)
	{
		pDbfVals[l] = (pDbfVals[l] - dbfAvg) / dbfDev;
	}

	return SMT_ERR_NONE;
}

long	SmtStdByDeviation(vector<double> &vVals)//标准差标准化：各要素的平均值为0，标准差为1
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfAvg = 0;
	double dbfDev = 1.;

	SmtAvg(dbfAvg,vVals);
	SmtStdDeviation(dbfDev,vVals);

	if (SmtIsRealEqual(dbfDev,0.))
		return SMT_ERR_FAILURE;

	for (long l = 0; l < vVals.size(); ++ l)
	{
		vVals[l] = (vVals[l] - dbfAvg) / dbfDev;
	}

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long	SmtStdByMaxVal(double * pDbfVals,long lNum)//极大值标准化
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfMax = 0;

	//求最大值
	SmtMax(dbfMax,pDbfVals,lNum);

	//标准化
	SmtStandardization(pDbfVals,lNum,dbfMax);

	return SMT_ERR_NONE;
}

long	SmtStdByMaxVal(vector<double> &vVals)//极大值标准化
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfMax = 0.;

	//求最大值
	SmtMax(dbfMax,vVals);

	//标准化
	SmtStandardization(vVals,dbfMax);

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long	SmtStdByMinVal(double * pDbfVals,long lNum)//极小值标准化
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfMin = 0;

	//求最小值
	SmtMin(dbfMin,pDbfVals,lNum);

	//标准化
	SmtStandardization(pDbfVals,lNum,dbfMin);

	return SMT_ERR_NONE;
}

long	SmtStdByMinVal(vector<double> &vVals)//极小值标准化
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfMin = 0;

	//求最小值
	SmtMin(dbfMin,vVals);

	//标准化
	SmtStandardization(vVals,dbfMin);

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long	SmtStdByMaxMin(double * pDbfVals,long lNum)//极差标准化
{
	if(NULL == pDbfVals || lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfMin = 0;
	double dbfMax = 0.;

	//求最小值
	SmtMin(dbfMin,pDbfVals,lNum);

	//求最大值
	SmtMax(dbfMax,pDbfVals,lNum);

	if (SmtIsRealEqual(dbfMin,dbfMax))
	{
		if (!SmtIsRealEqual(0.,dbfMax))
		{
			for (long l = 0; l < lNum; ++ l)
			{
				pDbfVals[l] = 1;
			}
		}
	}
	else
	{
		double dbfDivide = (dbfMax - dbfMin);
		for (long l = 0; l < lNum; ++ l)
		{
			pDbfVals[l] = (pDbfVals[l] - dbfMin)/dbfDivide;
		}
	}

	return SMT_ERR_NONE;
}

long	SmtStdByMaxMin(vector<double> &vVals)//极差标准化
{
	if (vVals.size() < 1)
		return SMT_ERR_INVALID_PARAM;

	double dbfMin = 0;
	double dbfMax = 0.;

	//求最小值
	SmtMin(dbfMin,vVals);

	//求最大值
	SmtMax(dbfMax,vVals);

	if (SmtIsRealEqual(dbfMin,dbfMax))
	{
		if (!SmtIsRealEqual(0.,dbfMax))
		{
			for (long l = 0; l < vVals.size(); ++ l)
			{
				vVals[l] = 1;
			}
		}
	}
	else
	{
		double dbfDivide = (dbfMax - dbfMin);
		for (long l = 0; l < vVals.size(); ++ l)
		{
			vVals[l] = (vVals[l] - dbfMin)/dbfDivide;
		}
	}

	return SMT_ERR_NONE;
}


//////////////////////////////////////////////////////////////////////////
//计算距离
//////////////////////////////////////////////////////////////////////////
//距离计算方法
long	SmtAbsoluteDistance(double &dbfDis,const double * valAPtr,const double * valBPtr,long lNum)//绝对值距离
{
	if(NULL == valAPtr ||NULL == valBPtr|| lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	dbfDis = 0;

	for (int i = 0; i < lNum; i++)
	{
		dbfDis += abs(valAPtr[i] - valBPtr[i]) ;
	}

	return SMT_ERR_NONE;
}

long	SmtAbsoluteDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals)//绝对值距离
{
	if (vAVals.size() < 1 ||(vAVals.size() != vBVals.size()))
		return SMT_ERR_INVALID_PARAM;

	dbfDis = 0;

	for (int i = 0; i < vAVals.size(); i++)
	{
		dbfDis += abs(vAVals[i] - vBVals[i]) ;
	}

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long	SmtEuclideanDistance(double &dbfDis,const double * valAPtr,const double * valBPtr,long lNum)//欧氏距离
{
	if(NULL == valAPtr ||NULL == valBPtr|| lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	dbfDis = 0;

	for (int i = 0; i < lNum; i++)
	{
		dbfDis += (valAPtr[i] - valBPtr[i]) * (valAPtr[i] - valBPtr[i]);
	}

	dbfDis = sqrt(dbfDis);

	return SMT_ERR_NONE;
}

long	SmtEuclideanDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals)//欧氏距离
{
	if (vAVals.size() < 1 ||(vAVals.size() != vBVals.size()))
		return SMT_ERR_INVALID_PARAM;

	dbfDis = 0;

	for (int i = 0; i < vAVals.size(); i++)
	{
		dbfDis += (vAVals[i] - vBVals[i]) * (vAVals[i] - vBVals[i]);
	}

	dbfDis = sqrt(dbfDis);

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
long	SmtEuclideanDistanceExt(double &dbfDis,const double * valAPtr,const double * valBPtr,long lNum)//欧氏距离
{
	if(NULL == valAPtr ||NULL == valBPtr|| lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	dbfDis = 0;

	for (int i = 0; i < lNum; i++)
	{
		dbfDis += (valAPtr[i] - valBPtr[i]) * (valAPtr[i] - valBPtr[i]);
	}

	dbfDis = sqrt(dbfDis/lNum);

	return SMT_ERR_NONE;
}

long	SmtEuclideanDistanceExt(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals)//欧氏距离
{
	if (vAVals.size() < 1 ||(vAVals.size() != vBVals.size()))
		return SMT_ERR_INVALID_PARAM;

	dbfDis = 0;

	for (int i = 0; i < vAVals.size(); i++)
	{
		dbfDis += (vAVals[i] - vBVals[i]) * (vAVals[i] - vBVals[i]);
	}

	dbfDis = sqrt(dbfDis/vAVals.size());

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long	SmtSquaredEuclideanDistance(double &dbfDis,const double * valAPtr,const double * valBPtr,long lNum)//欧氏距离平方和
{
	if(NULL == valAPtr ||NULL == valBPtr|| lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	dbfDis = 0;

	for (int i = 0; i < lNum; i++)
	{
		dbfDis += (valAPtr[i] - valBPtr[i]) * (valAPtr[i] - valBPtr[i]);
	}

	return SMT_ERR_NONE;
}


long	SmtSquaredEuclideanDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals)//欧氏距离平方和
{
	if (vAVals.size() < 1 ||(vAVals.size() != vBVals.size()))
		return SMT_ERR_INVALID_PARAM;

	dbfDis = 0;

	for (int i = 0; i < vAVals.size(); i++)
	{
		dbfDis += (vAVals[i] - vBVals[i]) * (vAVals[i] - vBVals[i]);
	}


	return SMT_ERR_NONE;
}
//////////////////////////////////////////////////////////////////////////
long	SmtChebychevDistance(double &dbfDis,const double * valAPtr,const double * valBPtr,long lNum)//切比雪夫距离
{
	if(NULL == valAPtr ||NULL == valBPtr|| lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	double  *dtempVal = NULL;
	dtempVal = new double[lNum];
	memset(dtempVal,0,sizeof(double) * lNum);

	dbfDis = 0;

	for (int i = 0; i < lNum; i++)
	{
		dtempVal[i] = abs(valAPtr[i] - valBPtr[i]);
	}

	SmtMax(dbfDis,dtempVal,lNum);

	SMT_SAFE_DELETE_A(dtempVal);

	return SMT_ERR_NONE;
}

long	SmtChebychevDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals)//切比雪夫距离
{
	if (vAVals.size() < 1 ||(vAVals.size() != vBVals.size()))
		return SMT_ERR_INVALID_PARAM;

	vector<double> vTmpVals;
	vTmpVals.resize(vAVals.size());

	dbfDis = 0;

	for (int i = 0; i < vAVals.size(); i++)
	{
		vTmpVals[i] = abs(vAVals[i] - vBVals[i]);
	}

	SmtMax(dbfDis,vTmpVals);

	vTmpVals.clear();

	return SMT_ERR_NONE;
}
//////////////////////////////////////////////////////////////////////////

long	SmtMinkowskiDistance(double &dbfDis,const double * valAPtr,const double * valBPtr,long lNum, long lPower)//明科夫斯基距离
{
	if(NULL == valAPtr ||NULL == valBPtr|| lNum < 1)
		return SMT_ERR_INVALID_PARAM;

	dbfDis = 0;

	for (int i = 0; i < lNum; i++)
	{
		dbfDis += pow(abs(valAPtr[i] - valBPtr[i]),lPower);
	}

	dbfDis = pow(dbfDis,(1.0 / lPower));

	return SMT_ERR_NONE;
}

long	SmtMinkowskiDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals, long lPower)//明科夫斯基距离
{
	if (vAVals.size() < 1 ||(vAVals.size() != vBVals.size()))
		return SMT_ERR_INVALID_PARAM;

	dbfDis = 0;

	for (int i = 0; i < vAVals.size(); i++)
	{
		dbfDis += pow(abs(vAVals[i] - vBVals[i]),lPower);
	}

	dbfDis = pow(dbfDis,(1.0 / lPower));

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
bool    SmtIsRealEqual(double a, double b, double eps) 
{
	return (fabs(a - b) < eps);
}

long	SmtFilter(vector<double> &vVals,double dbfMin,double dbfMax)
{
	if (dbfMin >= dbfMax)
		return SMT_ERR_INVALID_PARAM;

	vector<double> vTmpVals;
	for (int i = 0; i < vVals.size(); i++)
	{
		if (vVals[i] >= dbfMin && vVals[i] <= dbfMax)
			vTmpVals.push_back(vVals[i]);
	}

	vVals.clear();
	vVals = vTmpVals;

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
long SmtExtractDoubleDat(double &dbfValue,short sPos)
{
	double tDat = (long)(dbfValue); //取整.
	double scal =pow(10.,sPos);

	dbfValue -= tDat;				//取数据的小数部分.
	dbfValue =  tDat+((long)(dbfValue*scal))/scal;

	return SMT_ERR_NONE;
}