/*
File:   md3d_pointcloud.h

Desc:    Smt3DPointCloud,µãÔÆ

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.8.1

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MD3D_POINTCLOUD_H
#define _MD3D_POINTCLOUD_H

#include "rd3d_videobuffer.h"
#include "rd3d_3drenderdevice.h"
#include "bl3d_object.h"
#include "smt_core.h"
#include "bl3d_vertexocttree.h"

using namespace Smt_3DBase;

namespace Smt_3DModel
{
	class SMT_EXPORT_CLASS Smt3DPointCloud :public Smt3DObject
	{
	public:
		Smt3DPointCloud();
		virtual~Smt3DPointCloud();

	public:
		long					Init(Vector3& vPos,SmtMaterial&matMaterial);
		long					Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed); 
		long					Create(LP3DRENDERDEVICE p3DRenderDevice); 
		long					Render(LP3DRENDERDEVICE p3DRenderDevice); 
		long					Destroy();

	public:
		inline	bool			GetShowOctNodeBox(void){ return m_bShowOctNodeBox;}
		inline	void			SetShowOctNodeBox(bool bShow = true){ m_bShowOctNodeBox = bShow;}

	public:
		bool					Read3DPointCloud(const char* szFilePath);

		inline	SmtVertexOctTree &GetVertexOctTree(void) { return m_vtxOctTree;}

	private:
		SmtVertexOctTree		m_vtxOctTree;

		SmtVertexBuffer*		m_pVertexBuffer;

		SmtVertex3DList			m_vtxList;
		bool					m_bShowOctNodeBox;

		bool					m_bReadOK;
	};
}

#if !defined(Export_Smt3DPointCloud)
#if   defined( _DEBUG)
#          pragma comment(lib,"Smt3DPointCloudD.lib")
#       else
#          pragma comment(lib,"Smt3DPointCloud.lib")
#	    endif  
#endif

#endif //_MD3D_POINTCLOUD_H