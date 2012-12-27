/*
File:    bl3d_bas_struct.h

Desc:    三维基本结构

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _BL3D_BAS_STRUCT_H
#define _BL3D_BAS_STRUCT_H

#include "ml3d_mathlib.h"

using namespace Smt_3DMath;
namespace Smt_3DBase
{
	struct SmtVertex3D
	{
		Vector3		ver;
		SmtColor	clr;
	};

	typedef vector<SmtVertex3D>		vSmtVertex3Ds;

	struct SmtVertex3DList
	{
		int				nCount;
		SmtVertex3D		*pVertexs;

		SmtVertex3DList()
		{
			pVertexs = NULL;
			nCount = 0;
		}

		SmtVertex3DList( const vSmtVertex3Ds &vOther)
		{
			pVertexs = NULL;
			nCount = 0;

			if (vOther.size() < 1)
				return;

			Release();

			pVertexs = new SmtVertex3D[vOther.size()];
			nCount = vOther.size();
			for (int i = 0 ; i < nCount; i++)
			{
				pVertexs[i] = vOther[i];
			}
		}


		SmtVertex3DList( const SmtVertex3DList &Other)
		{
			pVertexs = NULL;
			nCount = 0;

			if (this == &Other || Other.nCount < 1 || Other.pVertexs == NULL)
				return;

			Release();

			pVertexs = new SmtVertex3D[Other.nCount];
			nCount = Other.nCount;
			memcpy(pVertexs,Other.pVertexs,sizeof(SmtVertex3D)*Other.nCount);
		}

		SmtVertex3DList& operator =(const SmtVertex3DList &Other)
		{
			if (this == &Other || Other.nCount < 1 || Other.pVertexs == NULL)
				return *this;

			Release();

			pVertexs = new SmtVertex3D[Other.nCount];
			nCount = Other.nCount;
			memcpy(pVertexs,Other.pVertexs,sizeof(SmtVertex3D)*Other.nCount);

			return *this;
		}

		SmtVertex3DList& operator =(const vSmtVertex3Ds &vOther)
		{
			if (vOther.size() < 1)
				return *this;

			Release();

			pVertexs = new SmtVertex3D[vOther.size()];
			nCount = vOther.size();
			for (int i = 0 ; i < nCount; i++)
			{
				pVertexs[i] = vOther[i];
			}

			return *this;
		}

		void	Release(void)
		{
			SMT_SAFE_DELETE_A(pVertexs);
			nCount = 0;
		}

		~SmtVertex3DList()
		{
			Release();
		}
	} ;

	enum eSmtOctreeNodes 
	{ 
		TOP_LEFT_FRONT = 0,			// 0 
		TOP_LEFT_BACK,				// 1 
		TOP_RIGHT_BACK,				// etc... 
		TOP_RIGHT_FRONT, 
		BOTTOM_LEFT_FRONT, 
		BOTTOM_LEFT_BACK, 
		BOTTOM_RIGHT_BACK, 
		BOTTOM_RIGHT_FRONT 
	}; 
}

#endif //_BL3D_BAS_STRUCT_H