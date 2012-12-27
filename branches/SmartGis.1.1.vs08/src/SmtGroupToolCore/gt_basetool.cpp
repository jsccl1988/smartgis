#include "gt_basetool.h"
#include "gt_grouptoolfactory.h"
#include "sys_sysmanager.h"
#include "bl_api.h"

using namespace Smt_Sys;

namespace Smt_GroupTool
{
	HINSTANCE g_hInstance = NULL;
}

extern "C" int APIENTRY DllMain(HINSTANCE hInstance,DWORD dwReason,LPVOID lpReserved)
{

	switch(dwReason)
	{
	case DLL_PROCESS_ATTACH:
		Smt_GroupTool::g_hInstance = hInstance;
		break;
	case DLL_THREAD_ATTACH:		 
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return 1;//³É¹¦
}


namespace Smt_GroupTool
{
	SmtBaseTool::SmtBaseTool():m_fScaleDelt(0.15)
		,m_pRenderDevice(NULL)
		,m_pOperMap(NULL)
	{
		;
	}

	SmtBaseTool::~SmtBaseTool()
	{
		;
	}

	int SmtBaseTool::Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack,void* pToFollow)
	{
		if (SMT_ERR_NONE != SmtIATool::Init(hWnd,pfnCallBack,pToFollow))
		{
			return SMT_ERR_FAILURE;
		}

		if (NULL == pMrdRenderDevice)
		{
			return SMT_ERR_FAILURE;
		}

		m_pRenderDevice = pMrdRenderDevice;
		m_pOperMap	   = pOperSmtMap;

		SmtSysManager *pSysMgr = SmtSysManager::GetSingletonPtr();

		SmtSysPra sysPra = pSysMgr->GetSysPra();

		m_fScaleDelt = sysPra.fZoomScaleDelt;
	
		return SMT_ERR_NONE;
	}

	int SmtBaseTool::KeyDown(uint nChar, uint nRepCnt, uint nFlags)
	{
		if(m_pDelegateTag != NULL)
			return m_pDelegateTag->KeyDown(nChar,nRepCnt,nFlags);
		else
		{
			switch (nChar)
			{
			case VK_F5:
				{
					m_pRenderDevice->Refresh();
				}
				break;
			case 'Q':
				{
					POINT mousePos;
					float fScale;
					fScale= 1-m_fScaleDelt;
					GetCursorPos(&mousePos);
					ScreenToClient(m_hWnd,&mousePos);

					lPoint lpnt(mousePos.x,mousePos.y);
					m_pRenderDevice->ZoomScale(m_pOperMap,lpnt,fScale);
					m_pRenderDevice->Refresh();
				}
				break;
			case 'E':
				{
					POINT mousePos;
					float fScale;
					fScale= 1+m_fScaleDelt;
					GetCursorPos(&mousePos);
					ScreenToClient(m_hWnd,&mousePos);

					lPoint lpnt(mousePos.x,mousePos.y);
					m_pRenderDevice->ZoomScale(m_pOperMap,lpnt,fScale);
					m_pRenderDevice->Refresh();
				}
				break;
			case 'R':
				{
					POINT mousePos;
					POINT wndCenter;
					RECT wndrt;
					GetWindowRect(m_hWnd,&wndrt);

					wndCenter.x =(wndrt.left + wndrt.right)>>1;
					wndCenter.y =(wndrt.top + wndrt.bottom)>>1;

					GetCursorPos(&mousePos);

					if((mousePos.x == wndCenter.x)&&(mousePos.y == wndCenter.y))	
						return SMT_ERR_NONE;

					//	SetCursorPos(wndCenter.x,wndCenter.y);

					ScreenToClient(m_hWnd,&mousePos);
					ScreenToClient(m_hWnd,&wndCenter);

					float x1,y1,x2,y2;
					m_pRenderDevice->DPToLP(mousePos.x,mousePos.y,x1,y1);
					m_pRenderDevice->DPToLP(wndCenter.x,wndCenter.y,x2,y2);


					fPoint doffset(x2-x1,y2-y1);
					m_pRenderDevice->ZoomMove(m_pOperMap,doffset);
					m_pRenderDevice->Refresh();
				}
				break;
			case 'W':
				{
					float x1,y1,x2,y2;
					m_pRenderDevice->DPToLP(0,0,x1,y1);
					m_pRenderDevice->DPToLP(0,-10,x2,y2);

					fPoint doffset(x2-x1,y2-y1);
					m_pRenderDevice->ZoomMove(m_pOperMap,doffset);
					m_pRenderDevice->Refresh();
				}
				break;
			case 'A':
				{
					float x1,y1,x2,y2;
					m_pRenderDevice->DPToLP(0,0,x1,y1);
					m_pRenderDevice->DPToLP(-10,0,x2,y2);

					fPoint doffset(x2-x1,y2-y1);
					m_pRenderDevice->ZoomMove(m_pOperMap,doffset);
					m_pRenderDevice->Refresh();
				}
				break;
			case 'S':
				{
					float x1,y1,x2,y2;
					m_pRenderDevice->DPToLP(0,0,x1,y1);
					m_pRenderDevice->DPToLP(0,10,x2,y2);

					fPoint doffset(x2-x1,y2-y1);
					m_pRenderDevice->ZoomMove(m_pOperMap,doffset);
					m_pRenderDevice->Refresh();
				}
				break;
			case 'D':
				{
					float x1,y1,x2,y2;
					m_pRenderDevice->DPToLP(0,0,x1,y1);
					m_pRenderDevice->DPToLP(10,0,x2,y2);

					fPoint doffset(x2-x1,y2-y1);
					m_pRenderDevice->ZoomMove(m_pOperMap,doffset);
					m_pRenderDevice->Refresh();
				}
				break;
			case 'Z':
				{
					if (m_pOperMap != NULL)
					{
						Envelope envelope ;
						fRect frt;

						SmtLayer *pLayer = m_pOperMap->GetActiveLayer();
						if (pLayer)
						{
							pLayer->CalEnvelope();
							pLayer->GetEnvelope(envelope);
							EnvelopeToRect(frt,envelope);
							m_pRenderDevice->ZoomToRect(m_pOperMap,frt);
							m_pRenderDevice->Refresh();
						}	
					}
				}
				break;
			}
		}

		return SMT_ERR_NONE;
	}
}