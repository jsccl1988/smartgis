/*
File:    rd3d_3ddevicecaps.h

Desc:    Smt3DDeviceCaps,ÏÔ¿¨ÄÜÁ¦

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_3DDEVICECAPS_H
#define _RD3D_3DDEVICECAPS_H

namespace Smt_3Drd
{
	class Smt3DRenderDevice;
	typedef class Smt3DRenderDevice *LP3DRENDERDEVICE;

	class Smt3DDeviceCaps
	{
	public:
		Smt3DDeviceCaps(LP3DRENDERDEVICE p3DRenderDevice):m_p3DRenderDevice(p3DRenderDevice){};
		virtual ~Smt3DDeviceCaps(void){};

	public:
		/**
		Checks if vertical synchronization is supported by the GPU.
		@return 'true' if vertical synchronization is supported and
		'false' otherwise.
		*/
		virtual bool			IsVSyncSupported() = 0;

		/**
		Checks if anisotropic filtration is supported on GPU.
		@return true if anisotropic filtration is supported by GPU.
		@return false if anisotropic filtration is not supported by GPU.
		*/
		virtual bool			IsAnisotropySupported() = 0;

		/**
		Returns number of texture slots available on this GPU.
		@return Number of texture slots available on this GPU.
		*/
		virtual int				GetTextureSlotsCount() = 0;

		/**
		Returns maximum number of color attachments that can
		be attached to a frame buffer.
		*/
		virtual int				GetMaxColorAttachments() = 0;

		/**
		Returns max level of anisotropic filtration that is supported on the GPU.
		@return Max level of anisotropic filtration that is supported on the GPU.
		*/
		virtual float			GetMaxAnisotropy() = 0;

	protected:
		LP3DRENDERDEVICE		m_p3DRenderDevice;
	};
}

#endif //_RD3D_3DDEVICECAPS_H

