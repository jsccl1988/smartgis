#include "md3d_water.h"
#include "ml3d_api.h"

using namespace Smt_3DBase;

namespace Smt_3DModel
{
	SmtWater::SmtWater():m_fXScale(1)
		,m_fYScale(1)
		,m_fZScale(1)
	{
		
	}

	SmtWater::~SmtWater()
	{
		Destroy();
	}

	long SmtWater::Init(Vector3& vPos,SmtMaterial&matMaterial,const char* szTexName)
	{
		Smt3DObject::Init(vPos,matMaterial,szTexName);

		m_fTexOffset = 0;

		//////////////////////////////////////////////////////////////////////////
		int x,y,index = 0;	
		/* place the vertices in a grid */
		for(y=0;y<CST_INT_GRID_HEIGHT;y++)
			for(x=0;x<CST_INT_GRID_WIDTH;x++)
			{
				index = y*CST_INT_GRID_WIDTH + x;
				wvertex[index].x = (x-CST_INT_GRID_WIDTH/2)*m_fXScale;
				wvertex[index].y = (y-CST_INT_GRID_HEIGHT/2)*m_fZScale;
				wvertex[index].z = 0;
				wvertex[index].r = 0.7;
				wvertex[index].g = 0.8;
				wvertex[index].b = 0.7;
				wvertex[index].u = y;
				wvertex[index].v = x;
			}

		//////////////////////////////////////////////////////////////////////////
		for(y = 0; y<CST_INT_GRID_HEIGHT; y++)
		{
			for(x = 0; x<CST_INT_GRID_WIDTH; x++)
			{
				p[x][y] = 0.0;
				vx[x][y] = 0.0;
				vy[x][y] = 0.0;
			}
		}

		UpdateVertex();
		UpdateNormal();

		UpdateTexcoord();

		return SMT_ERR_NONE;
	}

	long SmtWater::Create(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(2*(CST_INT_GRID_HEIGHT-1) * CST_INT_GRID_WIDTH,VF_XYZ|VF_DIFFUSE| VF_NORMAL|VF_TEXCOORD,false);
	
		m_aAbb.vcMax.Set(0,0,0);
		m_aAbb.vcMin.Set(0,0,0);
		m_aAbb.vcMax += (CST_INT_GRID_HEIGHT/2.)*m_fXScale;
		m_aAbb.vcMin += -(CST_INT_GRID_HEIGHT/2.)*m_fZScale;
		m_aAbb.vcCenter += (m_aAbb.vcMax+m_aAbb.vcMin)/2.;

		return SMT_ERR_NONE;
		//////////////////////////////////////////////////////////////////////////
		int x,y,index = 0;	
		double dx, dy, d;

		for(y = 0; y<CST_INT_GRID_HEIGHT; y++)
		{
			for(x = 0; x<CST_INT_GRID_WIDTH; x++)
			{
				dx = (double)(x-CST_INT_GRID_WIDTH/2);
				dy = (double)(y-CST_INT_GRID_HEIGHT/2);
				d = sqrt( dx*dx + dy*dy );
				if(d < 0.1 * (double)(CST_INT_GRID_WIDTH/2))
				{
					d = d * 10.0;
					p[x][y] = -cos(d * (PI / (double)(CST_INT_GRID_WIDTH * 4))) * 100.0;
				}
				else
				{
					p[x][y] = 0.0;
				}
				vx[x][y] = 0.0;
				vy[x][y] = 0.0;
			}
		}

		UpdateVertex();
		UpdateNormal();
		UpdateTexcoord();
		//
		UpdateVB();

		return SMT_ERR_NONE;
	}

	long SmtWater::Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed)
	{
		//¼ÆËã
		//Compute(fElapsed);

		//////////////////////////////////////////////////////////////////////////
		//UpdateVertex();
		//UpdateNormal();
		UpdateTexcoord();

		//
		UpdateVB();

		return SMT_ERR_NONE;
	}

	long SmtWater::Render(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		SmtTexture* pTex = p3DRenderDevice->GetTexture(m_strTexName.c_str());
		if(pTex)
		{
			p3DRenderDevice->SetTexture( pTex );
		}

		p3DRenderDevice->SetMaterial(&m_matMaterial);

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxWorld);

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxModel);

		for (int i=0; i<CST_INT_GRID_HEIGHT-1; i++)
		{
			p3DRenderDevice->DrawPrimitives(PT_TRIANGLESTRIP,m_pVertexBuffer,i*2*(CST_INT_GRID_WIDTH),2*(CST_INT_GRID_WIDTH-1) );
		}

		p3DRenderDevice->MatrixPop();
		p3DRenderDevice->MatrixPop();

		return SMT_ERR_NONE;
	}

	bool SmtWater::Select(LP3DRENDERDEVICE p3DRenderDevice,const lPoint& point)
	{
		if (NULL == p3DRenderDevice || NULL == m_pVertexBuffer)
			return false;

		Vector3 vOrg,vTar,vDir;

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxWorld);

		p3DRenderDevice->MatrixPush();
		p3DRenderDevice->MatrixMultiply(m_mtxModel);
		p3DRenderDevice->Transform2DTo3D(vOrg,vTar,point);
		p3DRenderDevice->MatrixPop();

		p3DRenderDevice->MatrixPop();

		vDir = vTar-vOrg;
		if (vDir.GetSqrLength() > 0)
		{
			Ray		ray;
			float   f;
			ray.Set(vOrg,vDir);
			if (m_aAbb.Intersects(ray,&f))
			{
				return true;
			}
		}

		return false;
	}

	long SmtWater::Destroy()
	{
		SMT_SAFE_DELETE( m_pVertexBuffer);
		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtWater::Compute(float dt)
	{
		int    x, y, x2, y2;
		double time_step =  dt*CST_DBF_ANIMATION_SPEED;

		/* compute accelerations */
		for(x = 0; x < CST_INT_GRID_WIDTH; x++)
		{
			x2 = (x + 1) % CST_INT_GRID_WIDTH;
			for(y = 0; y < CST_INT_GRID_HEIGHT; y++)
			{
				ax[x][y] = p[x][y] - p[x2][y];
			}
		}

		for(y = 0; y < CST_INT_GRID_HEIGHT;y++)
		{
			y2 = (y + 1) % CST_INT_GRID_HEIGHT;
			for(x = 0; x < CST_INT_GRID_WIDTH; x++)
			{
				ay[x][y] = p[x][y] - p[x][y2];
			}
		}

		/* compute speeds */
		for(x = 0; x < CST_INT_GRID_WIDTH; x++)
		{
			for(y = 0; y < CST_INT_GRID_HEIGHT; y++)
			{
				vx[x][y] = vx[x][y] + ax[x][y] * time_step;
				vy[x][y] = vy[x][y] + ay[x][y] * time_step;
			}
		}

		/* compute pressure */
		for(x = 1; x < CST_INT_GRID_WIDTH; x++)
		{
			x2 = x - 1;
			for(y = 1; y < CST_INT_GRID_HEIGHT; y++)
			{
				y2 = y - 1;
				p[x][y] = p[x][y] + (vx[x2][y] - vx[x][y] + vy[x][y2] - vy[x][y]) * time_step;
			}
		}
	}
 
	void SmtWater::UpdateVertex(void)
	{
		int index = 0;
		int x, y;

		for(y = 0; y<CST_INT_GRID_HEIGHT; y++)
		{
			for(x = 0; x<CST_INT_GRID_WIDTH; x++)
			{
				index = y*CST_INT_GRID_WIDTH + x;
				wvertex[index].z = (float) (p[x][y]*(1.0/10.0))*m_fYScale;
			}
		}
	}

	void SmtWater::UpdateNormal()
	{
		//new mem
		Vector3 *pNormals = new Vector3[CST_INT_GRID_SIZE];

		//calculator normal
		for( int iY=0; iY<CST_INT_GRID_HEIGHT - 1; iY++ )
		{
			for(int  iX=0; iX<CST_INT_GRID_WIDTH - 1; iX++ )
			{
				int P1,P2,P3,P4;
				P1 = (iY * CST_INT_GRID_WIDTH) + iX;
				P2 = ((iY + 1) * CST_INT_GRID_WIDTH) + iX;
				P3 = P1 + 1;
				P4 = P2 + 1;

				Vector4 V1(wvertex[P1].x,wvertex[P1].y,wvertex[P1].z),
					    V2(wvertex[P2].x,wvertex[P2].y,wvertex[P2].z),
					    V3(wvertex[P3].x,wvertex[P3].y,wvertex[P3].z),
					    V4(wvertex[P4].x,wvertex[P4].y,wvertex[P4].z);

				Vector4 nor1 = GalcTriangleNormal(V1, V3, V2);
				Vector4 nor2 = GalcTriangleNormal(V3, V4, V2);

				pNormals[P1] += nor1;
				pNormals[P2] += nor1;
				pNormals[P3] += nor1;
				pNormals[P2] += nor2;
				pNormals[P3] += nor2;
				pNormals[P4] += nor2;
			}
		}

		//normalize
		for(long i=0; i < CST_INT_GRID_SIZE; i++)
		{
			pNormals[i].Normalize();
			wvertex[i].nx = pNormals[i].x;
			wvertex[i].ny = pNormals[i].y;
			wvertex[i].nz = pNormals[i].z;
		}

		SMT_SAFE_DELETE_A(pNormals);
	}

	void SmtWater::UpdateTexcoord(void)
	{
		int index = 0;
		int x, y;
		
		for(y = 0; y<CST_INT_GRID_HEIGHT; y++)
		{
			for(x = 0; x<CST_INT_GRID_WIDTH; x++)
			{
				index = y*CST_INT_GRID_WIDTH + x;
				wvertex[index].u = 4*y/(float)CST_INT_GRID_WIDTH+m_fTexOffset;
				wvertex[index].v = 4*x/(float)CST_INT_GRID_HEIGHT+m_fTexOffset;
			}
		}

		m_fTexOffset += 0.002f;
	}

	 void SmtWater::UpdateVB(void)
	 {
		 if (SMT_ERR_NONE != m_pVertexBuffer->Lock())
			 return;

		 int p;
		 for( int iY=0; iY<CST_INT_GRID_HEIGHT - 1; iY++ )
		 {
			 for(int  iX=0; iX<CST_INT_GRID_WIDTH; iX++ )
			 {
				 p = iY*CST_INT_GRID_WIDTH + iX;
				 m_pVertexBuffer->Vertex(wvertex[p].x,wvertex[p].z,wvertex[p].y);
				 m_pVertexBuffer->Normal(wvertex[p].nx,wvertex[p].nz,wvertex[p].ny);
				 m_pVertexBuffer->Diffuse(wvertex[p].r,wvertex[p].g,wvertex[p].b,0.3);
				 m_pVertexBuffer->TexVertex(wvertex[p].u,wvertex[p].v);

				 p = (iY+1)*CST_INT_GRID_WIDTH + iX;
				 m_pVertexBuffer->Vertex(wvertex[p].x,wvertex[p].z,wvertex[p].y);
				 m_pVertexBuffer->Normal(wvertex[p].nx,wvertex[p].nz,wvertex[p].ny);
				 m_pVertexBuffer->Diffuse(wvertex[p].r,wvertex[p].g,wvertex[p].b,0.3);
				 m_pVertexBuffer->TexVertex(wvertex[p].u,wvertex[p].v);
			 }
		 }
		 m_pVertexBuffer->Unlock();
	 }

}