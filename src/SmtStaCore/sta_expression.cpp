#include "sta_valsetmgr.h"
#include "sta_expmgr.h"
#include "sta_expression.h"
#include "sta_calculator.h"

namespace Smt_StaCore
{
	SmtExpression::SmtExpression(void)
	{

	}

	SmtExpression::~SmtExpression(void)
	{

	}

	long SmtExpression::ParseExpression(string &strResult,string &strContent,const string &strExpression)
	{
		int nPos = strExpression.find('=');
		if (nPos == string::npos)
			return SMT_ERR_FUNC_INNER;

		strResult = strExpression.substr(0,nPos);
		strContent = strExpression.substr(nPos+1,strExpression.size()-nPos-1);

		return SMT_ERR_NONE;
	}

	long SmtExpression::ToExpression(string &strExpression,const string &strResult,const string &strContent)
	{
		strExpression = strResult + "=" + strContent;

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtExpression::Calculate(const vector<string> &vExpressions)
	{
		if (vExpressions.size() < 1)
			return SMT_ERR_NONE;
		 
		SmtValSetMgr	*pValSetMgr = NULL;
		SmtExpMgr		*pExpMgr = NULL;

		m_gcCalculator.Clear();
		
		string strResult,strContent,strExpression;
		for (int i = 0; i < vExpressions.size();i++)
		{
			strExpression = vExpressions[i];
			ParseExpression(strResult,strContent,strExpression);
			if (strContent == "")
				continue;

			//清理
			m_gcCalculator.Clear();
			if (SMT_ERR_NONE != PrepareOperandVal(strResult))
			{
				double		*pSrcDbfVals = NULL,*pTagDbfVals = NULL;
				long		lNum = 0;
				string		strOperand;
				//将[]去除
				SmtCalculator::ParseOperand(strOperand,strResult);

				pSrcDbfVals = new double[lNum];
				memset(pSrcDbfVals,0,sizeof(double)*lNum);
				long lFlag = pValSetMgr->AddValSetByName(pSrcDbfVals,lNum,strOperand.c_str());
				if (SMT_ERR_NONE != lFlag)
				{
					SMT_SAFE_DELETE_A(pSrcDbfVals);
					if (SMT_WRN_ALREADYEXIST != lFlag)
						return SMT_ERR_FUNC_INNER;
				}

				pTagDbfVals = new double[lNum];
				memset(pSrcDbfVals,0,sizeof(double)*lNum);
				m_gcCalculator.AddOperValsByName(pTagDbfVals,lNum,strOperand.c_str());
			}

			if (SMT_ERR_NONE != PrepareExpOperandVals(strContent))
				return SMT_ERR_FUNC_INNER; 

			if (SMT_ERR_NONE == Calculate(strResult,strContent))
			{
				strExpression = vExpressions[i];
				ParseExpression(strResult,strContent,strExpression);

				SmtExpDesc expDesc;
				expDesc.strName = strResult;
				expDesc.strContent = strContent;

				if (SMT_ERR_NONE != pExpMgr->UpdateExpDescByName(strResult,expDesc))
				{
					pExpMgr->AddExpDescByName(strResult,expDesc);
				}
			}
		}

		return SMT_ERR_NONE;
	}

	long SmtExpression::Calculate(string strExpression)
	{
		if (strExpression == "")
			return SMT_ERR_INVALID_PARAM;
		 
		SmtValSetMgr	*pValSetMgr = NULL;
		SmtExpMgr		*pExpMgr = NULL;

		m_gcCalculator.Clear();

		string strResult,strContent;
		ParseExpression(strResult,strContent,strExpression);

		//清理
		m_gcCalculator.Clear();
		if (SMT_ERR_NONE != PrepareOperandVal(strResult))
		{
			double		*pSrcDbfVals = NULL,*pTagDbfVals = NULL;
			long		lNum = 0;
			string		strOperand;
			//将[]去除
			SmtCalculator::ParseOperand(strOperand,strResult);

			pSrcDbfVals = new double[lNum];
			memset(pSrcDbfVals,0,sizeof(double)*lNum);
			long lFlag = pValSetMgr->AddValSetByName(pSrcDbfVals,lNum,strOperand.c_str());
			if (SMT_ERR_NONE != lFlag)
			{
				SMT_SAFE_DELETE_A(pSrcDbfVals);
				if (SMT_WRN_ALREADYEXIST != lFlag)
					return SMT_ERR_FUNC_INNER;
			}

			pTagDbfVals = new double[lNum];
			memset(pSrcDbfVals,0,sizeof(double)*lNum);
			m_gcCalculator.AddOperValsByName(pTagDbfVals,lNum,strOperand.c_str());
		}

		if (SMT_ERR_NONE != PrepareExpOperandVals(strContent))
			return SMT_ERR_FUNC_INNER; 

		ParseExpression(strResult,strContent,strExpression);

		if (SMT_ERR_NONE == Calculate(strResult,strContent))
		{
			SmtExpDesc expDesc;
			expDesc.strName = strResult;
			expDesc.strContent = strContent;

			if (SMT_ERR_NONE != pExpMgr->UpdateExpDescByName(strResult,expDesc))
			{
				return pExpMgr->AddExpDescByName(strResult,expDesc);
			}
		}
		else
			return SMT_ERR_FUNC_INNER;

		return SMT_ERR_NONE;
	}

	long SmtExpression::PrepareOperandVal(const string &strOperandName)
	{
		SmtValSetMgr	*pValSetMgr = NULL;
		SmtExpMgr		*pExpMgr = NULL;

		double			*pSrcDbfVals = NULL,*pTagDbfVals = NULL;
		long			lNum = 0;

		string			strOperand;
		//将[]去除
		SmtCalculator::ParseOperand(strOperand,strOperandName);

		if (SMT_ERR_NONE == pValSetMgr->GetValSetByName(pSrcDbfVals,lNum,strOperand.c_str()))
		{
			pTagDbfVals = new double[lNum];
			memcpy(pTagDbfVals,pSrcDbfVals,sizeof(double)*lNum);
			m_gcCalculator.AddOperValsByName(pTagDbfVals,lNum,strOperand.c_str());
		}
		else
			return SMT_ERR_FUNC_INNER;

		return SMT_ERR_NONE;
	}

	long SmtExpression::PrepareExpOperandVals(const string &strExp)
	{
		vector<string>	vStrOperands;

		m_gcCalculator.SetExpression(strExp.c_str());

		if (SMT_ERR_NONE == m_gcCalculator.GetOperandNames(vStrOperands))
		{
			for (int i = 0; i < vStrOperands.size();i++)
			{
				if (SMT_ERR_NONE != PrepareOperandVal(vStrOperands[i]))
					return SMT_ERR_FUNC_INNER; 
			}

			return SMT_ERR_NONE;
		}
	
		return SMT_ERR_FUNC_INNER;
	}

	long SmtExpression::Calculate(const string &strResult,const string &strExp)
	{
		SmtValSetMgr	*pValSetMgr = NULL;
		SmtExpMgr		*pExpMgr = NULL;

		double			*pRsDbfVals = NULL;
		SmtOperand	rsVal;
		long			lNum = 0;
		string			strOperand;

		//将[]去除
		SmtCalculator::ParseOperand(strOperand,strResult);

		//1.检查表达式是否正确
		//2.获取结果操作数
		//3.将结果操作数附加至结果operand
		if (SMT_ERR_NONE == m_gcCalculator.Check(strExp.c_str()) &&
			SMT_ERR_NONE == pValSetMgr->GetValSetByName(pRsDbfVals,lNum,strOperand.c_str()) &&
			SMT_ERR_NONE == rsVal.Attach(pRsDbfVals,lNum))
		{
			//设置计算式
			m_gcCalculator.SetExpression(strExp.c_str());

			//运行
			return m_gcCalculator.Run(rsVal);
		}
		
		return SMT_ERR_FUNC_INNER;
	}
}