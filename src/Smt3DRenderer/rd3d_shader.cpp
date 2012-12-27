#include "rd3d_shader.h"
#include "rd3d_3drenderdevice.h"

namespace Smt_3Drd
{
	SmtShader::SmtShader(LP3DRENDERDEVICE p3DRenderDevice, uint handle,string strName):m_unHandle(handle)
		,m_p3DRenderDevice(p3DRenderDevice)
		,m_strName(strName)
	{
		;
	}

	SmtShader::~SmtShader()
	{
		;
	}

	long SmtShader::Load(std::string fileName, bool needToCompile, ShaderCompilationFlag flags)
	{
		std::vector<char> data;

		FILE* sFile = fopen(fileName.c_str(), "rb");
		if (NULL == sFile)
		{
			return SMT_ERR_INVALID_FILE;
		}
		
		char buf[1024];
		while (!feof(sFile))
		{
			int readBytes = fread(buf, 1, sizeof(buf), sFile);
			for (int i = 0; i < readBytes; i++)
			{
				data.push_back(buf[i]);
			}
		}
		fclose(sFile);

		data.push_back(0); // To get NULL-terminated string from vector

		if (SMT_ERR_NONE !=m_p3DRenderDevice->LoadShaderSource(this, (char*)&data[0]))
			return SMT_ERR_FAILURE;
		
		if (needToCompile)
		{
			return Compile(flags);
		}

		return SMT_ERR_NONE;
	}

	long SmtShader::Compile(ShaderCompilationFlag flags)
	{
		if (SMT_ERR_NONE != m_p3DRenderDevice->CompileShader(this))
			return SMT_ERR_FAILURE;

		/* Check if there is a need to check compilation */
		if (flags & SCF_CHECK_ERRORS)
		{
			long result = IsCompiled();

			/* Check if there is a need to place errors in log file */
			if (result != SMT_ERR_NONE && (flags & SCF_LOG_ERRORS))
			{
				GetCompileLog();
			}

			return result;
		}

		return SMT_ERR_NONE;
	}

	long SmtShader::IsCompiled()
	{
		return m_p3DRenderDevice->IsShaderCompiled(this);
	}

	char* SmtShader::GetCompileLog()
	{
		return m_p3DRenderDevice->GetShaderLog(this);
	}

}
