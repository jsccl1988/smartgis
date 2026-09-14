#include <cstdint>
#include <math.h>

#include "base/core/api.h"
#include "sdb/carto/style_api.h"
#include "legacy/tool/group/appendfeaturetool.h"
#include "sdb/carto/stylemanager.h"
#include "sdb/feature/feature.h"
#include "sdb/map/map.h"
#include "algorithm/geo/geometry.h"
#include "legacy/tool/group/defs.h"
#include "sys/sysmanager.h"
#include "tool/gestures.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"

#include "legacy/tool/group/resource.h"
#include "legacy/ui/gui/gui_api.h"

#include "ogrsf_frmts.h"

#include <memory>

using namespace render;
using namespace base;
using namespace sdb;
using namespace geo;
using namespace sys;
using namespace base;

const string						CST_STR_APPENDFEATURE_TOOL_NAME	= "???????";

namespace {

OGRFeature* make_ogr_feature(SmtMap* map, OGRGeometry* geom,
			     SmtFeatureType ft, const char* style_name) {
	if (!map || !geom) {
		return nullptr;
	}
	OGRLayer* lyr = map->GetActiveOgrLayer();
	if (!lyr) {
		return nullptr;
	}
	OGRFeature* ogr = OGRFeature::CreateFeature(lyr->GetLayerDefn());
	if (!ogr) {
		return nullptr;
	}
	if (!sdb::datasource::encode_smt_geometry(geom, ogr, ft)) {
		OGRFeature::DestroyFeature(ogr);
		return nullptr;
	}
	if (style_name && style_name[0]) {
		SmtStyleManager* mgr = SmtStyleManager::get_singleton_ptr();
		if (mgr) {
			sdb::datasource::copy_smt_style_to_ogr(mgr->get_style(style_name),
							       ogr);
		}
	}
	return ogr;
}

bool commit_append(sdb::MapEditSession* edits, OGRFeature* ogr) {
	if (!edits || !ogr) {
		if (ogr) {
			OGRFeature::DestroyFeature(ogr);
		}
		return false;
	}
	if (!edits->commit_feature(sdb::EditOp::kAppend, ogr)) {
		OGRFeature::DestroyFeature(ogr);
		return false;
	}
	return true;
}

void set_field_string(OGRFeature* ogr, const char* name, const char* value) {
	if (!ogr || !name) {
		return;
	}
	const int i = ogr->GetFieldIndex(name);
	if (i >= 0) {
		ogr->SetField(i, value);
	}
}

void set_field_int(OGRFeature* ogr, const char* name, int value) {
	if (!ogr || !name) {
		return;
	}
	const int i = ogr->GetFieldIndex(name);
	if (i >= 0) {
		ogr->SetField(i, value);
	}
}

void set_field_double(OGRFeature* ogr, const char* name, double value) {
	if (!ogr || !name) {
		return;
	}
	const int i = ogr->GetFieldIndex(name);
	if (i >= 0) {
		ogr->SetField(i, value);
	}
}

}  // namespace

namespace tool
{
	SmtAppendFeatureTool::SmtAppendFeatureTool():m_pGeom(NULL)
		,m_strAnno("Smart GIS")
		,m_pointType(PT_DOT)
		,m_lineType(LT_LineString)
		,m_regionType(RT_Polygon)
		,m_digitizeKind(0)
	{
		 set_name(CST_STR_APPENDFEATURE_TOOL_NAME.c_str());
	}

	SmtAppendFeatureTool::~SmtAppendFeatureTool()
	{
		this->EndDelegate();
		m_pGeom = NULL;

		UnRegisterMsg();
	}

	int SmtAppendFeatureTool::Init(LPRENDERDEVICE pMrdRenderDevice,SmtMap *pOperSmtMap,HWND hWnd,pfnToolCallBack pfnCallBack,void* pToFollow)
	{
		if (SMT_ERR_NONE != SmtBaseTool::Init(pMrdRenderDevice,pOperSmtMap,hWnd,pfnCallBack,pToFollow))
		{
			return SMT_ERR_FAILURE;
		}

		m_edits = std::make_unique<sdb::MapEditSession>(pOperSmtMap);

		SmtStyleManager * pStyleMgr = SmtStyleManager::get_singleton_ptr();
		SmtStyle *pStyle = pStyleMgr->get_style(m_szStyleName);
		pStyle->set_style_type(ST_PenDesc|ST_BrushDesc|ST_SymbolDesc|ST_AnnoDesc);

		//
		append_func_items("???????",GT_MSG_APPEND_POINT_CHILDIMAGE_FEATURE,FIM_2DMFMENU);
		append_func_items("???????",GT_MSG_APPEND_POINT_ANNO_FEATURE,FIM_2DMFMENU);
		append_func_items("?????",GT_MSG_APPEND_POINT_DOT_FEATURE,FIM_2DMFMENU);

		//
		append_func_items("????????",GT_MSG_APPEND_LINE_LINESTRING_FEATURE,FIM_2DMFMENU);
		append_func_items("????????????????",GT_MSG_APPEND_LINE_SPLINE_LAG_FEATURE,FIM_2DMFMENU);
		append_func_items("????????????",GT_MSG_APPEND_LINE_SPLINE_BZER_FEATURE,FIM_2DMFMENU);
		append_func_items("????B????????",GT_MSG_APPEND_LINE_SPLINE_B_FEATURE,FIM_2DMFMENU);
		append_func_items("????????????????",GT_MSG_APPEND_LINE_SPLINE_3_FEATURE,FIM_2DMFMENU);
		append_func_items("?????????",GT_MSG_APPEND_LINE_RECT_FEATURE,FIM_2DMFMENU);
		append_func_items("???????",GT_MSG_APPEND_LINE_ARC_FEATURE,FIM_2DMFMENU);
		append_func_items("??????????",GT_MSG_APPEND_LINE_LINEARRING_FEATURE,FIM_2DMFMENU);

		//
		append_func_items("??????????",GT_MSG_APPEND_SURF_FAN_FEATURE,FIM_2DMFMENU);
		append_func_items("?????????",GT_MSG_APPEND_SURF_RECT_FEATURE,FIM_2DMFMENU);
		append_func_items("??????????",GT_MSG_APPEND_SURF_POLYGON_FEATURE,FIM_2DMFMENU);

		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_POINT_CHILDIMAGE_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_POINT_ANNO_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_POINT_DOT_FEATURE);

		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_LINESTRING_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_SPLINE_LAG_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_SPLINE_BZER_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_SPLINE_B_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_SPLINE_3_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_RECT_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_ARC_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_LINE_LINEARRING_FEATURE);

		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_SURF_FAN_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_SURF_RECT_FEATURE);
		SMT_IATOOL_APPEND_MSG(GT_MSG_APPEND_SURF_POLYGON_FEATURE);

		RegisterMsg();

		return SMT_ERR_NONE;
	}

	int SmtAppendFeatureTool::AuxDraw()
	{
		return SmtBaseTool::AuxDraw();
	}

	int SmtAppendFeatureTool::Timer()
	{
		return SmtBaseTool::Timer();
	}

	void SmtAppendFeatureTool::SetOperMap(SmtMap *pOperSmtMap)
	{ 
		SmtBaseTool::SetOperMap(pOperSmtMap);

		m_edits = std::make_unique<sdb::MapEditSession>(pOperSmtMap);
	}

	int SmtAppendFeatureTool::notify(long nMsg,SmtListenerMsg &param)
	{
		if (param.hSrcWnd != m_hWnd)
			return SMT_ERR_NONE;

		SmtSysManager * pSysMgr = SmtSysManager::get_singleton_ptr();
		SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

		switch (nMsg)
		{
		case GT_MSG_DEFAULT_PROCESS:
			{

			}
			break;
		case GT_MSG_APPEND_POINT_CHILDIMAGE_FEATURE:
			{
				ushort unType = PT_ChildImage;
				OnInputPointFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_POINT_ANNO_FEATURE:
			{
				ushort unType = PT_Text;
				OnInputPointFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_POINT_DOT_FEATURE:
			{
				ushort unType = PT_DOT;
				OnInputPointFeature(unType);
				param.bModify = true;
			}
			break;
		//////////////////////////////////////////////////////////////////////////
		case GT_MSG_APPEND_LINE_RECT_FEATURE:
			{
				ushort unType = LT_Rect;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_ARC_FEATURE:
			{
				ushort unType = LT_Arc;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_LINEARRING_FEATURE:
			{
				ushort unType = LT_LinearRing;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_LINESTRING_FEATURE:
			{
				ushort unType = LT_LineString;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_SPLINE_LAG_FEATURE:
			{
				ushort unType = LT_Spline_Lag;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_SPLINE_BZER_FEATURE:
			{
				ushort unType = LT_Spline_Bzer;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_SPLINE_B_FEATURE:
			{
				ushort unType = LT_Spline_B;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_LINE_SPLINE_3_FEATURE:
			{
				ushort unType = LT_Spline_3;
				OnInputLineFeature(unType);
				param.bModify = true;
			}
			break;
		//////////////////////////////////////////////////////////////////////////
		case GT_MSG_APPEND_SURF_FAN_FEATURE:
			{
				ushort unType = RT_Fan;
				OnInputRegionFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_SURF_RECT_FEATURE:
			{
				ushort unType = RT_Rect;
				OnInputRegionFeature(unType);
				param.bModify = true;
			}
			break;
		case GT_MSG_APPEND_SURF_POLYGON_FEATURE:
			{
				ushort unType = RT_Polygon;
				OnInputRegionFeature(unType);
				param.bModify = true;
			}
			break;
		//////////////////////////////////////////////////////////////////////////
		default:
			break;
		}
		return SMT_ERR_NONE;
	}

	namespace {
	void add_draft_points(LPRENDERDEVICE device,
			      const tool::Draft& draft,
			      OGRLineString* geom)
	{
		for (const tool::DraftPoint& p : draft.points)
		{
			float x = 0;
			float y = 0;
			device->DPToLP(p.x_px, p.y_px, x, y);
			geom->addPoint(x, y);
		}
	}

	void add_rect_ring(LPRENDERDEVICE device,
			   const tool::Draft& draft,
			   OGRLineString* geom)
	{
		if (draft.points.size() < 2)
		{
			add_draft_points(device, draft, geom);
			return;
		}
		const int32_t x0 = draft.points[0].x_px;
		const int32_t y0 = draft.points[0].y_px;
		const int32_t x1 = draft.points[1].x_px;
		const int32_t y1 = draft.points[1].y_px;
		const tool::DraftPoint corners[] = {
			{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}, {x0, y0},
		};
		for (const tool::DraftPoint& p : corners)
		{
			float x = 0;
			float y = 0;
			device->DPToLP(p.x_px, p.y_px, x, y);
			geom->addPoint(x, y);
		}
	}

	OGRGeometry* geom_from_line_draft(LPRENDERDEVICE device,
					  const tool::Draft& draft,
					  ushort line_type)
	{
		if (line_type == LT_LinearRing || draft.kind == tool::DraftKind::kPolygon)
		{
			OGRLinearRing* ring = new OGRLinearRing();
			if (draft.kind == tool::DraftKind::kRect)
			{
				add_rect_ring(device, draft, ring);
			}
			else
			{
				add_draft_points(device, draft, ring);
			}
			ring->closeRings();
			return ring;
		}
		if (line_type == LT_Spline_Lag || line_type == LT_Spline_Bzer ||
		    line_type == LT_Spline_B || line_type == LT_Spline_3)
		{
			OGRLineString* spline = new OGRLineString();
			add_draft_points(device, draft, spline);
			return spline;
		}
		OGRLineString* line = new OGRLineString();
		if (draft.kind == tool::DraftKind::kRect || line_type == LT_Rect)
		{
			add_rect_ring(device, draft, line);
		}
		else
		{
			add_draft_points(device, draft, line);
		}
		return line;
	}

	OGRGeometry* geom_from_region_draft(LPRENDERDEVICE device,
					    const tool::Draft& draft)
	{
		OGRLinearRing* ring = new OGRLinearRing();
		if (draft.kind == tool::DraftKind::kRect)
		{
			add_rect_ring(device, draft, ring);
		}
		else
		{
			add_draft_points(device, draft, ring);
		}
		ring->closeRings();
		OGRPolygon* poly = new OGRPolygon();
		poly->addRingDirectly(ring);
		return poly;
	}
	}  // namespace

	void SmtAppendFeatureTool::apply_draft(const tool::Draft& draft)
	{
		if (!m_pRenderDevice || draft.points.empty())
		{
			return;
		}

		if (m_digitizeKind == 1)
		{
			float x = 0;
			float y = 0;
			m_pRenderDevice->DPToLP(draft.points[0].x_px, draft.points[0].y_px, x, y);
			m_pGeom = new OGRPoint(x, y);
			AppendPointFeature(m_pointType);
			return;
		}
		if (m_digitizeKind == 2)
		{
			m_pGeom = geom_from_line_draft(m_pRenderDevice, draft, m_lineType);
			AppendLineFeature();
			return;
		}
		if (m_digitizeKind == 3)
		{
			m_pGeom = geom_from_region_draft(m_pRenderDevice, draft);
			AppendRegionFeature();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtAppendFeatureTool::OnInputPointFeature(ushort unType)
	{
		this->EndDelegate();
		m_digitizeKind = 1;
		m_pointType = unType;
		SetActive();
	}

	void SmtAppendFeatureTool::OnInputLineFeature(ushort unType)
	{
		this->EndDelegate();
		m_digitizeKind = 2;
		m_lineType = unType;
		SetActive();
	}

	void SmtAppendFeatureTool::OnInputRegionFeature(ushort unType)
	{
		this->EndDelegate();
		m_digitizeKind = 3;
		m_regionType = unType;
		SetActive();
	}
	//////////////////////////////////////////////////////////////////////////
	void SmtAppendFeatureTool::AppendPointFeature(ushort unType)
	{
		switch (unType)
		{
		case PT_ChildImage:
			{
				AppendChildImageFeature();
			}
			break;
		case PT_Text:
			{
				if (SMT_ERR_NONE == SmtInputTextDlg(m_strAnno))
					AppendTextFeature(m_strAnno.c_str(),0);
			}
			break;
		case PT_DOT:
			{
				AppendDotFeature();
			}
			break;
		}
	}

	void SmtAppendFeatureTool::AppendChildImageFeature()
	{
		if (m_pOperMap)
		{
			SmtSysManager * pSysMgr = SmtSysManager::get_singleton_ptr();
			SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

			OGRFeature* ogr = make_ogr_feature(m_pOperMap, m_pGeom,
							   SmtFeatureType::SmtFtChildImage,
							   styleSonfig.szPointStyle);

			if (commit_append(m_edits.get(), ogr))
			{
				fRect frt;
				Envelope envelope;
				OGRPoint * pPoint = (OGRPoint*)m_pGeom;
				float fMargin = 5./m_pRenderDevice->GetBlc();

				geo::copy_envelope(*m_pGeom, &envelope);
				envelope.merge(pPoint->getX()-fMargin,pPoint->getY()-fMargin);
				envelope.merge(pPoint->getX()+fMargin,pPoint->getY()+fMargin);
				envelope_to_rect(frt,envelope);

				m_pRenderDevice->Refresh(m_pOperMap,frt);
			}
			SMT_SAFE_DELETE(m_pGeom);
		}
	}

	void SmtAppendFeatureTool::AppendTextFeature(const char * szAnno,float fangle)
	{	 
		if (m_pOperMap)
		{
			SmtSysManager * pSysMgr = SmtSysManager::get_singleton_ptr();
			SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

			OGRFeature* ogr = make_ogr_feature(m_pOperMap, m_pGeom,
							   SmtFeatureType::SmtFtAnno,
							   styleSonfig.szPointStyle);
			set_field_string(ogr, "anno", szAnno);
			set_field_int(ogr, "color", int(RGB(0, 0, 0)));
			set_field_double(ogr, "angle", fangle);

			if (commit_append(m_edits.get(), ogr))
			{
				fRect frt;
				Envelope envelope;
				OGRPoint * pPoint = (OGRPoint*)m_pGeom;
				float fMargin = 5./m_pRenderDevice->GetBlc();

				geo::copy_envelope(*m_pGeom, &envelope);
				envelope.merge(pPoint->getX()-fMargin,pPoint->getY()-fMargin);
				envelope.merge(pPoint->getX()+fMargin,pPoint->getY()+fMargin);
				envelope_to_rect(frt,envelope);

				m_pRenderDevice->Refresh(m_pOperMap,frt);
			}
			SMT_SAFE_DELETE(m_pGeom);
		}
	}

	void  SmtAppendFeatureTool::AppendDotFeature()
	{
		if (m_pOperMap)
		{
			SmtSysManager * pSysMgr = SmtSysManager::get_singleton_ptr();
			SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

			OGRFeature* ogr = make_ogr_feature(m_pOperMap, m_pGeom,
							   SmtFeatureType::SmtFtDot,
							   styleSonfig.szPointStyle);

			if (commit_append(m_edits.get(), ogr))
			{
				fRect frt;
				Envelope envelope;
				OGRPoint * pPoint = (OGRPoint*)m_pGeom;
				float fMargin = 5./m_pRenderDevice->GetBlc();
				
				geo::copy_envelope(*m_pGeom, &envelope);
				envelope.merge(pPoint->getX()-fMargin,pPoint->getY()-fMargin);
				envelope.merge(pPoint->getX()+fMargin,pPoint->getY()+fMargin);
				envelope_to_rect(frt,envelope);

				m_pRenderDevice->Refresh(m_pOperMap,frt);
			}
			SMT_SAFE_DELETE(m_pGeom);
		}
	}

	void SmtAppendFeatureTool::AppendLineFeature(void)
	{	
		if(m_pOperMap)
		{
			SmtSysManager * pSysMgr = SmtSysManager::get_singleton_ptr();
			SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

			OGRFeature* ogr = make_ogr_feature(m_pOperMap, m_pGeom, SmtFtCurve,
							   styleSonfig.szLineStyle);
			if (auto* curve = dynamic_cast<OGRCurve*>(m_pGeom)) {
				set_field_double(ogr, "length", curve->get_Length());
			}

			if (commit_append(m_edits.get(), ogr))
			{
				fRect frt;
				Envelope envelope ;
				float fMargin = 5./m_pRenderDevice->GetBlc();

				geo::copy_envelope(*m_pGeom, &envelope);
				envelope.merge(envelope.MinX-fMargin,envelope.MinY-fMargin);
				envelope.merge(envelope.MaxX+fMargin,envelope.MaxY+fMargin);
				envelope_to_rect(frt,envelope);

				m_pRenderDevice->Refresh(m_pOperMap,frt);
			}
			SMT_SAFE_DELETE(m_pGeom);
		}
	}

	void SmtAppendFeatureTool::AppendRegionFeature()
	{
		if (m_pOperMap)
		{	
			SmtSysManager * pSysMgr = SmtSysManager::get_singleton_ptr();
			SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();

			OGRFeature* ogr = make_ogr_feature(m_pOperMap, m_pGeom, SmtFtSurface,
							   styleSonfig.szRegionStyle);
			if (auto* surf = dynamic_cast<OGRSurface*>(m_pGeom)) {
				set_field_double(ogr, "area", surf->get_Area());
			}

			if (commit_append(m_edits.get(), ogr))
			{
				fRect frt;
				Envelope envelope ;
				float fMargin = 5./m_pRenderDevice->GetBlc();

				geo::copy_envelope(*m_pGeom, &envelope);
				envelope.merge(envelope.MinX-fMargin,envelope.MinY-fMargin);
				envelope.merge(envelope.MaxX+fMargin,envelope.MaxY+fMargin);
				envelope_to_rect(frt,envelope);

				m_pRenderDevice->Refresh(m_pOperMap,frt);
			}
			SMT_SAFE_DELETE(m_pGeom);
		}
	}

	int SmtAppendFeatureTool::KeyDown(uint nChar, uint nRepCnt, uint nFlags)
	{
		{
			//if (GetKeyState(VK_CONTROL) & 0x80000000)
			{
				switch (nChar)
				{
				case 't':
				case 'T':
					{
						if (m_edits && m_edits->can_undo())
							m_edits->undo();
					}
					break;
				case 'y':
				case 'Y':
					{
						if (m_edits && m_edits->can_redo())
							m_edits->redo();
					}
					break;
				}
			}
		}

		return SmtBaseTool::KeyDown(nChar,nRepCnt,nFlags);
	}
}