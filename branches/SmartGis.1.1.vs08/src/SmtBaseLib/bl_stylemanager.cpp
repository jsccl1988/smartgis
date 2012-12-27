#include "bl_stylemanager.h"

namespace Smt_Base
{
	SmtStyleManager* SmtStyleManager::m_pSingleton = NULL;

	SmtStyleManager* SmtStyleManager::GetSingletonPtr(void)
	{
		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtStyleManager();
		}
		return m_pSingleton;
	}

	void SmtStyleManager::DestoryInstance(void)
	{
		SMT_SAFE_DELETE(m_pSingleton);
	}

	//////////////////////////////////////////////////////////////////////////
	SmtStyleManager::SmtStyleManager(void)
	{
		m_pDefaultStyle = NULL;

		DestroyAllStyle();
	}

	SmtStyleManager::~SmtStyleManager(void)
	{
		DestroyAllStyle();
	}

	//////////////////////////////////////////////////////////////////////////

	void SmtStyleManager::SetDefaultStyle(const char *   defName,
									SmtPenDesc        &penDesc,
									SmtBrushDesc      &brushDesc,
									SmtAnnotationDesc &annoDesc,
									SmtSymbolDesc     &symbolDesc)
	{
		m_pDefaultStyle = new SmtStyle(defName,penDesc,brushDesc,annoDesc,symbolDesc);
		m_StylePtrList.push_back(m_pDefaultStyle);
	}

	void SmtStyleManager::DestroyAllStyle()
	{
		StylePtrList::iterator i = m_StylePtrList.begin() ;

		while (i != m_StylePtrList.end())
		{
			SMT_SAFE_DELETE(*i);
			++i;
		}
		m_StylePtrList.clear();

	}

	SmtStyle * SmtStyleManager::CreateStyle(const char *    defName,
									SmtPenDesc        &penDesc,
									SmtBrushDesc      &brushDesc,
									SmtAnnotationDesc &annoDesc,
									SmtSymbolDesc     &symbolDesc)
	{ 
		SmtStyle *pStyle = NULL;
		pStyle = GetStyle(defName);
		if (!pStyle)
		{
			pStyle = new SmtStyle(defName,penDesc,brushDesc,annoDesc,symbolDesc);
			m_StylePtrList.push_back(pStyle);
		}

		return pStyle;
	}

	void SmtStyleManager::DestroyStyle(const char * name)
	{
		SmtStyle * pStyle = NULL;
		StylePtrList::iterator i = m_StylePtrList.begin() ;

		while (i != m_StylePtrList.end())
		{
			if ( strcmp((**i).GetStyleName(),name) == 0)
			{
				SMT_SAFE_DELETE(*i);
				m_StylePtrList.erase(i);			
				break;
			}
			++i;
		}
	}

	void SmtStyleManager::DestroyStyle(SmtStyle *pStyle)
	{
		DestroyStyle(pStyle->GetStyleName());
	}

	SmtStyle* SmtStyleManager::GetStyle(const char * name)
	{
		SmtStyle * pStyle = NULL;
		StylePtrList::iterator i = m_StylePtrList.begin() ;

		while (i != m_StylePtrList.end())
		{
			if (strcmp((**i).GetStyleName(),name) == 0)
			{
				pStyle = *i;
				break;
			}
			++i;
		}
		return pStyle;
	}

	SmtStyle* SmtStyleManager::GetDefaultStyle(void)
	{
		if (m_pDefaultStyle) return m_pDefaultStyle;
		else return NULL;
	}
}