#include "gis_3dfeature.h"
#include "smt_logmanager.h"
#include "bl_stylemanager.h"

using namespace Smt_Core;
using namespace Smt_3DGeo;
using namespace Smt_Base;

namespace Smt_GIS
{
	Smt3DFeature::Smt3DFeature(void)
	{
          m_pGeom = NULL;
		  m_pAtt = NULL;
		  m_pMaterial = NULL;
	}

	Smt3DFeature::~Smt3DFeature(void)
	{
        SMT_SAFE_DELETE(m_pAtt);
		SMT_SAFE_DELETE(m_pGeom);
	}

	//////////////////////////////////////////////////////////////////////////
	void Smt3DFeature::SetGeometryDirectly(Smt3DGeometry  *pGeom)
	{
		SMT_SAFE_DELETE(m_pGeom);
		m_pGeom = pGeom;
	}

    void Smt3DFeature::SetFeatureType(Smt3DFeatureType type)
	{
		if ( m_SmtFeatureType == type )
			return;

		m_SmtFeatureType = type;
		SMT_SAFE_DELETE(m_pAtt);
		m_pAtt = new SmtAttribute();

		SmtField smtFld;
		 
		switch(m_SmtFeatureType)
		{
		case SmtFt3DDot:
			{
				/*	smtFld.SetName("childimage_id");
				smtFld.SetType(SmtVarType::SmtInteger);
				m_pAtt->AddField(smtFld);*/
			}
			break;
		case SmtFt3DAnno:
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
		case SmtFt3DCurve:
			{
				smtFld.SetName("length");
				smtFld.SetType(SmtVarType::SmtReal);
				m_pAtt->AddField(smtFld);
			}
		    break;
		case SmtFt3DSurface:
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

	void Smt3DFeature::SetGeometry(Smt3DGeometry  *pGeom)
	{
		SMT_SAFE_DELETE(m_pGeom);

		if( pGeom != NULL )
			m_pGeom = pGeom->Clone();
		else
			m_pGeom = NULL;
	}

	Smt3DFeature *Smt3DFeature::Clone()
	{
       Smt3DFeature *pNewFeature = new Smt3DFeature();
	   if (pNewFeature == NULL)
		   return NULL;

	   pNewFeature->SetID(GetID()); 
	   pNewFeature->SetFeatureType(m_SmtFeatureType);
	   pNewFeature->SetGeometry(m_pGeom);
	   if (NULL != m_pAtt)
		   pNewFeature->m_pAtt = m_pAtt->Clone();
	   
	   return pNewFeature;
	}

	bool  Smt3DFeature::Equal( Smt3DFeature * poFeature )
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
	void Smt3DFeature::AddField(SmtField & fld)
	{
		if (m_pAtt)
			m_pAtt->AddField(fld);
	}

	void Smt3DFeature::RemoveField(const char * fldname)
	{
         if (m_pAtt == NULL)
			 return;
		 m_pAtt->RemoveField(fldname);
	}

	int  Smt3DFeature::GetFieldIndexByName(const char * szFldName)
	{
		if (m_pAtt == NULL)
			return -1;

		 return m_pAtt->GetFieldIndex(szFldName);
	}

	void Smt3DFeature::SetName(int index,const char * szName)
	{
		if (m_pAtt == NULL)
			return;

	   SmtField *pField = m_pAtt->GetFieldPtr(index);
       if (NULL ==pField)
		   return;

      pField->SetName(szName);
	}

	void Smt3DFeature::SetType(int index,varType uVt)
	{
		if (m_pAtt == NULL)
			return;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return;
		pField->SetType(uVt);
	}

	int  Smt3DFeature::SetFieldValue(int index,const SmtVariant &smtFld)
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(smtFld);
	}

	int Smt3DFeature:: SetFieldValue(int index, int nValue )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(nValue);
	}

	int  Smt3DFeature::SetFieldValue(int index, double dfValue )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(dfValue);
	}

	int  Smt3DFeature::SetFieldValue(int index, byte bValue )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(bValue);
	}

	int  Smt3DFeature::SetFieldValue(int index, bool bValue )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(bValue);
	}

	int  Smt3DFeature::SetFieldValue(int index,const char * pszValue )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(pszValue);
	}

	int  Smt3DFeature::SetFieldValue(int index, int nCount, int * panValues )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField =m_pAtt-> GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(nCount,panValues);
	}

	int  Smt3DFeature::SetFieldValue(int index, int nCount, double * padfValues )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(nCount,padfValues);
	}

	int  Smt3DFeature::SetFieldValue(int index, char ** papszValues )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(papszValues);
	}

	int  Smt3DFeature::SetFieldValue(int index, int nCount, byte * pabyBinary )
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(nCount,pabyBinary);
	}

	int  Smt3DFeature::SetFieldValue(int index, int nYear, int nMonth, int nDay,int nHour,int nMinute, int nSecond, int nTZFlag)
	{
		if (m_pAtt == NULL)
			return SMT_ERR_FAILURE;

		SmtField *pField = m_pAtt->GetFieldPtr(index);
		if (NULL ==pField)
			return SMT_ERR_FAILURE;

		return pField->SetValue(nYear,nMonth,nDay,nHour,nMinute,nSecond,nTZFlag);
	}

	//////////////////////////////////////////////////////////////////////////
	int Smt3DFeature::SetMaterial(const char * stylename) 
	{
		SmtStyleManager * pStyleMgr = SmtStyleManager::GetSingletonPtr();
		if (pStyleMgr)
		{
		/*	SmtMaterial * pMaterial = pStyleMgr->GetStyle(stylename);
			if (pMaterial != NULL)
				SetMaterial(pMaterial);*/
		}

		return SMT_ERR_NONE;
	}

	int  Smt3DFeature::SetMaterialDirectly(SmtMaterial * pMaterial)
	{
		if(pMaterial == NULL) 
			return SMT_ERR_FAILURE;
		SMT_SAFE_DELETE(m_pMaterial);

		m_pMaterial = pMaterial;
		return SMT_ERR_NONE;
	}

	int  Smt3DFeature::SetMaterial(SmtMaterial * pMaterial) 
	{ 
		if(pMaterial == NULL) 
			return SMT_ERR_FAILURE;
		SMT_SAFE_DELETE(m_pMaterial);

		m_pMaterial = new SmtMaterial();
		*m_pMaterial = *pMaterial;
		return SMT_ERR_NONE;
	}
}