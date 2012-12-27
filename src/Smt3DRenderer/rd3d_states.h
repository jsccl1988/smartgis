/*
File:    rd3d_states.h

Desc:    SmtAlphaTestState,SmtBlendState,SmtDepthTestState

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_STATES_H
#define _RD3D_STATES_H

#include "rd3d_3drenderdefs.h"
#include "rd3d_base.h"

namespace Smt_3Drd
{
	struct SmtAlphaTestState
	{
	public:
		SmtAlphaTestState():bEnabled(false)
			,cmpFunc(CMP_GREATER)
			,fRefValue(0)
		{
			;
		}

		SmtAlphaTestState(bool _bEnabled, Comparison _cmpFunc, float _fRefValue):bEnabled(_bEnabled)
			,cmpFunc(_cmpFunc)
			,fRefValue(_fRefValue)
		{
			;
		}

	public:	
		bool				bEnabled;
		Comparison			cmpFunc;
		float				fRefValue;
	};

	struct SmtBlendState
	{
	public:
		SmtBlendState():bEnabled(false)
			,srcFactor(BF_ONE)
			,dstFactor(BF_ZERO)
		{
			;
		}

		SmtBlendState(bool _bEnabled, BlendFactor _srcFactor,BlendFactor _dstFactor):bEnabled(_bEnabled)
			,srcFactor(_srcFactor)
			,dstFactor(_dstFactor)
		{
			;
		}

	public:
		bool				bEnabled;
		BlendFactor			srcFactor;
		BlendFactor			dstFactor;
	};

	struct SmtDepthTestState
	{
	public:
		SmtDepthTestState():bEnabled(false)
			,cmpFunc(CMP_GREATER)
			,bDepthMask(false)
		{
			;
		}

		SmtDepthTestState(bool _bEnabled, Comparison _cmpFunc,bool _bDepthMask):bEnabled(_bEnabled)
			,cmpFunc(_cmpFunc)
			,bDepthMask(_bDepthMask)
		{
			;
		}

	public:
		bool				bEnabled;
		Comparison			cmpFunc;
		bool				bDepthMask;
	};

	struct SmtMatrixState
	{
	public:
		SmtMatrixState()
		{
			;
		}

		SmtMatrixState(Matrix &_worldview, Matrix &_projection):worldview(_worldview)
			,projection(_projection)
		{
			;
		}

	public:
		Matrix				worldview;
		Matrix				projection;
	};

	struct SmtGPUState
	{
	public:
		Viewport3D			viewport;
		SmtBlendState		blend;
		SmtColor			color;
		SmtAlphaTestState	alphaTest;
		SmtDepthTestState	depthTest;
		SmtMatrixState		wpmatrix;
	};
}

#endif //_RD3D_STATES_H