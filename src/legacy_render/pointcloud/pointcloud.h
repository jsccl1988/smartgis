/*
File:   md3d_pointcloud.h

Desc:    Smt3DPointCloud,����

Version: Version 1.0

Writter:  �´���

Date:    2012.8.1

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MD3D_POINTCLOUD_H
#define _MD3D_POINTCLOUD_H

#include "legacy_render/render3d/videobuffer.h"
#include "legacy_render/render3d/3drenderdevice.h"
#include "legacy_render/scene3d/bl3d_object.h"
#include "base/core/core.h"
#include "legacy_render/scene3d/bl3d_vertexocttree.h"

#if !defined(POINTCLOUD_EXPORT_DEFINED)
#define POINTCLOUD_EXPORT_DEFINED
#if defined(POINTCLOUD_EXPORTS)
#define POINTCLOUD_EXPORT_API __declspec(dllexport)
#define POINTCLOUD_EXPORT_CLASS __declspec(dllexport)
#else
#define POINTCLOUD_EXPORT_API __declspec(dllimport)
#define POINTCLOUD_EXPORT_CLASS __declspec(dllimport)
#endif
#endif

using namespace render;

namespace render
{
	class POINTCLOUD_EXPORT_CLASS Smt3DPointCloud :public Smt3DObject
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

#if !defined(POINTCLOUD_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"pointcloudD.lib")
#       else
#          pragma comment(lib,"pointcloud.lib")
#	    endif  
#endif

#endif //_MD3D_POINTCLOUD_H