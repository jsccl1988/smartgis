/*
File:    d3d_vbofuncimp.h 

Desc:    

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _VBO_FUNCSIMP_H
#define _VBO_FUNCSIMP_H

#include "d3d_vbofunc.h"
#include "d3d_prerequisites.h"

namespace Smt_3Drd
{
	class SmtVBOFuncImpl : public SmtVBOFunc
	{
	public:
		SmtVBOFuncImpl();
		virtual ~SmtVBOFuncImpl();

	public:
		virtual long						Initialize(LPGLRENDERDEVICE pGLRenderDevice);

	public:
		virtual void						glGenBuffers(int count, uint *handle);
		virtual void						glDeleteBuffers(int count, const uint *handle);
		virtual void						glBindBuffer(uint target, uint handle);
		virtual void						glBufferData(uint target, int size, void *data, uint method);
		virtual void*						glMapBuffer(uint target, uint access);
		virtual uchar						glUnmapBuffer(uint target);
		virtual void						glGetBufferParameteriv(uint target, uint param, int *value);
	};
}

#endif //_VBO_FUNCSIMP_H
