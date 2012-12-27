#include "gdi_renderthread.h"
#include "smt_logmanager.h"
#include "bl_api.h"
#include "gdi_aux_api.h"
#include "smt_api.h"
#include "geo_api.h"
#include "ximage.h"
#include "Resource.h"

namespace Smt_Rd
{
	int SmtGdiRenderThread::RenderLayer(const SmtLayer *pLayer,int op)
	{
		if(pLayer->GetLayerType() == LYR_VECTOR)
			return RenderLayer((SmtVectorLayer*)pLayer,op);
		else if(pLayer->GetLayerType() == LYR_RASTER)
			return RenderLayer((SmtRasterLayer*)pLayer,op);
		else if(pLayer->GetLayerType() == LYR_TITLE)
			return RenderLayer((SmtTileLayer*)pLayer,op);

		return SMT_ERR_FAILURE;
	}

	int SmtGdiRenderThread::RenderLayer( const  SmtVectorLayer *pLayer,int op)
	{
		if (NULL == pLayer)
			return SMT_ERR_INVALID_PARAM;

		if (!pLayer->IsVisible())
			return SMT_ERR_NONE;

		Envelope envLayer ;
		pLayer->GetEnvelope(envLayer);
		Envelope envViewp;

		lRect lViewp;
		fRect fViewp;

		ViewportToRect(lViewp,m_smtRC.viewport);
		DRectToLRect(lViewp,fViewp);
		RectToEnvelope(envViewp,fViewp);

		if (!envLayer.Intersects(envViewp))
			return SMT_ERR_NONE;

		pLayer->MoveFirst();
		while (!pLayer->IsEnd())
		{
			RenderFeature(pLayer->GetFeature(),op);
			pLayer->MoveNext();
		}

		return SMT_ERR_FAILURE;
	}

	int SmtGdiRenderThread::RenderLayer(const SmtRasterLayer *pLayer,int op)
	{
		if (NULL == pLayer)
			return SMT_ERR_INVALID_PARAM;

		if (!pLayer->IsVisible())
			return SMT_ERR_NONE; 

		Envelope envLayer ;
		pLayer->GetEnvelope(envLayer);
		Envelope envViewp;

		lRect lViewp;
		fRect fViewp;

		ViewportToRect(lViewp,m_smtRC.viewport);
		DRectToLRect(lViewp,fViewp);
		RectToEnvelope(envViewp,fViewp);

		if (!envLayer.Intersects(envViewp))
			return SMT_ERR_NONE;

		char		*pRasterBuf = NULL;
		long		lRasterBufSize = 0;
		long		lCodeType = -1;
		fRect		locRect;

		if (SMT_ERR_NONE == pLayer->GetRasterNoClone(pRasterBuf,lRasterBufSize,locRect,lCodeType))
		{
			StrethImage(pRasterBuf,lRasterBufSize,locRect,lCodeType);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::RenderLayer(const SmtTileLayer *pLayer,int op)
	{
		if (NULL == pLayer)
			return SMT_ERR_INVALID_PARAM;

		if (!pLayer->IsVisible())
			return SMT_ERR_NONE; 

		Envelope envLayer ;
		pLayer->GetEnvelope(envLayer);
		Envelope envViewp;

		lRect lViewp;
		fRect fViewp;
		lRect titleDPRect;

		ViewportToRect(lViewp,m_smtRC.viewport);
		DRectToLRect(lViewp,fViewp);
		RectToEnvelope(envViewp,fViewp);

		if (!envLayer.Intersects(envViewp))
			return SMT_ERR_NONE;

		long	 lZoom = 0;
		long	 lCol = 0;
		long	 lRow = 0;

		pLayer->MoveFirst();
		while (!pLayer->IsEnd())
		{
			SmtTile *pTile = pLayer->GetTile();
			if (NULL != pTile && pTile->bVisible)
			{
				/*Envelope envTile;
				RectToEnvelope(envTile,pTile->rtTileRect);
				LRectToDRect(pTile->rtTileRect,titleDPRect);

				if (!envTile.Intersects(envViewp) ||
					(titleDPRect.Height() < 2 && titleDPRect.Width() < 2))
					return SMT_ERR_NONE;*/

				StrethImage(pTile->pTileBuf,pTile->lTileBufSize,pTile->rtTileRect,pTile->lImageCode);
			}			

			pLayer->MoveNext();
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::RenderFeature( const  SmtFeature *pFeature,int op)
	{
		if (NULL == pFeature)
			return SMT_ERR_INVALID_PARAM;

		const  SmtStyle *  pStyle = pFeature->GetStyle();
		const  SmtGeometry *  pGeom  = pFeature->GetGeometryRef();
		m_nFeatureType = pFeature->GetFeatureType();
		const  SmtAttribute * pAtt = pFeature->GetAttributeRef();

		switch(m_nFeatureType)
		{
		case SmtFeatureType::SmtFtAnno:
			{
				const SmtField *pFld = NULL;

				pFld = pAtt->GetFieldPtr(pAtt->GetFieldIndex("anno"));
				sprintf(m_szAnno,pFld->GetValueAsString());

				pFld = pAtt->GetFieldPtr(pAtt->GetFieldIndex("angle"));
				m_fAnnoAngle = pFld->GetValueAsDouble();
			}
			break;
		case SmtFeatureType::SmtFtChildImage:
			{

			}
			break;
		case SmtFeatureType::SmtFtSurface:
			{
				// nDrawMode = R2_MASKPEN;
			}
			break;
		default:
			break;
		}

		return RenderGeometry(pGeom,pStyle,op);	
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderThread::PrepareForDrawing( const  SmtStyle*pStyle,int nDrawMode)
	{
		::SetROP2(m_hCurDC,nDrawMode);

		m_bCurUseStyle	= (NULL != pStyle);

		if (m_bCurUseStyle)
		{
			ulong format = pStyle->GetStyleType();
			if( format & ST_PenDesc )
			{
				SmtPenDesc pen = pStyle->GetPenDesc();

				if (m_hPen)
				{
					DeleteObject(m_hPen);
					m_hPen = NULL;
				}

				m_hPen = CreatePen(pen.lPenStyle,pen.fPenWidth*m_smtRC.fblc,pen.lPenColor);
				m_hOldPen = (HPEN)::SelectObject(m_hCurDC, m_hPen);
			}

			if( format & ST_BrushDesc )
			{
				SmtBrushDesc brush = pStyle->GetBrushDesc();

				if (m_hBrush)
				{
					DeleteObject(m_hBrush);
					m_hBrush = NULL;
				}

				if(brush.brushTp == SmtBrushDesc::BT_Hatch)
				{
					m_hBrush = CreateHatchBrush(brush.lBrushStyle, brush.lBrushColor);
				}
				else
					m_hBrush = CreateSolidBrush(brush.lBrushColor);

				m_hOldBrush = (HBRUSH)::SelectObject(m_hCurDC, m_hBrush);
			}

			if (format & ST_SymbolDesc)
			{
				SmtSymbolDesc symbol = pStyle->GetSymbolDesc();

				if (m_hIcon)
				{
					DeleteObject(m_hIcon);
					m_hIcon = NULL;
				}

				m_hIcon = LoadIcon(m_hInst,MAKEINTRESOURCE(symbol.lSymbolID+IDI_ICON_A));
			}

			if (format & ST_AnnoDesc)
			{
				SmtAnnotationDesc anno = pStyle->GetAnnoDesc();

				if (m_hFont)
				{
					DeleteObject(m_hFont);
					m_hFont = NULL;
				}

				m_hFont = CreateFont( anno.fHeight*m_smtRC.fblc,anno.fWidth*m_smtRC.fblc,anno.lEscapement,anno.lOrientation
					,anno.lWeight,anno.lItalic,anno.lUnderline,anno.lStrikeOut,anno.lCharSet,
					anno.lOutPrecision,anno.lClipPrecision,anno.lQuality,anno.lPitchAndFamily,anno.szFaceName);

				m_hOldFont  = (HFONT)::SelectObject(m_hCurDC,m_hFont);
				SetBkMode(m_hCurDC,TRANSPARENT);
				SetTextColor(m_hCurDC,anno.lAnnoClr);

				//m_fAnnoAngle = anno.fAngle;
			}
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::EndDrawing()
	{
		if (m_bCurUseStyle)
		{
			::SelectObject(m_hCurDC, m_hOldBrush);
			::SelectObject(m_hCurDC, m_hOldPen);
			::SelectObject(m_hCurDC, m_hOldFont);
		}

		return SMT_ERR_FAILURE;
	}

	int SmtGdiRenderThread::RenderGeometry( const  SmtGeometry *pGeom,const SmtStyle*pStyle,int op)
	{
		if (!pGeom)
			return SMT_ERR_INVALID_PARAM;

		SmtGeometryType type  = pGeom->GetGeometryType();

		Envelope envFeature,envViewp;
		pGeom->GetEnvelope(&envFeature);

		lRect lViewp;
		fRect fViewp;
		fRect fenv;
		lRect lenv;

		ViewportToRect(lViewp,m_smtRC.viewport);
		DRectToLRect(lViewp,fViewp);
		RectToEnvelope(envViewp,fViewp);

		EnvelopeToRect(fenv,envFeature);
		LRectToDRect(fenv,lenv);

		if (!envFeature.Intersects(envViewp) ||
			(type !=GTPoint && lenv.Height() < 2 && lenv.Width() < 2))
			return SMT_ERR_NONE;

		::SaveDC(m_hCurDC);

		if (!m_bLockStyle)
			PrepareForDrawing(pStyle);

		if (m_rdPra.bShowMBR)
		{
			long lX = 0,lY = 0; 

			LPToDP(envFeature.MinX, envFeature.MinY,lX,lY);
			MoveToEx(m_hCurDC,lX,lY,NULL);

			LPToDP(envFeature.MaxX, envFeature.MinY,lX,lY);
			LineTo(m_hCurDC,lX,lY);

			LPToDP(envFeature.MaxX, envFeature.MaxY,lX,lY);
			LineTo(m_hCurDC,lX,lY);

			LPToDP(envFeature.MinX, envFeature.MaxY,lX,lY);
			LineTo(m_hCurDC,lX,lY) ;

			LPToDP(envFeature.MinX, envFeature.MinY,lX,lY);
			LineTo(m_hCurDC,lX,lY) ;     
		}

		switch(type)
		{
		case GTPoint:
			DrawPoint(pStyle,(SmtPoint*)pGeom);
			break;

		case GTLineString:
			DrawLineString((SmtLineString*)pGeom);
			break;
		case GTArc:
			DrawArc((SmtArc*)pGeom);
			break;

		case GTPolygon:
			DrawPloygon((SmtPolygon*)pGeom);
			break;

		case GTFan:
			DrawFan((SmtFan*)pGeom);
			break;

		case GTMultiPoint:
			DrawMultiPoint(pStyle,(SmtMultiPoint*)pGeom);
			break;

		case GTMultiLineString:
			DrawMultiLineString((SmtMultiLineString*)pGeom);
			break;

		case GTMultiPolygon:
			DrawMultiPolygon((SmtMultiPolygon*)pGeom);
			break;

		case GTSpline:
			DrawLineSpline((SmtSpline*)pGeom);
			break;

		case GTLinearRing:
			DrawLinearRing((SmtLinearRing*)pGeom);
			break;

		case GTGrid:
			DrawGrid((SmtGrid*)pGeom);
			break;

		case GTTin:
			DrawTin((SmtTin*)pGeom);
			break;

		case GTNone:
		case GTUnknown:
		default:
			break;
		}

		if(!m_bLockStyle)
			EndDrawing();

		::RestoreDC(m_hCurDC,-1);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderThread::DrawMultiLineString( const  SmtMultiLineString *pMultiLinestring)
	{
		int nLines = pMultiLinestring->GetNumGeometries();

		int i = 0;
		while (i < nLines)
		{
			DrawLineString((SmtLineString*)pMultiLinestring->GetGeometryRef(i));
			i++;
		}

		return SMT_ERR_FAILURE;
	}

	int SmtGdiRenderThread::DrawMultiPoint( const  SmtStyle*pStyle, const  SmtMultiPoint *pMultiPoint)
	{
		int nPoints = pMultiPoint->GetNumGeometries();

		int i = 0;
		while (i < nPoints)
		{
			DrawPoint(pStyle,(SmtPoint*)pMultiPoint->GetGeometryRef(i));
			i++;
		}

		return SMT_ERR_FAILURE;
	}

	int SmtGdiRenderThread::DrawMultiPolygon( const  SmtMultiPolygon *pMultiPolygon)
	{
		int nPolygons = pMultiPolygon->GetNumGeometries();

		int i = 0;
		while (i < nPolygons)
		{
			DrawPloygon((SmtPolygon*)pMultiPolygon->GetGeometryRef(i));
			i++;
		}

		return SMT_ERR_FAILURE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderThread::DrawPoint(const SmtStyle*pStyle,const SmtPoint *pPoint)
	{
		ulong format = pStyle->GetStyleType();
		if (m_nFeatureType == SmtFeatureType::SmtFtAnno)
		{
			SmtAnnotationDesc anno = pStyle->GetAnnoDesc();
			return DrawAnno(m_szAnno,m_fAnnoAngle,abs(anno.fHeight),abs(anno.fWidth),abs(anno.fSpace),pPoint);
		}
		else if(m_nFeatureType == SmtFeatureType::SmtFtChildImage)
		{
			SmtSymbolDesc symbol = pStyle->GetSymbolDesc();
			return DrawSymbol(m_hIcon,symbol.fSymbolHeight,symbol.fSymbolWidth,pPoint);
		}
		else if (m_nFeatureType == SmtFeatureType::SmtFtDot)
		{
			int r = m_rdPra.lPointRaduis/**m_smtRC.fblc*/;
			long lX,lY;
			LPToDP(pPoint->GetX(),pPoint->GetY(),lX,lY);
			Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);

			return SMT_ERR_NONE;
		}	

		return SMT_ERR_FAILURE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderThread::DrawAnno(const char *szAnno,float fangel,float fCHeight,float fCWidth,float fCSpace,const SmtPoint *pPoint)
	{
		if (szAnno == NULL)
			return SMT_ERR_INVALID_PARAM;

		fCHeight *= m_smtRC.fblc;
		fCWidth *= m_smtRC.fblc;
		fCSpace *= m_smtRC.fblc;

		unsigned char c1,c2;
		fPoint pt;
		long x,y;

		char bz[4];
		const char *ls1;
		ls1 = szAnno;

		LPToDP(pPoint->GetX(),pPoint->GetY(),x,y);
		pt.x = x;
		pt.y = y;

		pt.x -= 2*fCHeight*sin(fangel);
		pt.y -= 2*fCHeight*cos(fangel);

		int nStrLength  = (int)strlen(ls1);
		while(nStrLength > 0)
		{
			c1 = *ls1;
			c2 = *(ls1 + 1);
			if(c1 >127 && c2 > 127) //如果下一个字符是汉字
			{
				strncpy(bz,ls1,2);
				bz[2] = 0;
				ls1 = ls1 + 2;
				TextOut(m_hCurDC,pt.x,pt.y,(LPCSTR)bz,2);
				nStrLength -= 2;
				pt.x += (fCWidth*2 + fCSpace) * cos(fangel);
				pt.y += (fCWidth*2 + fCSpace) * sin(fangel);
			}
			else
			{
				strncpy(bz,ls1,1);
				bz[1] = 0;
				ls1++;
				TextOut(m_hCurDC,pt.x,pt.y,(LPCSTR)bz,1);
				nStrLength -= 1;

				pt.x += (fCWidth + fCSpace/2.) * cos(fangel);
				pt.y += (fCWidth + fCSpace/2.) * sin(fangel);
			}
		}

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			long lX,lY;
			LPToDP(pPoint->GetX(),pPoint->GetY(),lX,lY);
			DrawCross(m_hCurDC,lX,lY,m_rdPra.lPointRaduis);
			//Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);
			DrawCross(m_hCurDC,lX,lY,r);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawSymbol(HICON hIcon,long lHeight,long lWidth,const SmtPoint *pPoint)
	{
		lHeight *= m_smtRC.fblc;
		lWidth *= m_smtRC.fblc;

		long lX,lY;
		LPToDP(pPoint->GetX(),pPoint->GetY(),lX,lY);
		//::DrawIcon(m_hCurDC,pt.x-lWidth,pt.y-lHeight,hIcon);
		::DrawIconEx(m_hCurDC,lX-lWidth/2,lY+lHeight/2,   hIcon, lWidth, lHeight, 0, NULL, DI_NORMAL);
		//::DrawState( m_hCurDC,NULL,NULL,(LPARAM)hIcon,0,pt.x-lWidth/2,pt.y+lHeight/2,lWidth,lHeight, DSS_NORMAL | DST_ICON);

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			//Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);
			DrawCross(m_hCurDC,lX,lY,r);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawLineSpline(const SmtSpline *pSpline)
	{
		int    nPoints = pSpline->GetAnalyticPointCount();
		if (nPoints < 2)
			return SMT_ERR_INVALID_PARAM;

		int i = 0;
		POINT *lpPoint = NULL;

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < (m_bufPool.GetBufCount()*m_bufPool.GetSizePerBuf()))
		{
			m_bufPool.FreeAllBuf();
			lpPoint = (POINT *)m_bufPool.NewBuf();
		}
		else
			lpPoint = new POINT[nPoints];
#else
		lpPoint = new POINT[nPoints];
#endif

		for (int i = 0;i < nPoints ; i++)
		{
			LPToDP(pSpline->GetAnalyticX(i),pSpline->GetAnalyticY(i),lpPoint[i].x,lpPoint[i].y);
		}

		MoveToEx (m_hCurDC, lpPoint[0].x, lpPoint[0].y, NULL) ;    
		PolylineTo(m_hCurDC, lpPoint, nPoints);

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < (m_bufPool.GetBufCount()*m_bufPool.GetSizePerBuf()))
		{
			m_bufPool.FreeAllBuf();
		}
		else
			SMT_SAFE_DELETE_A(lpPoint);
#else
		SMT_SAFE_DELETE_A(lpPoint);
#endif
	//	if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			long lX,lY;
			for (int i = 0; i < pSpline->GetNumPoints();i++)
			{
				LPToDP(pSpline->GetX(i),pSpline->GetY(i),lX,lY);
				Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			}
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawLineString(const SmtLineString *pLinestring)
	{
		int    nPoints = pLinestring->GetNumPoints();
		if (nPoints < 2)
			return SMT_ERR_INVALID_PARAM;

		int i = 0;
		POINT *lpPoint = NULL;

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < (m_bufPool.GetBufCount()*m_bufPool.GetSizePerBuf()))
		{
			m_bufPool.FreeAllBuf();
			lpPoint = (POINT *)m_bufPool.NewBuf();
		}
		else
			lpPoint = new POINT[nPoints];
#else
		lpPoint = new POINT[nPoints];
#endif

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			for (int i = 0;i < nPoints ; i++)
			{
				LPToDP(pLinestring->GetX(i),pLinestring->GetY(i),lpPoint[i].x,lpPoint[i].y);
				//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				DrawCross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}
		}
		else
		{
			for (int i = 0;i < nPoints ; i++)
			{
				LPToDP(pLinestring->GetX(i),pLinestring->GetY(i),lpPoint[i].x,lpPoint[i].y);
			}
		}

		MoveToEx (m_hCurDC, lpPoint[0].x, lpPoint[0].y, NULL) ;    
		PolylineTo(m_hCurDC, lpPoint, nPoints);

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < (m_bufPool.GetBufCount()*m_bufPool.GetSizePerBuf()))
		{
			m_bufPool.FreeAllBuf();
		}
		else
			SMT_SAFE_DELETE_A(lpPoint);
#else
		SMT_SAFE_DELETE_A(lpPoint);
#endif

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawLinearRing(const SmtLinearRing *pLinearRing)
	{
		int    nPoints = pLinearRing->GetNumPoints();
		if (nPoints < 2)
			return SMT_ERR_INVALID_PARAM;

		int i = 0;
		POINT * lpPoint = NULL;
#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < m_bufPool.GetPoolSize())
		{
			m_bufPool.FreeAllBuf();
			lpPoint = (POINT *)m_bufPool.NewBuf();
		}
		else
			lpPoint = new POINT[nPoints];
#else
		lpPoint = new POINT[nPoints];
#endif

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			for (int i = 0; i < pLinearRing->GetNumPoints();i++)
			{
				LPToDP(pLinearRing->GetX(i),pLinearRing->GetY(i),lpPoint[i].x,lpPoint[i].y);
				//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				DrawCross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}
		}
		else
		{
			for (int i = 0;i < nPoints ; i++)
			{
				LPToDP(pLinearRing->GetX(i),pLinearRing->GetY(i),lpPoint[i].x,lpPoint[i].y);
			}
		}

		MoveToEx (m_hCurDC, lpPoint[0].x, lpPoint[0].y, NULL) ;    
		PolylineTo(m_hCurDC, lpPoint, nPoints);

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < m_bufPool.GetPoolSize())
		{
			m_bufPool.FreeAllBuf();
		}
		else
			SMT_SAFE_DELETE_A(lpPoint);
#else
		SMT_SAFE_DELETE_A(lpPoint);
#endif

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawPloygon(const SmtPolygon *pPloygon)
	{
		int nAllPts = 0;
		const SmtLinearRing *pLinerring = pPloygon->GetExteriorRing();

		int    nExteriorPts = pLinerring->GetNumPoints();
		if (nExteriorPts < 2)
			return SMT_ERR_INVALID_PARAM;

		nAllPts += nExteriorPts;

		int nInteriorRings = pPloygon->GetNumInteriorRings();
		int *nRings = new int[nInteriorRings + 1];
		nRings[0] = nExteriorPts;

		for (int i = 0; i < nInteriorRings ; i++)
		{
			const SmtLinearRing *pInteriorRing = pPloygon->GetInteriorRing(i);
			nRings[i+1] = pInteriorRing->GetNumPoints();
			nAllPts += nRings[i+1];
		}

		int i = 0;
		POINT * lpPoint = NULL;

#ifdef GDI_USE_BUFPOOL
		if (nAllPts*(sizeof(POINT)) < m_bufPool.GetPoolSize())
		{
			m_bufPool.FreeAllBuf();
			lpPoint = (POINT *)m_bufPool.NewBuf();
		}
		else
			lpPoint = new POINT[nAllPts];
#else
		lpPoint = new POINT[nAllPts];
#endif

		int nCount = 0;
		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			for (int i = 0;i < nExteriorPts ; i++,nCount++)
			{
				LPToDP(pLinerring->GetX(i),pLinerring->GetY(i),lpPoint[i].x,lpPoint[i].y);
				//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				DrawCross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}

			for (int i = 0; i < nInteriorRings ;i++)
			{
				const SmtLinearRing *pInteriorRing = pPloygon->GetInteriorRing(i);
				int nInteriorPts= pInteriorRing->GetNumPoints();
				for ( int j=0; j<nInteriorPts; ++j,nCount++)
				{
					LPToDP(pInteriorRing->GetX(i),pInteriorRing->GetY(i),lpPoint[i].x,lpPoint[i].y);
					//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					DrawCross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
				}
			}

			::PolyPolygon(m_hCurDC,lpPoint,nRings,nInteriorRings+1);
		}
		else
		{
			for (int i = 0;i < nExteriorPts ; i++,nCount++)
			{
				LPToDP(pLinerring->GetX(i),pLinerring->GetY(i),lpPoint[i].x,lpPoint[i].y);
			}

			for (int i = 0; i < nInteriorRings ;i++)
			{
				const SmtLinearRing *pInteriorRing = pPloygon->GetInteriorRing(i);
				int nInteriorPts= pInteriorRing->GetNumPoints();
				for ( int j=0; j<nInteriorPts; ++j,nCount++)
				{
					LPToDP(pInteriorRing->GetX(i),pInteriorRing->GetY(i),lpPoint[i].x,lpPoint[i].y);
				}
			}

			::PolyPolygon(m_hCurDC,lpPoint,nRings,nInteriorRings+1);
		}

#ifdef GDI_USE_BUFPOOL
		if (nAllPts*(sizeof(POINT)) < m_bufPool.GetPoolSize())
		{
			m_bufPool.FreeAllBuf();
		}
		else
			SMT_SAFE_DELETE_A(lpPoint);
#else
		SMT_SAFE_DELETE_A(lpPoint);
#endif

		SMT_SAFE_DELETE_A(nRings);

		return SMT_ERR_NONE;
	}
	
	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderThread::DrawTin(const SmtTin *pTin)
	{
		DrawTinLines(pTin);

		//if (m_rdPra.bShowPoint)
		{
			DrawTinNodes(pTin);
		}	

		return SMT_ERR_NONE;
	}

	//绘制Tin线
	int SmtGdiRenderThread::DrawTinLines(const SmtTin *pTin)
	{
		POINT	 lPt1,lPt2,lPt3;
		SmtPoint oPt1,oPt2,oPt3;

		Envelope envTri,envViewp;
		lRect lViewp;
		fRect fViewp;

		ViewportToRect(lViewp,m_smtRC.viewport);
		DRectToLRect(lViewp,fViewp);
		RectToEnvelope(envViewp,fViewp);

		for (int i = 0; i < pTin->GetTriangleCount();i++)
		{
			SmtTriangle tri = pTin->GetTriangle(i);

			if (!tri.bDelete)
			{
				oPt1 = pTin->GetPoint(tri.a);
				oPt2 = pTin->GetPoint(tri.b);
				oPt3 = pTin->GetPoint(tri.c);

				envTri.Merge(oPt1.GetX(),oPt1.GetY());
				envTri.Merge(oPt2.GetX(),oPt2.GetY());
				envTri.Merge(oPt3.GetX(),oPt3.GetY());

				if (envTri.Intersects(envViewp))
				{
					LPToDP(oPt1.GetX(),oPt1.GetY(),lPt1.x,lPt1.y);
					LPToDP(oPt2.GetX(),oPt2.GetY(),lPt2.x,lPt2.y);
					LPToDP(oPt3.GetX(),oPt3.GetY(),lPt3.x,lPt3.y);

					MoveToEx(m_hCurDC,lPt1.x, lPt1.y, NULL);
					LineTo(m_hCurDC,lPt2.x,lPt2.y);
					LineTo(m_hCurDC,lPt3.x,lPt3.y);
					LineTo(m_hCurDC,lPt1.x,lPt1.y);
				}
			}
		}

		return SMT_ERR_NONE;
	}

	//绘制Tin节点
	int SmtGdiRenderThread::DrawTinNodes(const SmtTin *pTin)
	{
		POINT		lPt;
		SmtPoint	oPt;
		int			r = m_rdPra.lPointRaduis;

		lRect lViewp;
		fRect fViewp;

		ViewportToRect(lViewp,m_smtRC.viewport);
		DRectToLRect(lViewp,fViewp);
		AjustfRect(fViewp);

		for (int i = 0; i < pTin->GetPointCount(); i++)
		{
			oPt = pTin->GetPoint(i);
			if (IsInfRect(oPt.GetX(),oPt.GetY(),fViewp) )
			{
				LPToDP(oPt.GetX(),oPt.GetY(),lPt.x,lPt.y);
				Ellipse(m_hCurDC,lPt.x - r ,lPt.y - r,lPt.x + r ,lPt.y + r);
			}
		}

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	int  SmtGdiRenderThread::DrawGrid(const SmtGrid *pGrid)
	{
		DrawGridLines(pGrid);

		//if (m_rdPra.bShowPoint)
		{
			DrawGridNodes(pGrid);
		}	

		return SMT_ERR_NONE;
	}

	//绘制网格线
	int SmtGdiRenderThread::DrawGridLines(const SmtGrid *pGrid)
	{
		const Matrix2D<RawPoint>  *pNodes = pGrid->GetGridNodeBuf();

		int nM,nN;
		pGrid->GetSize(nM,nN);

		POINT lPt;

		for (int j = 0; j < nN; j ++)
		{//绘制列
			RawPoint rawPt = pNodes->GetElement(0,j);
			LPToDP(rawPt.x,rawPt.y,lPt.x,lPt.y);
			MoveToEx(m_hCurDC,lPt.x, lPt.y, NULL);
			for (int i = 0; i < nM; i++)
			{//绘制行
				RawPoint rawPt1 = pNodes->GetElement(i,j);
				LPToDP(rawPt1.x,rawPt1.y,lPt.x,lPt.y);
				LineTo(m_hCurDC,lPt.x,lPt.y);
			}	 
		}

		for (int i = 0; i < nM ; i ++)
		{//绘制列
			RawPoint rawPt = pNodes->GetElement(i,0);
			LPToDP(rawPt.x,rawPt.y,lPt.x,lPt.y);
			MoveToEx(m_hCurDC,lPt.x, lPt.y, NULL);

			for (int j = 0; j < nN ;j ++)
			{//绘制行
				RawPoint rawPt1 = pNodes->GetElement(i,j);
				LPToDP(rawPt1.x,rawPt1.y,lPt.x,lPt.y);
				LineTo(m_hCurDC,lPt.x,lPt.y);
			}
		}

		return SMT_ERR_NONE;
	}

	//绘制网格节点
	int SmtGdiRenderThread::DrawGridNodes(const SmtGrid *pGrid)
	{
		const Matrix2D<RawPoint>  *pNodes = pGrid->GetGridNodeBuf();

		int nM,nN;
		pGrid->GetSize(nM,nN);

		int r = m_rdPra.lPointRaduis;
		POINT lPt;

		for (int j = 0; j < nN; j ++)
		{
			for (int i = 0; i < nM; i++)
			{
				RawPoint rawPt = pNodes->GetElement(i,j);
				LPToDP(rawPt.x,rawPt.y,lPt.x,lPt.y);
				Ellipse(m_hCurDC,lPt.x - r ,lPt.y - r,lPt.x + r ,lPt.y + r);
			}
		}

		return SMT_ERR_NONE;
	}

	int  SmtGdiRenderThread::DrawFan(const SmtFan *pFan)
	{
		long x1,y1,x2,y2,x3,y3,x4,y4,x5,y5;
		SmtPoint oStPoint,oEdbfPoint,oCtPoint;

		const  SmtArc *pArc = pFan->GetArc();
		pArc->StartPoint(&oStPoint);
		pArc->EndPoint(&oEdbfPoint);
		pArc->GetCenterPoint(&oCtPoint);

		LPToDP(oStPoint.GetX(),oStPoint.GetY(),x3,y3);
		LPToDP(oEdbfPoint.GetX(),oEdbfPoint.GetY(),x4,y4);
		LPToDP(oCtPoint.GetX(),oCtPoint.GetY(),x5,y5);

		int dr = m_rdPra.lPointRaduis;
		float r = SmtDistance(x4,y4,x5,y5);

		x1 = x5 - r;
		y1 = y5 - r;
		x2 = x5 + r;
		y2 = y5 + r;

		::MoveToEx(m_hCurDC,x4,y4,NULL);
		LineTo(m_hCurDC,x5,y5);
		LineTo(m_hCurDC,x3,y3);
		::Pie(m_hCurDC,x1,y1,x2,y2,x3,y3,x4,y4);

		Ellipse(m_hCurDC,x3 - dr ,y3 - dr,x3 + dr ,y3 + dr);
		Ellipse(m_hCurDC,x4 - dr ,y4 - dr,x4 + dr ,y4 + dr);
		Ellipse(m_hCurDC,x5 - dr ,y5 - dr,x5 + dr ,y5 + dr);

		if (m_rdPra.bShowPoint)
		{
			//Rectangle(m_hCurDC,x3 - dr ,y3 - dr,x3 + dr ,y3 + dr);
			//Rectangle(m_hCurDC,x4 - dr ,y4 - dr,x4 + dr ,y4 + dr);
			//Rectangle(m_hCurDC,x5 - dr ,y5 - dr,x5 + dr ,y5 + dr);

			DrawCross(m_hCurDC,x3,y3,dr);
			DrawCross(m_hCurDC,x4,y4,dr);
			DrawCross(m_hCurDC,x5,y5,dr);
		}

		return SMT_ERR_NONE;
	}

	int  SmtGdiRenderThread::DrawArc(const SmtArc *pArc)
	{
		long x1,y1,x2,y2,x3,y3,x4,y4,x5,y5;
		SmtPoint oStPoint,oEdbfPoint,oCtPoint;

		pArc->StartPoint(&oStPoint);
		pArc->EndPoint(&oEdbfPoint);
		pArc->GetCenterPoint(&oCtPoint);

		LPToDP(oStPoint.GetX(),oStPoint.GetY(),x3,y3);
		LPToDP(oEdbfPoint.GetX(),oEdbfPoint.GetY(),x4,y4);
		LPToDP(oCtPoint.GetX(),oCtPoint.GetY(),x5,y5);

		int dr = m_rdPra.lPointRaduis;
		float r = SmtDistance(x4,y4,x5,y5);

		x1 = x5 - r;
		y1 = y5 - r;
		x2 = x5 + r;
		y2 = y5 + r;

		//::MoveToEx(m_hCurDC,x4,y4,NULL);
		//LineTo(m_hCurDC,x5,y5);
		//LineTo(m_hCurDC,x3,y3);
		::Arc(m_hCurDC,x1,y1,x2,y2,x3,y3,x4,y4);

		Ellipse(m_hCurDC,x3 - dr ,y3 - dr,x3 + dr ,y3 + dr);
		Ellipse(m_hCurDC,x4 - dr ,y4 - dr,x4 + dr ,y4 + dr);
		Ellipse(m_hCurDC,x5 - dr ,y5 - dr,x5 + dr ,y5 + dr);

		if (m_rdPra.bShowPoint)
		{
			//Rectangle(m_hCurDC,x3 - dr ,y3 - dr,x3 + dr ,y3 + dr);
			//Rectangle(m_hCurDC,x4 - dr ,y4 - dr,x4 + dr ,y4 + dr);
			//Rectangle(m_hCurDC,x5 - dr ,y5 - dr,x5 + dr ,y5 + dr);

			DrawCross(m_hCurDC,x3,y3,dr);
			DrawCross(m_hCurDC,x4,y4,dr);
			DrawCross(m_hCurDC,x5,y5,dr);
		}

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderThread::DrawEllipse(float left,float top,float right,float bottom,bool bDP )
	{
		lRect lrect;

		fRect frect;
		frect.lb.x = left;
		frect.lb.y = bottom;
		frect.rt.x = right;
		frect.rt.y = top;

		if (!bDP)
			LRectToDRect(frect,lrect);
		else
			fRectTolRect(lrect,frect);

		Ellipse(m_hCurDC,lrect.lb.x ,lrect.lb.y,lrect.rt.x ,lrect.rt.y);

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawRect(const fRect &rect,bool bDP )
	{
		lRect tmpRectDP;

		fRectTolRect(tmpRectDP,rect);

		if (!bDP)
		{
			fRect tmpRectLP;

			lRectTofRect(tmpRectLP,tmpRectDP);
			LRectToDRect(tmpRectLP,tmpRectDP);
		}

		MoveToEx(m_hCurDC,tmpRectDP.lb.x,tmpRectDP.lb.y, NULL) ;
		LineTo(m_hCurDC,tmpRectDP.rt.x,tmpRectDP.lb.y);
		LineTo(m_hCurDC,tmpRectDP.rt.x,tmpRectDP.rt.y);
		LineTo(m_hCurDC,tmpRectDP.lb.x,tmpRectDP.rt.y);
		LineTo(m_hCurDC,tmpRectDP.lb.x,tmpRectDP.lb.y);

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawLine(fPoint *pfPoints,int nCount,bool bDP)
	{
		int    nPoints = nCount;
		if (nPoints < 2)
			return SMT_ERR_INVALID_PARAM;

		int i = 0;
		POINT *lpPoint = NULL;

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < (m_bufPool.GetBufCount()*m_bufPool.GetSizePerBuf()))
		{
			m_bufPool.FreeAllBuf();
			lpPoint = (POINT *)m_bufPool.NewBuf();
		}
		else
			lpPoint = new POINT[nPoints];
#else
		lpPoint = new POINT[nPoints];
#endif

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			if (!bDP)
			{
				for (int i = 0;i < nPoints ; i++)
				{
					LPToDP(pfPoints[i].x,pfPoints[i].y,lpPoint[i].x,lpPoint[i].y);
					//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					DrawCross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
				}
			}
			else
			{
				for (int i = 0;i < nPoints ; i++)
				{
					lpPoint[i].x = pfPoints[i].x;
					lpPoint[i].y = pfPoints[i].y;
					//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					DrawCross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
				}
			}
		}
		else
		{
			if (!bDP)
			{
				for (int i = 0;i < nPoints ; i++)
				{
					LPToDP(pfPoints[i].x,pfPoints[i].y,lpPoint[i].x,lpPoint[i].y);
				}
			}
			else
			{
				for (int i = 0;i < nPoints ; i++)
				{
					lpPoint[i].x = pfPoints[i].x;
					lpPoint[i].y = pfPoints[i].y;
				}
			}
		}

		MoveToEx (m_hCurDC, lpPoint[0].x, lpPoint[0].y, NULL) ;    
		PolylineTo(m_hCurDC, lpPoint, nPoints);

#ifdef GDI_USE_BUFPOOL
		if (nPoints*(sizeof(POINT)) < (m_bufPool.GetBufCount()*m_bufPool.GetSizePerBuf()))
		{
			m_bufPool.FreeAllBuf();
		}
		else
			SMT_SAFE_DELETE_A(lpPoint);
#else
		SMT_SAFE_DELETE_A(lpPoint);
#endif

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawLine(const fPoint &ptA,const fPoint &ptB,bool bDP)
	{
		lPoint pt1(ptA.x,ptA.y),pt2(ptB.x,ptB.y);

		if (!bDP)
		{
			LPToDP(ptA.x,ptA.y,pt1.x,pt1.y);
			LPToDP(ptB.x,ptB.y,pt2.x,pt2.y);
		}

		MoveToEx(m_hCurDC,pt1.x,pt1.y, NULL) ;
		LineTo(m_hCurDC,pt2.x,pt2.y);

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawText(const char *szAnno,float fangel,float fCHeight,float fCWidth,float fCSpace,const fPoint &point,bool bDP)
	{
		if (szAnno == NULL)
			return SMT_ERR_INVALID_PARAM;

		fCHeight *= m_smtRC.fblc;
		fCWidth *= m_smtRC.fblc;
		fCSpace *= m_smtRC.fblc;

		unsigned char c1,c2;
		fPoint pt;
		long x,y;
		char bz[4];
		const char *ls1;
		ls1 = szAnno;

		lRect tmpRectDP;

		if (!bDP)
		{
			LPToDP(point.x,point.y,x,y);
			pt.x = x;
			pt.y = y;
		}
		else
		{
			pt.x = point.x;
			pt.y = point.y;
		}

		pt.x -= 2*fCHeight*sin(fangel);
		pt.y -= 2*fCHeight*cos(fangel);

		int nStrLength  = (int)strlen(ls1);
		while(nStrLength > 0)
		{
			c1 = *ls1;
			c2 = *(ls1 + 1);
			if(c1 >127 && c2 > 127) //如果下一个字符是汉字
			{
				strncpy(bz,ls1,2);
				bz[2] = 0;
				ls1 = ls1 + 2;
				TextOut(m_hCurDC,pt.x,pt.y,(LPCSTR)bz,2);
				nStrLength -= 2;
				pt.x += (fCWidth*2 + fCSpace) * cos(fangel);
				pt.y += (fCWidth*2 + fCSpace) * sin(fangel);
			}
			else
			{
				strncpy(bz,ls1,1);
				bz[1] = 0;
				ls1++;
				TextOut(m_hCurDC,pt.x,pt.y,(LPCSTR)bz,1);
				nStrLength -= 1;

				pt.x += (fCWidth + fCSpace/2.) * cos(fangel);
				pt.y += (fCWidth + fCSpace/2.) * sin(fangel);
			}
		}

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			long lX,lY;
			if (!bDP)
			{
				LPToDP(point.x,point.y,lX,lY);
			}
			else
			{
				lX = point.x;
				lY = point.y;
			}

			//Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);
			DrawCross(m_hCurDC,lX,lY,r);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawImage(const char *szImageBuf,int nImageBufSize,const fRect &frect,long lCodeType)
	{
		lRect lrt;
		LRectToDRect(frect,lrt);

		CxImage tmpImage;
		tmpImage.Decode((BYTE*)szImageBuf,nImageBufSize,lCodeType);
		tmpImage.Draw(m_hCurDC,lrt.lb.x,lrt.rt.y,lrt.Width(),lrt.Height());

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::StrethImage(const char *szImageBuf,int nImageBufSize,const fRect &frect,long lCodeType)
	{
		lRect lrt;
		LRectToDRect(frect,lrt);

		CxImage tmpImage;
		tmpImage.Decode((BYTE*)szImageBuf,nImageBufSize,lCodeType);
		tmpImage.Stretch(m_hCurDC,lrt.lb.x,lrt.rt.y,lrt.Width(),lrt.Height());

		return SMT_ERR_NONE;
	}
}