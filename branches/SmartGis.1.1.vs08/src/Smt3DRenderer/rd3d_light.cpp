#include "rd3d_base.h"

namespace Smt_3Drd
{
	SmtLight::SmtLight(void)
	{
		m_fCutoffAngle = 180.f;
		m_fExponent = 0.0f;
		m_fAttenuationConstant = 1.f;
		m_fAttenuationLinear = 0.f;
		m_fAttenuationQuadric = 0.f;
		m_fRange  = 1000;
		m_fThetaAngle = 180;
		m_fPhiAngle = 45;
	}

	//set
	void SmtLight::SetType(LIGHTTYPE type)
	{
		m_Type = type;
	}

	void SmtLight::SetDiffuseValue(const SmtColor& diffuse)
	{
        m_cDiffuse = diffuse;
	}

	void SmtLight::SetSpecularValue(const SmtColor& specular)
	{
        m_cSpecular = specular;
	}

	void SmtLight::SetAmbientValue(const SmtColor& ambient)
	{
        m_cAmbient = ambient;
	}
	


	void SmtLight::SetPoistion(const Vector4& position)
	{ 
        m_vPosition = position;
	}

	void SmtLight::SetDirection(const Vector4& direction)
	{
        m_vDirection = direction;
	}
	
	//get

	LIGHTTYPE SmtLight::GetType()
	{
       return m_Type;
	}

	const SmtColor& SmtLight::GetDiffuseValue(void)
	{
        return m_cDiffuse;
	}

	const SmtColor& SmtLight::GetSpecularValue(void)
	{
        return m_cSpecular;
	}

	const SmtColor& SmtLight::GetAmbientValue(void)
	{
        return m_cAmbient;
	}
	

	const Vector4& SmtLight::GetPosition(void)
	{
        return m_vPosition;
	}

	const Vector4& SmtLight::GetDirection(void)
	{
        return m_vDirection;
	}
}