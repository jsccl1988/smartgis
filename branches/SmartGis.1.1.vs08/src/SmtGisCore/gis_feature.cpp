#include "gis_feature.h"
#include "bl_stylemanager.h"
#include "smt_logmanager.h"

using namespace Smt_Core;
using namespace Smt_Base;

namespace Smt_GIS
{
	SmtFeature::SmtFeature(void)
	{
          m_pGeom = NULL;
		  m_pAtt = NULL;
		  m_pStyle = NULL;
	}

	SmtFeature::~SmtFeature(void)
	{
        SMT_SAFE_DELETE(m_pAtt);
		SMT_SAFE_DELETE(m_pGeom);
		SMT_SAFE_DELETE(m_pStyle);
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtFeature::SetGeometryDirectly(SmtGeometry  *pGeom)
	{
		SMT_SAFE_DELETE(m_pGeom);
		m_pGeom = pGeom;
	}

	void SmtFeature::SetGeometry( const SmtGeometry  *pGeom)
	{
		SMT_SAFE_DELETE(m_pGeom);

		if( pGeom != NULL )
			m_pGeom = pGeom->Clone();
		else
			m_pGeom = NULL;
	}

	void SmtFeature::SetAttributeDirectly(SmtAttribute  *pAtt)
	{
		SMT_SAFE_DELETE(m_pAtt);
		m_pAtt = pAtt;
	}

	void SmtFeature::SetAttribute( const SmtAttribute  *pAtt)
	{
		SMT_SAFE_DELETE(m_pAtt);
	
		if( pAtt != NULL )
			m_pAtt = pAtt->Clone();
		else
			m_pAtt = NULL;
	}

    void SmtFeature::SetFeatureType(SmtFeatureType type)
	{
		if ( m_SmtFeatureType == type )
			return;

		m_SmtFeatureType = type;
		SMT_SAFE_DELETE(m_pAtt);
		m_pAtt = new SmtAttribute();

		SmtField smtFld;
		 
		switch(m_SmtFeatureType)
		{
		case SmtFtDot:
			{
				/*	smtFld.SetName("childimage_id");
				smtFld.SetType(SmtVarType::SmtInteger);
				m_pAtt->AddField(smtFld);*/
			}
			break;
		case SmtFtAnno:
			{
				smtFld.SetName("anno");
				smtFld.SetType(SmtVarType::SmtString);
				m_pAtt->AddField(smtFld);

				smtFld.SetName("color");
				smtFld.SetType(SmtVarType::SmtInteger);
				m_pAtt->AddField(smtFld);

				smtFld.SetName("angle");
				smtFld.SetType(SmtVarType::SmtReal);
				m_pAtt->AddField(smtFld);
			}
			
			break;
		case SmtFtChildImage:
			{
			/*	smtFld.SetName("childimage_id");
				smtFld.SetType(SmtVarType::SmtInteger);
				m_pAtt->AddField(smtFld);*/
			}
			break;
		case SmtFtCurve:
			{
				smtFld.SetName("length");
				smtFld.SetType(SmtVarType::SmtReal);
				m_pAtt->AddField(smtFld);
			}
		    break;
		case SmtFtSurface:
			{
				smtFld.SetName("area");
				smtFld.SetType(SmtVarType::SmtReal);
				m_pAtt->AddField(smtFld);
			}
		    break;
		default:
		    break;
		}
	}

	SmtFeature *SmtFeature::Clone() const 
	{
       SmtFeature *pNewFeature = new SmtFeature();
	   if (pNewFeature == NULL)
		   return NULL;

	   pNewFeature->SetID(GetID()); 
	   pNewFeature->SetFeatureType(m_SmtFeatureType);
	   pNewFeature->SetGeometry(m_pGeom);
	   if (NULL != m_pAtt)
		   pNewFeature->m_pAtt = m_pAtt->Clone();
	   if (NULL != m_pStyle)
		    pNewFeature->SetStyle(m_pStyle);
	   
	   return pNewFeature;
	}

	bool  SmtFeature::Equal( const SmtFeature * poFeature )
	{
		if( poFeature == this )
			return true;

		if( GetID() != poFeature->GetID() )
			return false;

		if( GetGeometryRef() == NULL && poFeature->GetGeometryRef() != NULL )
			return false;

		if( GetGeometryRef() != NULL && poFeature->GetGeometryRef() == NULL )
			return false;

		if( GetGeometryRef() != NULL && poFeature->GetGeometryRef() != NULL 
			&& (!GetGeometryRef()->Equals( poFeature->GetGeometryRef() ) ) )
			return false;

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtFeature::AddField(SmtField &fld)
	{
		if (m_pAtt)
			m_pAtt->AddField(fld);
	}

	void SmtFeature::RemoveField(const char *szFldName)
	{
         if (m_pAtt == NULL)
			 return;
		 m_pAtt->RemoveField(szFldName);
	}

	int  SmtFeature::GetFieldIndexByName(const char *szName)
	{
		if (m_pAtt == NULL)
			return -1;

		 return m_pAtt->GetFieldIndex(szName);
	}

	void SmtFeature::SetName(int index,const char *szName)
	{
		if (m_pAtt == NULL)
			return;

	   SmtField *pField = m_pAtt->GetFieldPtr(index);
       if (NULL ==pField)
		   return;

      pField->SetName(szName);
	}

	void SmtFeature::SetType(int index,varType uVt)
	{
		if (m_pAtt == NULL)
			return;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return;
		pField->SetType(uVt);
	}

	int  SmtFeature::SetFieldValue(int index,const SmtVariant &smtFld)
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(smtFld);
	}

	int SmtFeature:: SetFieldValue(int index, int nValue )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(nValue);
	}

	int  SmtFeature::SetFieldValue(int index, double dfValue )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(dfValue);
	}

	int  SmtFeature::SetFieldValue(int index, byte bValue )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(bValue);
	}

	int  SmtFeature::SetFieldValue(int index, bool bValue )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(bValue);
	}

	int  SmtFeature::SetFieldValue(int index,const char *szValue )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(szValue);
	}

	int  SmtFeature::SetFieldValue(int index, int nCount, int *panValues )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField =m_pAtt-> GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(nCount,panValues);
	}

	int  SmtFeature::SetFieldValue(int index, int nCount, double *padfValues )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(nCount,padfValues);
	}

	int  SmtFeature::SetFieldValue(int index, char **papszValues )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(papszValues);
	}

	int  SmtFeature::SetFieldValue(int index, int nCount, byte *pabyBinary )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(nCount,pabyBinary);
	}

	int  SmtFeature::SetFieldValue(int index, int nYear, int nMonth, int nDay,int nHour,int nMinute, int nSecond, int nTZFlag)
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(nYear,nMonth,nDay,nHour,nMinute,nSecond,nTZFlag);
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtFeature::SetStyle(const char *szStyleName) 
	{
		SmtStyleManager * pStyleMgr = SmtStyleManager::GetSingletonPtr();
		if (pStyleMgr)
		{
			SmtStyle * pStyle = pStyleMgr->GetStyle(szStyleName);
			if (pStyle != NULL)
				SetStyle(pStyle);
		}

		return SMT_ERR_NONE;
	}

	int  SmtFeature::SetStyleDirectly(SmtStyle *pStyle)
	{
		if(pStyle == NULL) 
			return SMT_ERR_FAILURE;
		SMT_SAFE_DELETE(m_pStyle);
		m_pStyle = pStyle;
		return SMT_ERR_NONE;
	}

	int  SmtFeature::SetStyle(const SmtStyle *pStyle) 
	{ 
		if(pStyle == NULL) 
			return SMT_ERR_FAILURE;

		SMT_SAFE_DELETE(m_pStyle);

		m_pStyle = pStyle->Clone(pStyle->GetStyleName());

		return SMT_ERR_NONE;
	}
}