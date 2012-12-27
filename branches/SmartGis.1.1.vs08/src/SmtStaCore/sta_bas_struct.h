/*
File:    sta_bas_struct.h

Desc:    SmartGis ͳ��ģ�� �����ṹ��

Version: Version 1.0

Writter:  �´���

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
	{//�������򻯴���
		//��׼��
		DTRT_STD_SUM,										//�ܺͱ�׼��
		DTRT_STD_MAX,										//���ֵ��׼��
		DTRT_STD_MIN,										//��Сֵ��׼��
		DTRT_STD_MEDIAN,									//��λ����׼��
		DTRT_STD_BGV,										//����ֵ��׼��
		DTRT_STD_DEVIATION,									//��׼���׼��
		DTRT_STD_MAXMIN,									//�����׼��

		//��Ԫ������
		DTRT_LN,											//ȡ��Ȼ����
		DTRT_LOG10,											//ȡ����
		DTRT_SIN,											//ȡ����
		DTRT_COS,											//ȡ����
		DTRT_TAN,											//ȡ����
		DTRT_ASIN,											//ȡ������
		DTRT_ACOS,											//ȡ������
		DTRT_ATAN,											//ȡ������
		DTRT_LINEAR,										//���Ա任
	};

	enum eSmtDistMtd
	{//���빫ʽ
		DIS_ABSOLUTE,										//����ֵ����
		DIS_EUCLIDEAN,										//ŷ�Ͼ���
		DIS_EUCLIDEAN_EXT,									//ŷ�Ͼ���(��չ)
		DIS_SQUAREDEUCLIDEAN,								//ŷ�Ͼ���ƽ����
		DIS_CHEBYCHEV,										//�б�ѩ�����
		DIS_MINKOWSKI,										//���Ʒ�˹������
	};

	enum eSmtStaticMtd
	{//ͳ������
		DTSTA_MAX				= 1<<0,						//���ֵ
		DTSTA_MIN				= 1<<1,						//��Сֵ
		DTSTA_AVG				= 1<<2,						//ƽ��ֵ
		DTSTA_MEDIAN			= 1<<3,						//��λ��
		DTSTA_STDDEV			= 1<<4,						//��׼��
		DTSTA_VARCOEF			= 1<<5,						//����ϵ��
		DTSTA_VARIANCE			= 1<<6,						//����
	};

	const	long c_long_dtsta_all = DTSTA_MAX|DTSTA_MIN|DTSTA_AVG|DTSTA_MEDIAN|\
		DTSTA_STDDEV|DTSTA_VARCOEF;

	const	long c_long_dtsta_basic = DTSTA_MAX|DTSTA_MIN|DTSTA_AVG|DTSTA_MEDIAN|DTSTA_VARIANCE|DTSTA_STDDEV;


	//ͳ�ƽ��
	struct SmtStaticResult
	{
		double						dbfMax;					//���ֵ
		double						dbfMin;					//��Сֵ
		double						dbfAvg;					//ƽ��ֵ
		double						dbfMedian;				//��λ��
		double						dbfVariance;			//����
		double						dbfStdDeviation;		//��׼��

		double						dbfVarcoeff;			//����ϵ��

		eSmtStaticMtd				dtstaType;				//ͳ����
		long						lStaDataNum;			//���ݸ���

		SmtStaticResult():dtstaType((eSmtStaticMtd)c_long_dtsta_basic)
			,lStaDataNum(0)
		{
			dbfMax = dbfMin = dbfAvg = dbfMedian = dbfVariance = dbfStdDeviation = 0;
			dbfVarcoeff = 0;
		}
	};

	struct SmtExpDesc 
	{//���ʽ����
		string		strName;								//���ʽ���ƣ����������
		string		strContent;								//���ʽ����
		string		strInfo;								//���ʽ��Ϣ
		int			nPri;									//���ʽ���ȼ� ԽС���ȼ�Խ��

		/*--------------------------------------------------------------------
		������	�������ʽ
		����:	
		���:
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	�磺strExpression Ϊ"name=content",�� strName = "name"��strContent = "content"
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
		������	��ϱ��ʽ
		����:	
		���:	
		����:	long										������룬�� SMT_ERR_NONE����ʾ�ɹ�
		˵��:	�磺strName = "name"��strContent = "content"��strExpression Ϊ"name=content"
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