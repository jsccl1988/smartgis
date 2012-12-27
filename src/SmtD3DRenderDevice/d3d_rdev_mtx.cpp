#include "d3d_3drenderdevice.h"
#include "smt_logmanager.h"

using namespace Smt_Core;
namespace Smt_3Drd
{
	//////////////////////////////////////////////////////////////////////////
	// Tranformation functions
	long SmtD3DRenderDevice::MatrixModeSet(MatrixMode mode)
	{
		m_matrixMode = mode;

		return SMT_ERR_NONE;
	}

	MatrixMode SmtD3DRenderDevice::MatrixModeGet() const
	{
		return m_matrixMode;
	}

	long SmtD3DRenderDevice::MatrixLoadIdentity()
	{
		Matrix mat;
		mat.Identity();
		MatrixLoad(mat);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::MatrixLoad( const Matrix& mtx )
	{
		if ( m_matrixMode==MM_PROJECTION )
		{
			memcpy(&m_prjD3DMatrix, &mtx, sizeof(Matrix)); 
			m_pD3DDevice->SetTransform( D3DTS_PROJECTION, &m_prjD3DMatrix );
		}
		else
		{
			memcpy(&m_worldD3DMatrix, &mtx, sizeof(Matrix)); 
			m_pD3DDevice->SetTransform( D3DTS_WORLD, &m_worldD3DMatrix );
		}

		// Reset D3D view matrix
		//--
		D3DXMatrixIdentity(&m_viewD3DMatrix);
		m_pD3DDevice->SetTransform( D3DTS_VIEW, &m_viewD3DMatrix );

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::MatrixPush()
	{
		if ( m_matrixMode==MM_PROJECTION )
			m_matStack.push( m_prjD3DMatrix );
		else
			m_matStack.push( m_viewD3DMatrix );

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::MatrixPop()
	{
		if (m_matStack.empty())
			return SMT_ERR_FAILURE;

		if ( m_matrixMode==MM_PROJECTION )
		{
			m_prjD3DMatrix = m_matStack.top();
			m_pD3DDevice->SetTransform( D3DTS_PROJECTION, &m_prjD3DMatrix );
		}
		else
		{
			m_viewD3DMatrix = m_matStack.top();
			m_pD3DDevice->SetTransform( D3DTS_WORLD, &m_viewD3DMatrix );
		}

		m_matStack.pop();

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::MatrixScale( float x, float y, float z )
	{
		Matrix mtx;
		mtx.Identity();
		mtx.Scale( x, y, z );
		MatrixMultiply( mtx );

		return SMT_ERR_NONE;
	}


	long SmtD3DRenderDevice::MatrixTranslation( float x, float y, float z )
	{
		Matrix mtx;
		mtx.Identity();
		mtx.Translate(x, y, z );
		MatrixMultiply( mtx );

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::MatrixRotation( float angle, float x, float y, float z )
	{
		Matrix mtx;
		mtx.Identity();
		mtx.Rotate( angle,x, y, z );
		MatrixMultiply( mtx );

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::MatrixMultiply(const Matrix& mtx)
	{
		D3DXMATRIX d3dMtx,curD3dMtx;

		memcpy(&d3dMtx,&mtx,sizeof(D3DXMATRIX));

		if ( m_matrixMode==MM_PROJECTION )
		{
			curD3dMtx = m_prjD3DMatrix;
			D3DXMatrixMultiply(&m_prjD3DMatrix,&d3dMtx,&curD3dMtx);
			m_pD3DDevice->SetTransform( D3DTS_PROJECTION, &m_prjD3DMatrix);
		}
		else
		{
			curD3dMtx = m_viewD3DMatrix;
			D3DXMatrixMultiply(&m_viewD3DMatrix,&d3dMtx,&curD3dMtx);
			m_pD3DDevice->SetTransform( D3DTS_WORLD, &m_viewD3DMatrix );
		}

		return SMT_ERR_NONE;
	}

	Matrix SmtD3DRenderDevice::MatrixGet()
	{
		Matrix mtx;
		D3DXMATRIX d3dMat;

		if ( m_matrixMode==MM_PROJECTION )
		{
			m_pD3DDevice->GetTransform( D3DTS_PROJECTION, &d3dMat );
		}
		else
		{
			m_pD3DDevice->GetTransform( D3DTS_WORLD, &d3dMat );
		}

		memcpy(&mtx, &d3dMat, sizeof(Matrix)); 

		return mtx;
	}
}