#include "sta_expmgr.h"

namespace Smt_StaCore
{
	//////////////////////////////////////////////////////////////////////////
	//表达式管理
	SmtExpMgr::SmtExpMgr(void)
	{

	}

	SmtExpMgr::~SmtExpMgr(void)
	{
		;
	}

	//////////////////////////////////////////////////////////////////////////
	//获取所有表达式名称
	long SmtExpMgr::GetAllExpNames(vector<string> &vExpNames)  const
	{
		map<string,SmtExpDesc>:: const_iterator iter = m_nameToExpDescs.begin();
		while(iter != m_nameToExpDescs.end())
		{
			vExpNames.push_back(iter->first);
			iter++;
		}
		return SMT_ERR_NONE;
	}

	//获取所有表达式名称及表达式内容
	long SmtExpMgr::GetAllExpDescs(vector<SmtExpDesc> &vExpDescs)  const
	{
		map<string,SmtExpDesc>:: const_iterator iter = m_nameToExpDescs.begin();
		while(iter != m_nameToExpDescs.end())
		{
			vExpDescs.push_back(iter->second);
			iter++;
		}
		return SMT_ERR_NONE;
	}

	//获取所有组合表达式，"name = content"
	long SmtExpMgr::GetAllExpressions(vector<string> &vExpression)  const
	{
		string temp = "";
		map<string,SmtExpDesc>:: const_iterator iter = m_nameToExpDescs.begin();
		while(iter != m_nameToExpDescs.end())
		{
			SmtExpDesc::ToExpression(temp,iter->second.strName,iter->second.strContent);
			vExpression.push_back(temp);
			iter++;
		}
		return SMT_ERR_NONE;
	}

	//添加表达式
	long SmtExpMgr::AddExpDescByName(string strName,const SmtExpDesc &expDesc,bool bOnlyMem)
	{
		SmtExpDesc expDesc0;
		expDesc0 = expDesc;
		
		map<string,SmtExpDesc>::iterator iter;
		iter = m_nameToExpDescs.find(strName);
		if (iter != m_nameToExpDescs.end())
		{
			iter->second = expDesc0;
		}
		else
		{
			m_nameToExpDescs.insert(map<string,SmtExpDesc>::value_type(strName,expDesc0));
		}

		return SMT_ERR_NONE;
	}

	long SmtExpMgr::UpdateExpDescByName(string strName,const SmtExpDesc &expDesc)
	{
		map<string,SmtExpDesc>::iterator iter;
		iter = m_nameToExpDescs.find(strName);
		if (iter != m_nameToExpDescs.end())
		{
			iter->second = expDesc;
		}

		return SMT_ERR_NONE;
	}

	//获取表达式
	long SmtExpMgr::GetExpDescByName(string strName,SmtExpDesc &expDesc)  const
	{
		map<string,SmtExpDesc>:: const_iterator iter;
		iter = m_nameToExpDescs.find(strName);
		if (iter == m_nameToExpDescs.end())
			return SMT_ERR_FUNC_INNER;

		expDesc = iter->second;

		return SMT_ERR_NONE;
	}

	//删除表达式
	long SmtExpMgr::DeleteExpDescByName(string strName,SmtExpDesc &expDesc)
	{
		map<string,SmtExpDesc>::iterator iter;
		iter = m_nameToExpDescs.find(strName);
		if (iter == m_nameToExpDescs.end())
			return SMT_ERR_FUNC_INNER;

		expDesc = iter->second;
		m_nameToExpDescs.erase(iter);
		return SMT_ERR_NONE;
	}
}