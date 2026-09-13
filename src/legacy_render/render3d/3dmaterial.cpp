#include "legacy_render/render3d/base.h"

namespace render
{
	SmtMaterial::SmtMaterial(void)
	{
		;
	}

	//set
	void SmtMaterial::SetDiffuseValue(const SmtColor& diffuse)
	{
        m_cDiffuse = diffuse;
	}
	
	void SmtMaterial::SetSpecularValue(const SmtColor& specular)
	{
        m_cSpecular = specular;
	}
	
	void SmtMaterial::SetAmbientValue(const SmtColor& ambient)
	{
        m_cAmbient = ambient;
	}

	void SmtMaterial::SetEmissiveValue(const SmtColor& emissive)
	{
        m_cEmissive = emissive;
	}

	void SmtMaterial::SetShininessValue(float shininess)
	{
        m_fShininess = shininess;
	}

	//get
	const SmtColor& SmtMaterial::GetDiffuseValue(void)
	{
        return m_cDiffuse;
	}
	
	const SmtColor& SmtMaterial::GetSpecularValue(void)
	{
        return m_cSpecular;
	}
	
	const SmtColor& SmtMaterial::GetAmbientValue(void)
	{
        return m_cAmbient;
	}

	const SmtColor& SmtMaterial::GetEmissiveValue(void)
	{
        return m_cEmissive;
	}

	float SmtMaterial::GetShininessValue(void)
	{
        return m_fShininess;
	}


}