// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/map_to_scene.h"

#include "legacy/render/model3d/2dgeoobject.h"
#include "sdb/carto/style.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"

#include "gdal.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cmath>
#include <string>

using namespace base;

namespace render {

void apply_view3d_viewport(Viewport3D* vp, ulong width, ulong height) {
  if (!vp) {
    return;
  }
  vp->ulX = 0;
  vp->ulY = 0;
  vp->ulWidth = width;
  vp->ulHeight = height;
  vp->fFovy = 45.f;
  vp->fZNear = 0.1f;
  vp->fZFar = 1000.f;
}

void frame_persp_camera_to_aabb(SmtPerspCamera* camera,
                                Viewport3D* vp,
                                const Aabb& aabb) {
  if (!camera) {
    return;
  }
  Vector3 target(0.f, 0.f, 0.f);
  float span = 40.f;
  if (aabb.is_init()) {
    target = Vector3(aabb.vcCenter.x, aabb.vcCenter.y, aabb.vcCenter.z);
    const float dx = aabb.vcMax.x - aabb.vcMin.x;
    const float dy = aabb.vcMax.y - aabb.vcMin.y;
    const float dz = aabb.vcMax.z - aabb.vcMin.z;
    span = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (span < 1.f) {
      span = 40.f;
    }
  }
  Vector3 eye = target + Vector3(0.f, span * 0.75f, span * 0.85f);
  Vector3 up(0.f, 1.f, 0.f);
  camera->SetETU(eye, target, up);
  camera->SetMoveStep(span / 100.f);
  if (vp) {
    if (vp->fZNear <= 0.f) {
      vp->fZNear = 0.1f;
    }
    const float need_far = span * 4.f + 10.f;
    if (vp->fZFar < need_far) {
      vp->fZFar = need_far;
    }
    camera->SetViewport(*vp);
  }
}

int seed_ogr_layer_into_scene(LP3DRENDERDEVICE device,
                              SmtScene* scene,
                              OGRLayer* layer) {
  if (!device || !scene || !layer) {
    return 0;
  }
  layer->ResetReading();
  int added = 0;
  while (OGRFeature* feat = layer->GetNextFeature()) {
    OGRGeometry* geom = feat->GetGeometryRef();
    if (!geom) {
      OGRFeature::DestroyFeature(feat);
      continue;
    }
    SmtStyle style;
    sdb::datasource::fill_default_draw_style(feat, &style, 1.f);
    Smt2DGeoObject* obj = new Smt2DGeoObject();
    Vector3 pos(0.f, 0.f, 0.f);
    SmtMaterial mat;
    const OGRwkbGeometryType gt = wkbFlatten(geom->getGeometryType());
    const COLORREF brush = style.get_brush_desc().lBrushColor;
    const COLORREF pen = style.get_pen_desc().lPenColor;
    const float br = GetRValue(brush) / 255.f;
    const float bg = GetGValue(brush) / 255.f;
    const float bb = GetBValue(brush) / 255.f;
    const float pr = GetRValue(pen) / 255.f;
    const float pg = GetGValue(pen) / 255.f;
    const float pb = GetBValue(pen) / 255.f;
    if (gt == wkbLineString || gt == wkbMultiLineString) {
      mat.SetAmbientValue(SmtColor(pr * 0.5f, pg * 0.5f, pb * 0.5f, 1.f));
      mat.SetDiffuseValue(SmtColor(pr, pg, pb, 1.f));
      mat.SetEmissiveValue(SmtColor(pr * 0.35f, pg * 0.35f, pb * 0.35f, 1.f));
    } else if (gt == wkbPoint || gt == wkbMultiPoint) {
      const int ai = feat->GetFieldIndex("anno");
      const char* anno = ai >= 0 ? feat->GetFieldAsString(ai) : nullptr;
      if (anno && anno[0]) {
        mat.SetAmbientValue(SmtColor(0.55f, 0.50f, 0.10f, 1.f));
        mat.SetDiffuseValue(SmtColor(1.00f, 0.92f, 0.20f, 1.f));
        mat.SetEmissiveValue(SmtColor(0.45f, 0.40f, 0.05f, 1.f));
      } else {
        mat.SetAmbientValue(SmtColor(0.55f, 0.15f, 0.10f, 1.f));
        mat.SetDiffuseValue(SmtColor(0.95f, 0.25f, 0.15f, 1.f));
        mat.SetEmissiveValue(SmtColor(0.40f, 0.08f, 0.05f, 1.f));
      }
    } else {
      mat.SetAmbientValue(SmtColor(br * 0.45f, bg * 0.45f, bb * 0.45f, 1.f));
      mat.SetDiffuseValue(SmtColor(br, bg, bb, 1.f));
      mat.SetEmissiveValue(SmtColor(br * 0.20f, bg * 0.20f, bb * 0.20f, 1.f));
    }
    obj->Init(pos, mat);
    obj->SetGeometry(geom);
    obj->SetStyle(&style);
    if (obj->Create(device) == SMT_ERR_NONE) {
      obj->SetVisible(true);
      scene->Add3DObject(obj);
      ++added;
    } else {
      delete obj;
    }
    OGRFeature::DestroyFeature(feat);
  }
  return added;
}

int seed_geojson_into_scene(LP3DRENDERDEVICE device,
                            SmtScene* scene,
                            const char* path) {
  if (!device || !scene || !path || !path[0]) {
    return 0;
  }
  GDALAllRegister();
  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpenEx(
      path, GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr));
  if (!ds || ds->GetLayerCount() < 1) {
    if (ds) {
      GDALClose(ds);
    }
    return 0;
  }
  int added = 0;
  for (int i = 0; i < ds->GetLayerCount(); ++i) {
    added += seed_ogr_layer_into_scene(device, scene, ds->GetLayer(i));
  }
  GDALClose(ds);
  return added;
}

int seed_sample_map_into_scene(LP3DRENDERDEVICE device, SmtScene* scene) {
  if (!device || !scene) {
    return 0;
  }
  char module[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, module, MAX_PATH);
  std::string dir;
  if (n > 0 && n < MAX_PATH) {
    dir.assign(module, module + n);
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) {
      dir.resize(slash + 1);
    }
  }
  const char* rel[] = {
      "china_city.gpkg",
      "china_city.geojson",
      "china_plp.geojson",
      "testing\\data\\china_city.gpkg",
      "testing\\data\\china_city.geojson",
      "testing\\data\\china_plp.geojson",
      "..\\testing\\data\\china_city.gpkg",
      "..\\testing\\data\\china_city.geojson",
      "..\\testing\\data\\china_plp.geojson",
      "..\\..\\testing\\data\\china_city.gpkg",
      "..\\..\\testing\\data\\china_plp.geojson",
  };
  for (const char* r : rel) {
    const std::string cand = dir + r;
    const DWORD attr = GetFileAttributesA(cand.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      const int added = seed_geojson_into_scene(device, scene, cand.c_str());
      if (added > 0) {
        return added;
      }
    }
  }
  return 0;
}

}  // namespace render
