#include <math.h>

#include "legacy_render/gdi/gdi_renderthread.h"
#include "base/core/logmanager.h"
#include "base/style/style_api.h"
#include "legacy_render/gdi/gdi_aux_api.h"
#include "base/core/api.h"
#include "ximage.h"
#include "legacy_render/gdi/resource.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"
#include "ogrsf_frmts.h"

namespace render
{
	int SmtGdiRenderThread::RenderLayer(const SmtLayer *pLayer,int op)
	{
		if(pLayer->GetLayerType() == LYR_RASTER)
			return RenderLayer((SmtRasterLayer*)pLayer,op);
		else if(pLayer->GetLayerType() == LYR_TITLE)
			return RenderLayer((SmtTileLayer*)pLayer,op);

		return SMT_ERR_FAILURE;
	}

	int SmtGdiRenderThread::RenderLayer(OGRLayer *pLayer,int op)
	{
		if (NULL == pLayer)
			return SMT_ERR_INVALID_PARAM;

		Envelope envLayer;
		OGREnvelope ogr_env;
		if (pLayer->GetExtent(&ogr_env, TRUE) == OGRERR_NONE) {
			envLayer.MinX = ogr_env.MinX;
			envLayer.MinY = ogr_env.MinY;
			envLayer.MaxX = ogr_env.MaxX;
			envLayer.MaxY = ogr_env.MaxY;
		}
		Envelope envViewp;

		lRect lViewp;
		fRect fViewp;

		viewport_to_rect(lViewp,m_smtRC.viewport);
		DRectToLRect(lViewp,fViewp);
		rect_to_envelope(envViewp,fViewp);

		if (!envLayer.intersects(envViewp))
			return SMT_ERR_NONE;

		pLayer->ResetReading();
		while (OGRFeature* feat = pLayer->GetNextFeature())
		{
			RenderFeature(feat,op);
			OGRFeature::DestroyFeature(feat);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::RenderLayer(const SmtRasterLayer *pLayer,int op)
	{
		if (NULL == pLayer)
			return SMT_ERR_INVALID_PARAM;

		if (!pLayer->IsVisible())
			return SMT_ERR_NONE; 

		Envelope envLayer ;
		pLayer->get_envelope(envLayer);
		Envelope envViewp;

		lRect lViewp;
		fRect fViewp;

		viewport_to_rect(lViewp,m_smtRC.viewport);
		DRectToLRect(lViewp,fViewp);
		rect_to_envelope(envViewp,fViewp);

		if (!envLayer.intersects(envViewp))
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
		pLayer->get_envelope(envLayer);
		Envelope envViewp;

		lRect lViewp;
		fRect fViewp;
		lRect titleDPRect;

		viewport_to_rect(lViewp,m_smtRC.viewport);
		DRectToLRect(lViewp,fViewp);
		rect_to_envelope(envViewp,fViewp);

		if (!envLayer.intersects(envViewp))
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
				rect_to_envelope(envTile,pTile->rtTileRect);
				LRectToDRect(pTile->rtTileRect,titleDPRect);

				if (!envTile.intersects(envViewp) ||
					(titleDPRect.height() < 2 && titleDPRect.width() < 2))
					return SMT_ERR_NONE;*/

				StrethImage(pTile->pTileBuf,pTile->lTileBufSize,pTile->rtTileRect,pTile->lImageCode);
			}			

			pLayer->MoveNext();
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::RenderFeature(OGRFeature *pFeature,int op)
	{
		if (NULL == pFeature)
			return SMT_ERR_INVALID_PARAM;

		m_nFeatureType = sdb::datasource::infer_feature_type(
			pFeature, SmtFeatureType::SmtFtUnknown);
		SmtStyle* pStyle = sdb::datasource::copy_ogr_style_from_ogr(pFeature);
		OGRGeometry* pGeom = sdb::datasource::decode_ogr_geometry(
			pFeature, static_cast<SmtFeatureType>(m_nFeatureType));
		if (m_nFeatureType == SmtFeatureType::SmtFtAnno) {
			const int ai = pFeature->GetFieldIndex("anno");
			const int gi = pFeature->GetFieldIndex("angle");
			if (ai >= 0) {
				sprintf(m_szAnno, "%s", pFeature->GetFieldAsString(ai));
			}
			if (gi >= 0) {
				m_fAnnoAngle = static_cast<float>(pFeature->GetFieldAsDouble(gi));
			}
		}
		const int rc = RenderGeometry(pGeom, pStyle, op);
		delete pGeom;
		delete pStyle;
		return rc;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderThread::PrepareForDrawing( const  SmtStyle*pStyle,int nDrawMode)
	{
		::SetROP2(m_hCurDC,nDrawMode);

		m_bCurUseStyle	= (NULL != pStyle);

		if (m_bCurUseStyle)
		{
			ulong format = pStyle->get_style_type();
			if( format & ST_PenDesc )
			{
				SmtPenDesc pen = pStyle->get_pen_desc();

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
				SmtBrushDesc brush = pStyle->get_brush_desc();

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
				SmtSymbolDesc symbol = pStyle->get_symbol_desc();

				if (m_hIcon)
				{
					DeleteObject(m_hIcon);
					m_hIcon = NULL;
				}

				m_hIcon = LoadIcon(m_hInst,MAKEINTRESOURCE(symbol.lSymbolID+IDI_ICON_A));
			}

			if (format & ST_AnnoDesc)
			{
				SmtAnnotationDesc anno = pStyle->get_anno_desc();

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

	int SmtGdiRenderThread::RenderGeometry( const  OGRGeometry *pGeom,const SmtStyle*pStyle,int op)
	{
		if (!pGeom)
			return SMT_ERR_INVALID_PARAM;

		const OGRwkbGeometryType type = wkbFlatten(pGeom->getGeometryType());

		Envelope envFeature,envViewp;
		geo::copy_envelope(*pGeom, &envFeature);

		lRect lViewp;
		fRect fViewp;
		fRect fenv;
		lRect lenv;

		viewport_to_rect(lViewp,m_smtRC.viewport);
		DRectToLRect(lViewp,fViewp);
		rect_to_envelope(envViewp,fViewp);

		envelope_to_rect(fenv,envFeature);
		LRectToDRect(fenv,lenv);

		if (!envFeature.intersects(envViewp) ||
			(type != wkbPoint && lenv.height() < 2 && lenv.width() < 2))
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

		switch (type) {
		case wkbPoint:
			DrawPoint(pStyle, (OGRPoint*)pGeom);
			break;
		case wkbLineString:
			DrawLineString((OGRLineString*)pGeom);
			break;
		case wkbPolygon:
		case wkbTriangle:
			DrawPloygon((OGRPolygon*)pGeom);
			break;
		case wkbMultiPoint:
			DrawMultiPoint(pStyle, (OGRMultiPoint*)pGeom);
			break;
		case wkbMultiLineString:
			DrawMultiLineString((OGRMultiLineString*)pGeom);
			break;
		case wkbMultiPolygon:
		case wkbTIN:
			DrawMultiPolygon((OGRMultiPolygon*)pGeom);
			break;
		case wkbLinearRing:
			DrawLinearRing((OGRLinearRing*)pGeom);
			break;
		default:
			break;
		}
		if(!m_bLockStyle)
			EndDrawing();

		::RestoreDC(m_hCurDC,-1);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderThread::DrawMultiLineString( const  OGRMultiLineString *pMultiLinestring)
	{
		int nLines = pMultiLinestring->getNumGeometries();

		int i = 0;
		while (i < nLines)
		{
			DrawLineString((OGRLineString*)pMultiLinestring->getGeometryRef(i));
			i++;
		}

		return SMT_ERR_FAILURE;
	}

	int SmtGdiRenderThread::DrawMultiPoint( const  SmtStyle*pStyle, const  OGRMultiPoint *pMultiPoint)
	{
		int nPoints = pMultiPoint->getNumGeometries();

		int i = 0;
		while (i < nPoints)
		{
			DrawPoint(pStyle,(OGRPoint*)pMultiPoint->getGeometryRef(i));
			i++;
		}

		return SMT_ERR_FAILURE;
	}

	int SmtGdiRenderThread::DrawMultiPolygon( const  OGRMultiPolygon *pMultiPolygon)
	{
		int nPolygons = pMultiPolygon->getNumGeometries();

		int i = 0;
		while (i < nPolygons)
		{
			DrawPloygon((OGRPolygon*)pMultiPolygon->getGeometryRef(i));
			i++;
		}

		return SMT_ERR_FAILURE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderThread::DrawPoint(const SmtStyle*pStyle,const OGRPoint *pPoint)
	{
		ulong format = pStyle->get_style_type();
		if (m_nFeatureType == SmtFeatureType::SmtFtAnno)
		{
			SmtAnnotationDesc anno = pStyle->get_anno_desc();
			return DrawAnno(m_szAnno,m_fAnnoAngle,abs(anno.fHeight),abs(anno.fWidth),abs(anno.fSpace),pPoint);
		}
		else if(m_nFeatureType == SmtFeatureType::SmtFtChildImage)
		{
			SmtSymbolDesc symbol = pStyle->get_symbol_desc();
			return DrawSymbol(m_hIcon,symbol.fSymbolHeight,symbol.fSymbolWidth,pPoint);
		}
		else if (m_nFeatureType == SmtFeatureType::SmtFtDot)
		{
			int r = m_rdPra.lPointRaduis/**m_smtRC.fblc*/;
			long lX,lY;
			LPToDP(pPoint->getX(),pPoint->getY(),lX,lY);
			Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);

			return SMT_ERR_NONE;
		}	

		return SMT_ERR_FAILURE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtGdiRenderThread::DrawAnno(const char *szAnno,float fangel,float fCHeight,float fCWidth,float fCSpace,const OGRPoint *pPoint)
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

		LPToDP(pPoint->getX(),pPoint->getY(),x,y);
		pt.x = x;
		pt.y = y;

		pt.x -= 2*fCHeight*sin(fangel);
		pt.y -= 2*fCHeight*cos(fangel);

		int nStrLength  = (int)strlen(ls1);
		while(nStrLength > 0)
		{
			c1 = *ls1;
			c2 = *(ls1 + 1);
			if(c1 >127 && c2 > 127) //�����һ���ַ��Ǻ���
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
			LPToDP(pPoint->getX(),pPoint->getY(),lX,lY);
			draw_cross(m_hCurDC,lX,lY,m_rdPra.lPointRaduis);
			//Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);
			draw_cross(m_hCurDC,lX,lY,r);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawSymbol(HICON hIcon,long lHeight,long lWidth,const OGRPoint *pPoint)
	{
		lHeight *= m_smtRC.fblc;
		lWidth *= m_smtRC.fblc;

		long lX,lY;
		LPToDP(pPoint->getX(),pPoint->getY(),lX,lY);
		//::DrawIcon(m_hCurDC,pt.x-lWidth,pt.y-lHeight,hIcon);
		::DrawIconEx(m_hCurDC,lX-lWidth/2,lY+lHeight/2,   hIcon, lWidth, lHeight, 0, NULL, DI_NORMAL);
		//::DrawState( m_hCurDC,NULL,NULL,(LPARAM)hIcon,0,pt.x-lWidth/2,pt.y+lHeight/2,lWidth,lHeight, DSS_NORMAL | DST_ICON);

		if (m_rdPra.bShowPoint)
		{
			int r = m_rdPra.lPointRaduis;
			//Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			//Rectangle(m_hCurDC,lX - r,lY - r,lX + r,lY + r);
			draw_cross(m_hCurDC,lX,lY,r);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawLineSpline(const OGRLineString *pSpline)
	{
		int    nPoints = pSpline->getNumPoints();
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
			LPToDP(pSpline->getX(i),pSpline->getY(i),lpPoint[i].x,lpPoint[i].y);
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
			for (int i = 0; i < pSpline->getNumPoints();i++)
			{
				LPToDP(pSpline->getX(i),pSpline->getY(i),lX,lY);
				Ellipse(m_hCurDC,lX - r ,lY - r,lX + r ,lY + r);
			}
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawLineString(const OGRLineString *pLinestring)
	{
		int    nPoints = pLinestring->getNumPoints();
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
				LPToDP(pLinestring->getX(i),pLinestring->getY(i),lpPoint[i].x,lpPoint[i].y);
				//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				draw_cross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}
		}
		else
		{
			for (int i = 0;i < nPoints ; i++)
			{
				LPToDP(pLinestring->getX(i),pLinestring->getY(i),lpPoint[i].x,lpPoint[i].y);
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

	int SmtGdiRenderThread::DrawLinearRing(const OGRLinearRing *pLinearRing)
	{
		int    nPoints = pLinearRing->getNumPoints();
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
			for (int i = 0; i < pLinearRing->getNumPoints();i++)
			{
				LPToDP(pLinearRing->getX(i),pLinearRing->getY(i),lpPoint[i].x,lpPoint[i].y);
				//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				draw_cross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}
		}
		else
		{
			for (int i = 0;i < nPoints ; i++)
			{
				LPToDP(pLinearRing->getX(i),pLinearRing->getY(i),lpPoint[i].x,lpPoint[i].y);
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

	int SmtGdiRenderThread::DrawPloygon(const OGRPolygon *pPloygon)
	{
		int nAllPts = 0;
		const OGRLinearRing *pLinerring = pPloygon->getExteriorRing();

		int    nExteriorPts = pLinerring->getNumPoints();
		if (nExteriorPts < 2)
			return SMT_ERR_INVALID_PARAM;

		nAllPts += nExteriorPts;

		int nInteriorRings = pPloygon->getNumInteriorRings();
		int *nRings = new int[nInteriorRings + 1];
		nRings[0] = nExteriorPts;

		for (int i = 0; i < nInteriorRings ; i++)
		{
			const OGRLinearRing *pInteriorRing = pPloygon->getInteriorRing(i);
			nRings[i+1] = pInteriorRing->getNumPoints();
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
				LPToDP(pLinerring->getX(i),pLinerring->getY(i),lpPoint[i].x,lpPoint[i].y);
				//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
				draw_cross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
			}

			for (int i = 0; i < nInteriorRings ;i++)
			{
				const OGRLinearRing *pInteriorRing = pPloygon->getInteriorRing(i);
				int nInteriorPts= pInteriorRing->getNumPoints();
				for ( int j=0; j<nInteriorPts; ++j,nCount++)
				{
					LPToDP(pInteriorRing->getX(i),pInteriorRing->getY(i),lpPoint[i].x,lpPoint[i].y);
					//Ellipse(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					//Rectangle(m_hCurDC,lpPoint[i].x - r ,lpPoint[i].y - r,lpPoint[i].x + r ,lpPoint[i].y + r);
					draw_cross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
				}
			}

			::PolyPolygon(m_hCurDC,lpPoint,nRings,nInteriorRings+1);
		}
		else
		{
			for (int i = 0;i < nExteriorPts ; i++,nCount++)
			{
				LPToDP(pLinerring->getX(i),pLinerring->getY(i),lpPoint[i].x,lpPoint[i].y);
			}

			for (int i = 0; i < nInteriorRings ;i++)
			{
				const OGRLinearRing *pInteriorRing = pPloygon->getInteriorRing(i);
				int nInteriorPts= pInteriorRing->getNumPoints();
				for ( int j=0; j<nInteriorPts; ++j,nCount++)
				{
					LPToDP(pInteriorRing->getX(i),pInteriorRing->getY(i),lpPoint[i].x,lpPoint[i].y);
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

	//����Tin��
	int SmtGdiRenderThread::DrawTinLines(const SmtTin *pTin)
	{
		POINT	 lPt1,lPt2,lPt3;
		OGRPoint oPt1,oPt2,oPt3;

		Envelope envTri,envViewp;
		lRect lViewp;
		fRect fViewp;

		viewport_to_rect(lViewp,m_smtRC.viewport);
		DRectToLRect(lViewp,fViewp);
		rect_to_envelope(envViewp,fViewp);

		for (int i = 0; i < pTin->get_triangle_count();i++)
		{
			SmtTriangle tri = pTin->get_triangle(i);

			if (!tri.bDelete)
			{
				oPt1 = pTin->get_point(tri.a);
				oPt2 = pTin->get_point(tri.b);
				oPt3 = pTin->get_point(tri.c);

				envTri.merge(oPt1.getX(),oPt1.getY());
				envTri.merge(oPt2.getX(),oPt2.getY());
				envTri.merge(oPt3.getX(),oPt3.getY());

				if (envTri.intersects(envViewp))
				{
					LPToDP(oPt1.getX(),oPt1.getY(),lPt1.x,lPt1.y);
					LPToDP(oPt2.getX(),oPt2.getY(),lPt2.x,lPt2.y);
					LPToDP(oPt3.getX(),oPt3.getY(),lPt3.x,lPt3.y);

					MoveToEx(m_hCurDC,lPt1.x, lPt1.y, NULL);
					LineTo(m_hCurDC,lPt2.x,lPt2.y);
					LineTo(m_hCurDC,lPt3.x,lPt3.y);
					LineTo(m_hCurDC,lPt1.x,lPt1.y);
				}
			}
		}

		return SMT_ERR_NONE;
	}

	//����Tin�ڵ�
	int SmtGdiRenderThread::DrawTinNodes(const SmtTin *pTin)
	{
		POINT		lPt;
		OGRPoint	oPt;
		int			r = m_rdPra.lPointRaduis;

		lRect lViewp;
		fRect fViewp;

		viewport_to_rect(lViewp,m_smtRC.viewport);
		DRectToLRect(lViewp,fViewp);
		adjust_f_rect(fViewp);

		for (int i = 0; i < pTin->get_point_count(); i++)
		{
			oPt = pTin->get_point(i);
			if (is_in_f_rect(oPt.getX(),oPt.getY(),fViewp) )
			{
				LPToDP(oPt.getX(),oPt.getY(),lPt.x,lPt.y);
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

	//����������
	int SmtGdiRenderThread::DrawGridLines(const SmtGrid *pGrid)
	{
		int nM,nN;
		pGrid->get_size(nM,nN);

		POINT lPt;

		for (int j = 0; j < nN; j ++)
		{//������
			RawPoint rawPt = pGrid->node(0,j);
			LPToDP(rawPt.x,rawPt.y,lPt.x,lPt.y);
			MoveToEx(m_hCurDC,lPt.x, lPt.y, NULL);
			for (int i = 0; i < nM; i++)
			{//������
				RawPoint rawPt1 = pGrid->node(i,j);
				LPToDP(rawPt1.x,rawPt1.y,lPt.x,lPt.y);
				LineTo(m_hCurDC,lPt.x,lPt.y);
			}	 
		}

		for (int i = 0; i < nM ; i ++)
		{//������
			RawPoint rawPt = pGrid->node(i,0);
			LPToDP(rawPt.x,rawPt.y,lPt.x,lPt.y);
			MoveToEx(m_hCurDC,lPt.x, lPt.y, NULL);

			for (int j = 0; j < nN ;j ++)
			{//������
				RawPoint rawPt1 = pGrid->node(i,j);
				LPToDP(rawPt1.x,rawPt1.y,lPt.x,lPt.y);
				LineTo(m_hCurDC,lPt.x,lPt.y);
			}
		}

		return SMT_ERR_NONE;
	}

	//��������ڵ�
	int SmtGdiRenderThread::DrawGridNodes(const SmtGrid *pGrid)
	{
		int nM,nN;
		pGrid->get_size(nM,nN);

		int r = m_rdPra.lPointRaduis;
		POINT lPt;

		for (int j = 0; j < nN; j ++)
		{
			for (int i = 0; i < nM; i++)
			{
				RawPoint rawPt = pGrid->node(i,j);
				LPToDP(rawPt.x,rawPt.y,lPt.x,lPt.y);
				Ellipse(m_hCurDC,lPt.x - r ,lPt.y - r,lPt.x + r ,lPt.y + r);
			}
		}

		return SMT_ERR_NONE;
	}

	int  SmtGdiRenderThread::DrawFan(const OGRPolygon *pFan)
	{
		return DrawPloygon(pFan);
#if 0
		long x1,y1,x2,y2,x3,y3,x4,y4,x5,y5;
		OGRPoint oStPoint,oEdbfPoint,oCtPoint;

		const  SmtArc *pArc = pFan->GetArc();
		pArc->StartPoint(&oStPoint);
		pArc->EndPoint(&oEdbfPoint);
		pArc->GetCenterPoint(&oCtPoint);

		LPToDP(oStPoint.getX(),oStPoint.getY(),x3,y3);
		LPToDP(oEdbfPoint.getX(),oEdbfPoint.getY(),x4,y4);
		LPToDP(oCtPoint.getX(),oCtPoint.getY(),x5,y5);

		int dr = m_rdPra.lPointRaduis;
		float r = static_cast<float>(hypot(x5 - x4, y5 - y4));

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

			draw_cross(m_hCurDC,x3,y3,dr);
			draw_cross(m_hCurDC,x4,y4,dr);
			draw_cross(m_hCurDC,x5,y5,dr);
		}

		return SMT_ERR_NONE;
#endif
	}

	int  SmtGdiRenderThread::DrawArc(const OGRLineString *pArc)
	{
		return DrawLineString(pArc);
#if 0
		long x1,y1,x2,y2,x3,y3,x4,y4,x5,y5;
		OGRPoint oStPoint,oEdbfPoint,oCtPoint;

		pArc->StartPoint(&oStPoint);
		pArc->EndPoint(&oEdbfPoint);
		pArc->GetCenterPoint(&oCtPoint);

		LPToDP(oStPoint.getX(),oStPoint.getY(),x3,y3);
		LPToDP(oEdbfPoint.getX(),oEdbfPoint.getY(),x4,y4);
		LPToDP(oCtPoint.getX(),oCtPoint.getY(),x5,y5);

		int dr = m_rdPra.lPointRaduis;
		float r = static_cast<float>(hypot(x5 - x4, y5 - y4));

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

			draw_cross(m_hCurDC,x3,y3,dr);
			draw_cross(m_hCurDC,x4,y4,dr);
			draw_cross(m_hCurDC,x5,y5,dr);
		}

		return SMT_ERR_NONE;
#endif
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
			f_rect_to_l_rect(lrect,frect);

		Ellipse(m_hCurDC,lrect.lb.x ,lrect.lb.y,lrect.rt.x ,lrect.rt.y);

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawRect(const fRect &rect,bool bDP )
	{
		lRect tmpRectDP;

		f_rect_to_l_rect(tmpRectDP,rect);

		if (!bDP)
		{
			fRect tmpRectLP;

			l_rect_to_f_rect(tmpRectLP,tmpRectDP);
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
					draw_cross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
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
					draw_cross(m_hCurDC,lpPoint[i].x,lpPoint[i].y,r);
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
			if(c1 >127 && c2 > 127) //�����һ���ַ��Ǻ���
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
			draw_cross(m_hCurDC,lX,lY,r);
		}

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::DrawImage(const char *szImageBuf,int nImageBufSize,const fRect &frect,long lCodeType)
	{
		lRect lrt;
		LRectToDRect(frect,lrt);

		CxImage tmpImage;
		tmpImage.Decode((BYTE*)szImageBuf,nImageBufSize,lCodeType);
		tmpImage.Draw(m_hCurDC,lrt.lb.x,lrt.rt.y,lrt.width(),lrt.height());

		return SMT_ERR_NONE;
	}

	int SmtGdiRenderThread::StrethImage(const char *szImageBuf,int nImageBufSize,const fRect &frect,long lCodeType)
	{
		lRect lrt;
		LRectToDRect(frect,lrt);

		CxImage tmpImage;
		tmpImage.Decode((BYTE*)szImageBuf,nImageBufSize,lCodeType);
		tmpImage.Stretch(m_hCurDC,lrt.lb.x,lrt.rt.y,lrt.width(),lrt.height());

		return SMT_ERR_NONE;
	}
}