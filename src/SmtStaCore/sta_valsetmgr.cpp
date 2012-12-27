#include "sta_valsetmgr.h"

namespace Smt_StaCore
{
	//////////////////////////////////////////////////////////////////////////
	//数据集管理
	SmtValSetMgr::SmtValSetMgr(void)
	{
		Clear();
	}

	SmtValSetMgr::~SmtValSetMgr(void)
	{
		Clear();
	}

	long SmtValSetMgr::Clear()
	{
		//释放内存
		map<string,double*>::iterator iterDbfValsPtr;

		iterDbfValsPtr = m_nameToDbfValSetPtrs.begin();
		while (iterDbfValsPtr != m_nameToDbfValSetPtrs.end())
		{
			SMT_SAFE_DELETE_A(iterDbfValsPtr->second);
			iterDbfValsPtr++;
		}

		m_nameToDbfValSetPtrs.clear();

		m_nameToDbfValSetNums.clear();

		return SMT_ERR_NONE;
	}

	//获取所有数据集名称
	long SmtValSetMgr::GetAllValSetNames(vector<string> &vValSetNames)
	{
		map<string,double*>::iterator iter = m_nameToDbfValSetPtrs.begin();
		while(iter != m_nameToDbfValSetPtrs.end())
		{
			vValSetNames.push_back(iter->first);
			iter++;
		}
		return SMT_ERR_NONE;
	}

	//添加操作数
	long SmtValSetMgr::AddValSetByName(double *&pDbfVals ,long lNum,const char *szName)
	{
		map<string,double*>::iterator iter,iterEle;
		//先找数据集
		iter = m_nameToDbfValSetPtrs.find(szName);
		if (iter != m_nameToDbfValSetPtrs.end() && m_nameToDbfValSetNums[szName] == lNum)
		{
			double *pTarDbf = iter->second;
			memcpy(pTarDbf,pDbfVals,sizeof(double)*lNum);
			return SMT_WRN_ALREADYEXIST;
		}

		m_nameToDbfValSetPtrs.insert(map<string,double*>::value_type(szName,pDbfVals));
		m_nameToDbfValSetNums.insert(map<string,long>::value_type(szName,lNum));

		return SMT_ERR_NONE;
	}

	//获取操作数
	long SmtValSetMgr::GetValSetByName(double *&pDbfVals ,long &lNum,const char *szName)
	{
		map<string,double*>::iterator iter;

		//先找数据集
		iter = m_nameToDbfValSetPtrs.find(szName);
		if (iter != m_nameToDbfValSetPtrs.end())
		{
			pDbfVals = iter->second;
			lNum = m_nameToDbfValSetNums[szName];
		}
		else 
			return SMT_ERR_FUNC_INNER;

		return SMT_ERR_NONE;
	}

	long SmtValSetMgr::GetValSetByName(const double *&pDbfVals ,long &lNum,const char *szName) const
	{
		map<string,double*>::const_iterator iter;

		//先找数据集
		iter = m_nameToDbfValSetPtrs.find(szName);
		if (iter != m_nameToDbfValSetPtrs.end())
		{
			map<string,long>::const_iterator numIter;
			numIter = m_nameToDbfValSetNums.find(szName);

			pDbfVals = iter->second;
			lNum = numIter->second;
		}
		else 
			return SMT_ERR_FUNC_INNER;

		return SMT_ERR_NONE;
	}

	//移除操作数
	long SmtValSetMgr::RemoveValSetByName(double *&pDbfVals ,long &lNum,const char *szName)
	{
		map<string,double*>::iterator iter;
		iter = m_nameToDbfValSetPtrs.find(szName);
		if (iter == m_nameToDbfValSetPtrs.end())
			return SMT_ERR_FUNC_INNER;

		pDbfVals = iter->second;
		lNum = m_nameToDbfValSetNums[szName];
		m_nameToDbfValSetPtrs.erase(iter);

		map<string,long>::iterator iter1;
		iter1 = m_nameToDbfValSetNums.find(szName);
		m_nameToDbfValSetNums.erase(iter1);

		return SMT_ERR_NONE;
	}

	//删除操作数
	long SmtValSetMgr::DeleteValSetByName(const char *szName)
	{
		map<string,double*>::iterator iter;
		iter = m_nameToDbfValSetPtrs.find(szName);
		if (iter == m_nameToDbfValSetPtrs.end())
			return SMT_ERR_FUNC_INNER;

		SMT_SAFE_DELETE_A(iter->second);
		m_nameToDbfValSetPtrs.erase(iter);
		return SMT_ERR_NONE;
	}
}