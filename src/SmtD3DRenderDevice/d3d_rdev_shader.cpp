#include "d3d_3drenderdevice.h"
#include "smt_logmanager.h"
#include "rd3d_programmanager.h"
#include "rd3d_shadermanager.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	//vertex shader
	SmtShader* SmtD3DRenderDevice::CreateVertexShader(const char *szName)
	{
		uint newHandle;// = m_pFuncShaders->glCreateShader(GL_VERTEX_SHADER_ARB);
		SmtShader *pNewShader = new SmtShader(this,newHandle,szName);
		
		if (SMT_ERR_NONE == m_shaderMgr.AddShader(pNewShader))
			return pNewShader;
		else
		{
			SMT_SAFE_DELETE(pNewShader);
			return NULL;
		}
	}

	//pixel shader
	SmtShader* SmtD3DRenderDevice::CreatePixelShader(const char *szName)
	{		
		uint newHandle ;//= m_pFuncShaders->glCreateShader(GL_FRAGMENT_SHADER_ARB);
		SmtShader *pNewShader = new SmtShader(this, newHandle,szName);

		if (SMT_ERR_NONE == m_shaderMgr.AddShader(pNewShader))
			return pNewShader;
		else
		{
			SMT_SAFE_DELETE(pNewShader);
			return NULL;
		}
	}

	SmtShader* SmtD3DRenderDevice::GetShader(const char *szName)
	{
		return m_shaderMgr.GetShader(szName);
	}

	//program
	SmtProgram* SmtD3DRenderDevice::CreateProgram(const char *szName)
	{
		uint newHandle = m_pFuncShaders->glCreateProgram();
		SmtProgram *pNewProgram = new SmtProgram(this,newHandle,szName);

		if (SMT_ERR_NONE == m_progamMgr.AddProgram(pNewProgram))
			return pNewProgram;
		else
		{
			SMT_SAFE_DELETE(pNewProgram);
			return NULL;
		}
	}

	SmtProgram* SmtD3DRenderDevice::GetProgram(const char *szName)
	{
		return m_progamMgr.GetProgram(szName);
	}

	long SmtD3DRenderDevice::LoadShaderSource(SmtShader *shader, char* source)
	{
		uint handle = shader->GetHandle();
		m_pFuncShaders->glShaderSource(handle, 1, (const char**)&source, NULL);
		
		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::CompileShader(SmtShader *shader)
	{
		uint handle = shader->GetHandle();
		m_pFuncShaders->glCompileShader(handle);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::IsShaderCompiled(SmtShader *shader)
	{
		uint handle = shader->GetHandle();
		int compileStatus = 0;

		//m_pFuncShaders->glGetObjectParameteriv(handle, GL_OBJECT_COMPILE_STATUS_ARB, &compileStatus);
		if (compileStatus == 0)
		{
			return SMT_ERR_FAILURE;
		}

		return SMT_ERR_NONE;
	}

	char* SmtD3DRenderDevice::GetShaderLog(SmtShader *shader)
	{
		if (NULL == shader)
			return NULL;
		 
		char *log = 0;
		int logLength    = 0;
		int charsWritten = 0;
		uint handle = shader->GetHandle();

		//m_pFuncShaders->glGetObjectParameteriv(handle, GL_OBJECT_INFO_LOG_LENGTH_ARB, &logLength);

		if (logLength < 1)
			return NULL;

		log = new char[logLength];
		
		m_pFuncShaders->glGetInfoLog(handle, logLength, &charsWritten, log);

		return log;
	}

	long SmtD3DRenderDevice::DestroyShader(const char *szName)
	{
		SmtShader *shader = m_shaderMgr.GetShader(szName);
		if (NULL == shader)
			return SMT_ERR_FAILURE;

		uint handle = shader->GetHandle();
		if (handle != 0)
		{
			m_pFuncShaders->glDeleteObject(handle);
		}

		m_shaderMgr.DestroyShader(shader->GetShaderName());

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtD3DRenderDevice::BindProgram(SmtProgram *program)
	{
		if (NULL == program)
			return SMT_ERR_FAILURE;

		m_pFuncShaders->glUseProgram(program->GetHandle());

		return SMT_ERR_NONE;
	}
	
	long SmtD3DRenderDevice::UnbindProgram()
	{
		m_pFuncShaders->glUseProgram(0);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetProgramVertexShader(SmtProgram *program, SmtShader *shader)
	{
		if (NULL == program || NULL == shader)
			return SMT_ERR_FAILURE;

		m_pFuncShaders->glAttachShader(program->GetHandle(), shader->GetHandle());

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetProgramPixelShader(SmtProgram *program, SmtShader *shader)
	{
		if (NULL == program || NULL == shader)
			return SMT_ERR_FAILURE;

		m_pFuncShaders->glAttachShader(program->GetHandle(), shader->GetHandle());

		return SMT_ERR_NONE;
	}


	long SmtD3DRenderDevice::LinkProgram(SmtProgram *program)
	{
		if (NULL == program )
			return SMT_ERR_FAILURE;

		m_pFuncShaders->glLinkProgram(program->GetHandle());

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::IsProgramLinked(SmtProgram *program)
	{
		if (NULL == program )
			return SMT_ERR_FAILURE;

		int linkStatus = 0;

		//m_pFuncShaders->glGetObjectParameteriv(program->GetHandle(), GL_OBJECT_LINK_STATUS_ARB, &linkStatus);
		if (linkStatus == 0)
		{
			return SMT_ERR_FAILURE;
		}

		return SMT_ERR_NONE;
	}

	char* SmtD3DRenderDevice::GetProgramLinkLog(SmtProgram *program)
	{
		if (NULL == program )
			return NULL;

		int logLength;
		char *log = NULL;
		uint handle = program->GetHandle();
		//m_pFuncShaders->glGetObjectParameteriv(handle, GL_OBJECT_INFO_LOG_LENGTH_ARB, &logLength);

		if (logLength != 1)
		{
			log = new char[logLength];
			int charsWritten;

			m_pFuncShaders->glGetInfoLog(handle, logLength, &charsWritten, log);
		}

		return log;
	}

	long SmtD3DRenderDevice::DestroyProgram(const char *szName)
	{
		SmtProgram *program = m_progamMgr.GetProgram(szName);

		if (NULL == program )
			return SMT_ERR_FAILURE;

		uint handle = program->GetHandle();

		if (handle != 0)
		{
			m_pFuncShaders->glDeleteObject(handle);
		}

		m_progamMgr.DestroyProgram(program->GetProgramName());

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetProgramVector(SmtProgram *program, string &param, const Vector4 &value)
	{
		if (NULL == program )
			return SMT_ERR_FAILURE;

		uint handle = program->GetHandle();

		int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
		if (loc < 0)
			return SMT_ERR_FAILURE;
	 
		m_pFuncShaders->glUniform4fv(loc, 1, (const float*) &value);
	
		return SMT_ERR_NONE;
	}

	
	long SmtD3DRenderDevice::SetProgramVector(SmtProgram *program, string &param, const Vector3 &value)
	{
		if (NULL == program )
			return SMT_ERR_FAILURE;

		uint handle = program->GetHandle();

		int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
		if (loc < 0)
			return SMT_ERR_FAILURE;

		m_pFuncShaders->glUniform3fv(loc, 1, (const float*) &value);
		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetProgramVector(SmtProgram *program, string &param, const Vector2 &value)
	{
		if (NULL == program )
			return SMT_ERR_FAILURE;

		uint handle = program->GetHandle();

		int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
		if (loc < 0)
			return SMT_ERR_FAILURE;

		m_pFuncShaders->glUniform2fv(loc, 1, (const float*) &value);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetProgramFloat(SmtProgram *program, string &param, float value)
	{
		if (NULL == program )
			return SMT_ERR_FAILURE;

		uint handle = program->GetHandle();

		int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
		if (loc < 0)
			return SMT_ERR_FAILURE;

		m_pFuncShaders->glUniform1fv(loc, 1, (const float*) &value);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetProgramInt(SmtProgram *program, string &param, int value)
	{
		if (NULL == program )
			return SMT_ERR_FAILURE;

		uint handle = program->GetHandle();

		int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
		if (loc < 0)
			return SMT_ERR_FAILURE;

		m_pFuncShaders->glUniform1i(loc, value);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::GetProgramFloat(SmtProgram *program, string &param, float *value)
	{
		if (NULL == program )
			return SMT_ERR_FAILURE;

		uint handle = program->GetHandle();

		int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
		if (loc < 0)
			return SMT_ERR_FAILURE;

		m_pFuncShaders->glGetUniformfv(handle, loc, value);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SetProgramTexture(SmtProgram *program, string &param, int texture)
	{
		if (NULL == program )
			return SMT_ERR_FAILURE;

		uint handle = program->GetHandle();

		int loc = m_pFuncShaders->glGetUniformLocation(handle, param.c_str());
		if (loc < 0)
			return SMT_ERR_FAILURE;

		m_pFuncShaders->glUniform1i(loc,texture);

		return SMT_ERR_NONE;
	}
}