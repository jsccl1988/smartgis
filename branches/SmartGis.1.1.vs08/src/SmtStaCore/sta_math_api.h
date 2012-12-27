/*
File:    sta_math_api.h

Desc:    SmartGis ������ѧͳ���� API

Version: Version 1.0

Writter:  �´���

Date:    2012.8.22

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _STA_MATH_API_H
#define _STA_MATH_API_H

#include "smt_core.h"
enum eSmtSortOrderType
{//���򷽷�
	SO_ASC	,					//����
	SO_DESC						//����
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
//����
//////////////////////////////////////////////////////////////////////////
long	SMT_EXPORT_API  SmtSort(double * pDbfVals,long lNum ,char so = SO_ASC);				//����
long	SMT_EXPORT_API  SmtSort(vector<double> &vVals,char so = SO_ASC);					//����

long	SMT_EXPORT_API  SmtGetValueByPercent(double &dbfResultValue,const double * pDbfVals,long lNum,double dbfPercent);			//�쳣���� pDbfVals ����������,dbfPercent%	
long	SMT_EXPORT_API  SmtGetValueByPercent(double &dbfResultValue,const vector<double> &vVals,double dbfPercent);					//�쳣���� pDbfVals ����������,dbfPercent%	

//////////////////////////////////////////////////////////////////////////
//ͳ��
//////////////////////////////////////////////////////////////////////////
long	SMT_EXPORT_API  SmtSum(double &dbfResultValue,const double * pDbfVals,long lNum);				//���
long	SMT_EXPORT_API  SmtSum(double &dbfResultValue,const vector<double> &vVals);						//���

long	SMT_EXPORT_API  SmtMax(double &dbfResultValue,const double * pDbfVals,long lNum);				//���ֵ
long	SMT_EXPORT_API  SmtMax(double &dbfResultValue,const vector<double> &vVals);						//���ֵ

long	SMT_EXPORT_API  SmtMin(double &dbfResultValue,const double * pDbfVals,long lNum);				//��Сֵ
long	SMT_EXPORT_API  SmtMin(double &dbfResultValue,const vector<double> &vVals);						//��Сֵ

long	SMT_EXPORT_API  SmtAvg(double &dbfResultValue,const double * pDbfVals,long lNum);				//ƽ��ֵ
long	SMT_EXPORT_API  SmtAvg(double &dbfResultValue,const vector<double> &vVals);						//ƽ��ֵ

long	SMT_EXPORT_API  SmtMedian(double &dbfResultValue,const double * pDbfVals,long lNum);			//��λ��
long	SMT_EXPORT_API  SmtMedian(double &dbfResultValue,const vector<double> &vVals);					//��λ��

long	SMT_EXPORT_API  SmtVariance(double &dbfResultValue,const double * pDbfVals,long lNum);				//����
long	SMT_EXPORT_API  SmtVariance(double &dbfResultValue,const vector<double> &vVals);					//����

long	SMT_EXPORT_API  SmtStdDeviation(double &dbfResultValue,const double * pDbfVals,long lNum);			//��׼��
long	SMT_EXPORT_API  SmtStdDeviation(double &dbfResultValue,const vector<double> &vVals);				//��׼��

long	SMT_EXPORT_API  SmtLogAvg(double &dbfResultValue,const double * pDbfVals,long lNum);				//����ƽ��ֵ
long	SMT_EXPORT_API  SmtLogAvg(double &dbfResultValue,const vector<double> &vVals);						//����ƽ��ֵ

//////////////////////////////////////////////////////////////////////////
//��׼��
//////////////////////////////////////////////////////////////////////////
long	SMT_EXPORT_API  SmtStandardization(double * pDbfVals,long lNum,double dbfStd);					//ָ��ֵ��׼��
long	SMT_EXPORT_API  SmtStandardization(vector<double> &vVals,double dbfStd);						//ָ��ֵ��׼��

long	SMT_EXPORT_API  SmtStdBySum(double * pDbfVals,long lNum);										//�ܺͱ�׼��
long	SMT_EXPORT_API  SmtStdBySum(vector<double> &vVals);												//�ܺͱ�׼��

long	SMT_EXPORT_API  SmtStdByBackVal(double * pDbfVals,long lNum,long lBVTye,double dbfPercent);		//����ֵ��׼��,dbfPercent,��lBVType = BV_FREQUENCY��Ч
long	SMT_EXPORT_API  SmtStdByBackVal(vector<double> &vVals,long lBVTye,double dbfPercent);			//����ֵ��׼��,dbfPercent,��lBVType = BV_FREQUENCY��Ч

long	SMT_EXPORT_API  SmtStdByDeviation(double * pDbfVals,long lNum);									//��׼���׼������Ҫ�ص�ƽ��ֵΪ0����׼��Ϊ1
long	SMT_EXPORT_API  SmtStdByDeviation(vector<double> &vVals);										//��׼���׼������Ҫ�ص�ƽ��ֵΪ0����׼��Ϊ1

long	SMT_EXPORT_API  SmtStdByMaxVal(double * pDbfVals,long lNum);									//����ֵ��׼��
long	SMT_EXPORT_API  SmtStdByMaxVal(vector<double> &vVals);											//����ֵ��׼��

long	SMT_EXPORT_API  SmtStdByMaxMin(double * pDbfVals,long lNum);									//�����׼��
long	SMT_EXPORT_API  SmtStdByMaxMin(vector<double> &vVals);											//�����׼��

long	SMT_EXPORT_API  SmtStdByMinVal(double * pDbfVals,long lNum);									//��Сֵ��׼��
long	SMT_EXPORT_API  SmtStdByMinVal(vector<double> &vVals);											//��Сֵ��׼��


//////////////////////////////////////////////////////////////////////////
//�������
//////////////////////////////////////////////////////////////////////////
//������㷽��
long	SMT_EXPORT_API  SmtAbsoluteDistance(double &dbfDis,const double * AValPtr,const double * BValPtr,long lNum);				//����ֵ����
long	SMT_EXPORT_API  SmtAbsoluteDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals);				//����ֵ����

long	SMT_EXPORT_API  SmtEuclideanDistance(double &dbfDis,const double * AValPtr,const double * BValPtr,long lNum);               //ŷ�Ͼ���
long	SMT_EXPORT_API  SmtEuclideanDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals);				//ŷ�Ͼ���

long	SMT_EXPORT_API  SmtEuclideanDistanceExt(double &dbfDis,const double * AValPtr,const double * BValPtr,long lNum);            //��չŷ�Ͼ���
long	SMT_EXPORT_API  SmtEuclideanDistanceExt(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals);			//��չŷ�Ͼ���

long	SMT_EXPORT_API  SmtSquaredEuclideanDistance(double &dbfDis,const double * AValPtr,const double * BValPtr,long lNum);		//ŷ�Ͼ���ƽ����
long	SMT_EXPORT_API  SmtSquaredEuclideanDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals);		//ŷ�Ͼ���ƽ����

long	SMT_EXPORT_API  SmtChebychevDistance(double &dbfDis,const double * AValPtr,const double * BValPtr,long lNum);               //�б�ѩ�����
long	SMT_EXPORT_API  SmtChebychevDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals);				//�б�ѩ�����

long	SMT_EXPORT_API  SmtMinkowskiDistance(double &dbfDis,const double * AValPtr,const double * BValPtr,long lNum, long lPower);  //���Ʒ�˹������
long	SMT_EXPORT_API  SmtMinkowskiDistance(double &dbfDis,const vector<double> &vAVals,const vector<double> &vBVals, long lPower);//���Ʒ�˹������

//////////////////////////////////////////////////////////////////////////
//aux 
bool	SMT_EXPORT_API	SmtIsRealEqual(double a, double b, double eps = sc_dbf_esp);
long	SMT_EXPORT_API  SmtFilter(vector<double> &vVals,double dbfMin,double dbfMax);												//�������ݣ�������������֮�������
long	SMT_EXPORT_API  SmtExtractDoubleDat(double &dbfValue,short sPos);


#if !defined(Export_SmtStaDiagram)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtStaDiagramD.lib")
#       else
#          pragma comment(lib,"SmtStaDiagram.lib")
#	    endif  
#endif

#endif //_STA_API_H
