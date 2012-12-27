#include "rd3d_statesmanager.h"

namespace Smt_3Drd
{
	SmtGPUState* SmtGPUStateManager::GetState()
	{
		SmtGPUState *newState = new SmtGPUState();
		newState->viewport = GetViewportState();
		newState->color = GetColorState();
		newState->blend = GetBlendState();
		newState->alphaTest = GetAlphaTestState();
		newState->depthTest = GetDepthTestState();
		return newState;
	}


	void SmtGPUStateManager::PushStates(uint flags)
	{
		/* Rasterizer states */
		if (flags & ALPHA_TEST_STATE)
		{
			alphaStack.push(GetAlphaTestState());
		}

		if (flags & DEPTH_TEST_STATE)
		{
			depthStack.push(GetDepthTestState());
		}

		if (flags & BLEND_STATE)
		{
			blendStack.push(GetBlendState());
		}

		if (flags & COLOR_STATE)
		{
			colorStack.push(GetColorState());
		}

		/* Transform states */
		if (flags & VIEWPORT_STATE)
		{
			viewportStack.push(GetViewportState());
		}

		if (flags & MATRIX_STATE)
		{
			matrixStack.push(GetMatrixState());
		}
	}

	void SmtGPUStateManager::PopStates(uint flags)
	{
		/* Rasterizer states */
		if (flags & ALPHA_TEST_STATE)
		{
			SetAlphaTestState(alphaStack.top());
			alphaStack.pop();
		}

		if (flags & DEPTH_TEST_STATE)
		{
			SetDepthTestState(depthStack.top());
			depthStack.pop();
		}

		if (flags & BLEND_STATE)
		{
			SetBlendState(blendStack.top());
			blendStack.pop();
		}

		if (flags & COLOR_STATE)
		{
			SetColorState(colorStack.top());
			colorStack.pop();
		}

		/* Transform states */
		if (flags & VIEWPORT_STATE)
		{
			SetViewportState(viewportStack.top());
			viewportStack.pop();
		}

		if (flags & MATRIX_STATE)
		{
			SetMatrixState(matrixStack.top());
			matrixStack.pop();
		}
	}

	long SmtGPUStateManager::SetViewport(int x, int y, int width, int height)
	{
		return SetViewportState(Viewport3D(x, y, width, height,0,0,0));
	}

	//clr
	long SmtGPUStateManager::SetColor(float red, float green, float blue, float alpha)
	{
		return SetColorState(SmtColor(red, green, blue, alpha));
	}

	//matrix
	SmtMatrixState SmtGPUStateManager::GetMatrixState()
	{
		return SmtMatrixState(GetWorldViewMatrix(), GetProjectionMatrix());
	}

	long SmtGPUStateManager::SetMatrixState(SmtMatrixState& state)
	{
		SetWorldViewMatrix(state.worldview);
		SetProjectionMatrix(state.projection);

		return SMT_ERR_NONE;
	}
}

