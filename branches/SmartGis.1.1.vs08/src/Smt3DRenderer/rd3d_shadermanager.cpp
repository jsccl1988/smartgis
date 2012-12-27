#include "rd3d_shadermanager.h"
#include "rd3d_3drenderdevice.h"
#include "smt_logmanager.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	//////////////////////////////////////////////////////////////////////////
	SmtShaderManager::SmtShaderManager(void)
	{
         DestroyAllShader();
	}

	SmtShaderManager::~SmtShaderManager(void)
	{
         DestroyAllShader();
	}

	long SmtShaderManager::AddShader(SmtShader* pShader)
	{
		SmtShader *pShaderTmp = NULL;
		pShaderTmp = GetShader(pShader->GetShaderName());
		if (NULL == pShaderTmp)
			m_mapNameToShaderPtrs.insert(pairNameToShaderPtr(pShader->GetShaderName(),pShader));
		else
		{
			SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
			SmtLog *pLog = pLogMgr->GetDefaultLog();

			pLog->LogMessage("AddShader () already exist");

			return SMT_ERR_FAILURE;
		}

		return SMT_ERR_NONE;
	}

	SmtShader * SmtShaderManager::GetShader(const char * szName)
	{
		SmtShader * pShader = NULL;
		mapNameToShaderPtrs::iterator mapIter;	
		mapIter = m_mapNameToShaderPtrs.find(szName);

		if (mapIter != m_mapNameToShaderPtrs.end())
		{
			pShader = (mapIter->second);
		}

		return pShader;
	}

	void SmtShaderManager::DestroyShader(const char * szName)
	{
		mapNameToShaderPtrs::iterator iter = m_mapNameToShaderPtrs.find(szName) ;

		if(iter != m_mapNameToShaderPtrs.end())
		{
			SMT_SAFE_DELETE(iter->second);
			m_mapNameToShaderPtrs.erase(iter);
		}
	}

	void  SmtShaderManager::DestroyAllShader(void)
	{
		mapNameToShaderPtrs::iterator i = m_mapNameToShaderPtrs.begin() ;

		while (i != m_mapNameToShaderPtrs.end())
		{
			SMT_SAFE_DELETE(i->second);
			i++;
		}

		m_mapNameToShaderPtrs.clear();
	}

	void SmtShaderManager::GetAllShaderName(vector<string> &vStrAllShaderName)
	{
		vStrAllShaderName.clear();

		mapNameToShaderPtrs::iterator iter = m_mapNameToShaderPtrs.begin() ;

		while (iter != m_mapNameToShaderPtrs.end())
		{
			vStrAllShaderName.push_back(iter->first);
			iter++;
		}
	}
}