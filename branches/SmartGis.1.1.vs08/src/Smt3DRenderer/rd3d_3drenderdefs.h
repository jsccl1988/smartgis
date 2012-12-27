/*
File:    rd3d_3drenderdefs.h

Desc:    基本定义

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _RD3D_3DRENDERDEFS_H
#define _RD3D_3DRENDERDEFS_H

#include "smt_core.h"
#include "smt_bas_struct.h"
using namespace Smt_Core;

namespace Smt_3Drd
{
	//Base Api used to render primitives
	enum RenderBase3DApi
	{
		RA_OPENGL,
		RA_D3D09
	};

	enum Type
	{
		SMT_SHORT = 0,
		SMT_INT,
		SMT_FLOAT,
		SMT_DOUBLE,
		SMT_UNSIGNED_INT,
		SMT_UNSIGNED_BYTE,
		SMT_UNSIGNED_SHORT
	};

	enum MatrixMode
	{
		MM_PROJECTION ,
		MM_MODELVIEW  
	};

	//List of the render states that can be set on a device.
	enum RenderStateType 
	{
		RS_CULLINGENABLE,				
		RS_ZENABLE,	
		RS_ALPHAENABLE,
		RS_BLENDENABLE,
		RS_STENCILENABLE,

		RS_FOGENABLE,
		RS_LIGHTINGENABLE,
		RS_MATERIALCOLORENABLE,	
		RS_TEXTUREENABLE,

		//////////////////////////////////////////////////////////////////////////
		RS_CULLMODE,
		RS_FILLMODE,
		RS_ZFUNC,
		RS_ALPHAFUNC,
		RS_BLENDFUCN,
		RS_STENCILFUNC,
		RS_FOGMODE,

		RS_POINTSIZE,
		RS_LINEWIDTH,
	} ;

	enum RenderStateValue
	{
		//cullmode
		RSV_CULL_CW,          // cull clockwise ordered triangles
		RSV_CULL_CCW,         // cull counter cw ordered triangles
		RSV_CULL_NONE,        // render front- and backsides

		//depth
		RSV_DEPTH_READWRITE,  // read and write depth buffer
		RSV_DEPTH_READONLY,   // read but don't write depth buffer
		RSV_DEPTH_NONE,       // no read or write with depth buffer

		//fillmode
		RSV_SHADE_POINTS,     // render just vertices
		RSV_SHADE_LINES,      // render two verts as one line
		RSV_SHADE_TRIWIRE,    // render triangulated wire
		RSV_SHADE_HULLWIRE,   // render poly hull as polyline
		RSV_SHADE_SOLID,      // render solid polygons

		//zfunc or alphafunc 
		RSV_CMP_NEVER,
		RSV_CMP_LESS,
		RSV_CMP_EQUAL,
		RSV_CMP_LESSEQUAL,
		RSV_CMP_GREATER,
		RSV_CMP_NOTEQUAL,
		RSV_CMP_GREATEREQUAL,
		RSV_CMP_ALWAYS,

		//blendfunc
		//blend src
		RSV_DST_COLOR ,
		RSV_ONE_MINUS_DST_COLOR,
		RSV_SRC_ALPHA_SATURATE,
		//blend tar
		RSV_ZERO,
		RSV_ONE ,
		RSV_SRC_COLOR,
		RSV_ONE_MINUS_SRC_COLOR,
		RSV_SRC_ALPHA,
		RSV_ONE_MINUS_SRC_ALPHA,
		RSV_DST_ALPHA,
		RSV_ONE_MINUS_DST_ALPHA ,

		//stencilfunc
		RSV_STENCIL_MASK,           // stencil mask
		RSV_STENCIL_WRITEMASK,      // stencil write mask
		RSV_STENCIL_REF,            // reference value
		RSV_STENCIL_FAIL_DECR,      // stencil fail decrements
		RSV_STENCIL_FAIL_INCR,      // stencil fail increments
		RSV_STENCIL_FAIL_KEEP,      // stencil fail keeps
		RSV_STENCIL_ZFAIL_DECR,     // stencil pass but z fail decrements
		RSV_STENCIL_ZFAIL_INCR,     // stencil pass but z fail increments
		RSV_STENCIL_ZFAIL_KEEP,     // stencil pass but z fail keeps
		RSV_STENCIL_PASS_DECR,      // stencil and z pass decrements
		RSV_STENCIL_PASS_INCR,      // stencil and z pass increments
		RSV_STENCIL_PASS_KEEP,      // stencil and z pass keeps
		RSV_DEPTHBIAS,              // bis value to add to depth value

		//fog mode
		RSV_LINEAR,
		RSV_EXP,
		RSV_EXP2
	};

	//Flags used to describe the format of a vertex in a vertex buffer
	enum VetexFormatFlags
	{
		VF_XYZ           = 1, /** Untransformed XYZ value set. */
		VF_XYZRHW        = 1<<1, /** transformed XYZRHW value set. */
		VF_NORMAL        = 1<<2, /** XYZ normal value set */
		VF_DIFFUSE       = 1<<3, /** RGBA diffuse color value set */
		VF_TEXCOORD      = 1<<4, /** Texture vertex flag. */
		VF_VERTEXINDEX	 = 1<<5	 /** vertex index flag. */
	};

	//Supported primitive types
	enum PrimitiveType
	{
		PT_POINTLIST,     /** Specifies a point list  */
		PT_LINELIST,      /** Specifies a line list   */
		PT_LINESTRIP,     /** Specifies a line strip  */
		PT_TRIANGLELIST,  /** Specifies a triangle list  */
		PT_TRIANGLESTRIP, /** Specifies a triangle strip */
		PT_TRIANGLEFAN,   /** Specifies a triangle fan   */
	};

	//Valid clear flags
	enum ClearFlag
	{
		CLR_COLOR   = 0x0001, /** Clear color buffer */
		CLR_ZBUFFER = 0x0002, /** Clear z-buffer */
		CLR_STENCIL = 0x0004, /** Clear stencil buffer */
	};

	enum PolygonMode
	{
		PM_POINT,
		PM_LINE,
		PM_FILL
	};

	enum FogMode
	{
		FM_EXP,
		FM_EXP2,
		FM_LINEAR,
		FM_NONE
	};

	enum RenderMode
	{
		RM_RENDER		= 1,	//render
		RM_FEEDBACK		= 2,	//feedback
		RM_SELECT		= 3,	//select
	};

	enum BlendFactor
	{
		BF_ZERO = 0,
		BF_ONE,
		BF_SRC_COLOR,
		BF_DST_COLOR,
		BF_ONE_MINUS_DST_COLOR,
		BF_SRC_ALPHA,
		BF_ONE_MINUS_SRC_ALPHA,
		BF_DST_ALPHA,
		BF_ONE_MINUS_DST_ALPHA,
		BF_SRC_ALPHA_SATURATE
	};

	enum Comparison
	{
		CMP_LESS = 0,
		CMP_LEQUAL,
		CMP_GREATER,
		CMP_GEQUAL,
		CMP_EQUAL
	};

	enum FaceMode
	{
		FM_BACK = 0,
		FM_FRONT,
		FM_FRONT_AND_BACK
	};

	enum AccessMode
	{
		READ_ONLY = 0,
		WRITE_ONLY,
		READ_WRITE
	};
}

#endif //_RD3D_3DRENDERDEFS_H