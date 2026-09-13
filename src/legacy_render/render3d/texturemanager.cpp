#include "legacy_render/render3d/texturemanager.h"
#include "legacy_render/render3d/3drenderdevice.h"
#include "base/core/logmanager.h"

using namespace base;


namespace render
{
	SmtTextureManager::SmtTextureManager(void)
	{
         DestroyAllTexture();
	}

	SmtTextureManager::~SmtTextureManager(void)
	{
         DestroyAllTexture();
	}

	long SmtTextureManager::AddTexture(SmtTexture*	pTexture)
	{
		if (NULL == pTexture)
			return SMT_ERR_FAILURE;
		 
		SmtLogManager * pLogMgr = SmtLogManager::get_singleton_ptr();
		SmtLog *pLog = pLogMgr->get_default_log();

		SmtTexture *pTexTmp = NULL;
		pTexTmp = GetTexture(pTexture->GetTextureName());
		if (NULL == pTexTmp)
			m_mapNameToTexturePtrs.insert(pairNameToTexturePtr(pTexture->GetTextureName(),pTexture));
		else
		{
			pLog->log_message("AddTexture () already exist");
			return SMT_ERR_FAILURE;
		}

		return SMT_ERR_NONE;
	}

	SmtTexture * SmtTextureManager::GetTexture(const char * szName)
	{
		SmtTexture * pTexture = NULL;
		mapNameToTexturePtrs::iterator mapIter;	
		mapIter = m_mapNameToTexturePtrs.find(szName);

		if (mapIter != m_mapNameToTexturePtrs.end())
		{
			pTexture = (mapIter->second);
		}

		return pTexture;
	}

	long SmtTextureManager::DestroyTexture(const char * szName)
	{
		mapNameToTexturePtrs::iterator iter = m_mapNameToTexturePtrs.find(szName) ;

		if(iter != m_mapNameToTexturePtrs.end())
		{
			SMT_SAFE_DELETE(iter->second);
			m_mapNameToTexturePtrs.erase(iter);
		}

		return SMT_ERR_NONE;
	}

	long  SmtTextureManager::DestroyAllTexture(void)
	{
		mapNameToTexturePtrs::iterator i = m_mapNameToTexturePtrs.begin() ;

		while (i != m_mapNameToTexturePtrs.end())
		{
			SMT_SAFE_DELETE(i->second);
			i++;
		}

		m_mapNameToTexturePtrs.clear();

		return SMT_ERR_NONE;
	}

	void SmtTextureManager::GetAllTextureName(vector<string> &vStrAllTextureName)
	{
		vStrAllTextureName.clear();

		mapNameToTexturePtrs::iterator iter = m_mapNameToTexturePtrs.begin() ;

		while (iter != m_mapNameToTexturePtrs.end())
		{
			vStrAllTextureName.push_back(iter->first);
			iter++;
		}
	}
}