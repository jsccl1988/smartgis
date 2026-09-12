#include "rd3d_program.h"
#include "rd3d_3drenderdevice.h"

namespace Smt_3Drd
{
	SmtProgram::SmtProgram(LP3DRENDERDEVICE p3DRenderDevice, uint handle,string strName):m_unHandle(handle)
		,m_p3DRenderDevice(p3DRenderDevice)
		,m_strName(strName)
	{
		;
	}

	SmtProgram::~SmtProgram()
	{
		;
	}

	long SmtProgram::Use()
	{
		return m_p3DRenderDevice->BindProgram(this);
	}

	long SmtProgram::Unuse()
	{
		return m_p3DRenderDevice->UnbindProgram();
	}

	long SmtProgram::SetVertexShader(SmtShader *shader)
	{
		return m_p3DRenderDevice->SetProgramVertexShader(this, shader);
	}

	long SmtProgram::SetPixelShader(SmtShader *shader)
	{
		return m_p3DRenderDevice->SetProgramPixelShader(this, shader);
	}

	long SmtProgram::Link(ShaderCompilationFlag flags)
	{
		if (SMT_ERR_NONE != m_p3DRenderDevice->LinkProgram(this))
			return SMT_ERR_FAILURE;
		
		/* Check if there is a need to check compilation */
		if (flags & SCF_CHECK_ERRORS)
		{
			long result = IsLinked();

			/* Check if there is a need to place errors in log file */
			if (SMT_ERR_NONE != result && 
				(flags & SCF_LOG_ERRORS))
			{
				;
			}

			return result;
		}

		return SMT_ERR_NONE;
	}

	long SmtProgram::IsLinked()
	{
		return m_p3DRenderDevice->IsProgramLinked(this);
	}

	char* SmtProgram::GetLinkLog()
	{
		return m_p3DRenderDevice->GetProgramLinkLog(this);
	}

	long SmtProgram::SetFloat(string param, float value)
	{
		return m_p3DRenderDevice->SetProgramFloat(this, param, value);
	}

	long SmtProgram::GetFloat(string param, float *value)
	{
		return m_p3DRenderDevice->GetProgramFloat(this, param, value);
	}

	long SmtProgram::SetVector(string param, const Vector2 &value)
	{
		return m_p3DRenderDevice->SetProgramVector(this, param, value);
	}

	long SmtProgram::SetVector(string param, const Vector3 &value)
	{
		return m_p3DRenderDevice->SetProgramVector(this, param, value);
	}

	long SmtProgram::SetVector(string param, const Vector4 &value)
	{
		return m_p3DRenderDevice->SetProgramVector(this, param, value);
	}

	long SmtProgram::SetTexture(string param, int texture)
	{
		return m_p3DRenderDevice->SetProgramTexture(this, param, texture);
	}

	long SmtProgram::SetInt(string param, int value)
	{
		return m_p3DRenderDevice->SetProgramInt(this, param, value);
	}
}
