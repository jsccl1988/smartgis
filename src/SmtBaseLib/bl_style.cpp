#include "bl_style.h"

namespace Smt_Base
{
	//////////////////////////////////////////////////////////////////////////
	SmtStyle::SmtStyle(void)
	{
       strcpy(m_szName,"Default");
	   m_stType = ST_PenDesc;
	}

	SmtStyle::SmtStyle(const char		*szName,
				const SmtPenDesc        &penDesc,
				const SmtBrushDesc      &brushDesc,
				const SmtAnnotationDesc &annoDesc,
				const SmtSymbolDesc     &symbolDesc)
	: m_stPenDesc(penDesc),m_stBrushDesc(brushDesc),m_stAnnoDesc(annoDesc),m_stSymbolDesc(symbolDesc)
	{
        SetStyleName(szName);
	}

	SmtStyle::~SmtStyle()
	{

	}

	SmtStyle *SmtStyle::Clone(const char *szNewName)const 
	{
		SmtStyle * pNewStyle = new SmtStyle(szNewName,m_stPenDesc,m_stBrushDesc,m_stAnnoDesc,m_stSymbolDesc);
		if (pNewStyle != NULL)
		{
			pNewStyle->SetStyleType(m_stType);
			return pNewStyle;
		}
		else 
			return NULL;
	}
}