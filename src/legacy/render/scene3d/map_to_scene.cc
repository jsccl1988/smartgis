// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/map_to_scene.h"

#include "gdal.h"
#include "gdal_priv.h"
#include "legacy/render/model3d/2dgeoobject.h"
#include "legacy/render/scene3d/dem_to_world.h"
#include "legacy/render/scene3d/map_label_batch.h"
#include "legacy/render/scene3d/scene_to_world.h"
#include "legacy/render/scene3d/stereo_terrain.h"
#include "ogrsf_frmts.h"
#include "base/carto/style.h"
#include "gis/datasource/gdal/ogr_feature_codec.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>


using namespace base;

namespace render {
namespace {

// Framing cache for leftover_dem_aabb / leftover_has_scene_dem. Updated on each
// successful seed; does NOT block re-seed of another SmtScene (no sticky skip).
struct DemFrameCache {
  bool valid = false;
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  float min_m = 0;
  float max_m = 1;
  float vert_exag = 0.0012f;
};

DemFrameCache g_last_dem_frame;
// Non-owning: points at the DemHeightField adopted by the last StereoTerrain
// (scene-owned). Used for drape/labels during the same seed_* call.
DemHeightField* g_active_dem = nullptr;
MapLabelBatch* g_pending_labels = nullptr;
gis::World g_map_world;

void remember_dem_frame(const DemHeightField& dem) {
  if (dem.empty()) {
    g_last_dem_frame.valid = false;
    return;
  }
  dem.envelope(&g_last_dem_frame.minx, &g_last_dem_frame.miny,
               &g_last_dem_frame.maxx, &g_last_dem_frame.maxy);
  g_last_dem_frame.min_m = dem.min_meters();
  g_last_dem_frame.max_m = dem.max_meters();
  g_last_dem_frame.vert_exag = dem.vertical_exaggeration();
  g_last_dem_frame.valid = true;
}

void clear_map_world() {
  while (g_map_world.node_count() > 0) {
    const gis::Node* n = g_map_world.node_at(0);
    if (!n || !g_map_world.remove_node(n->id)) {
      break;
    }
  }
}

void seed_dem_into_map_world(const DemHeightField& dem) {
  clear_map_world();
  set_smt_scene_world_mirror(&g_map_world);
  if (!dem.empty()) {
    seed_dem_height_field_into_world(&g_map_world, dem, "stereo_dem");
  }
}

float sample_draped_height(double x, double y, void* user) {
  auto* dem = static_cast<DemHeightField*>(user);
  if (!dem || dem->empty()) {
    return 0.f;
  }
  return dem->sample(x, y) + dem->drape_lift();
}

const char* field_or_empty(OGRFeature* feat, const char* key) {
  if (!feat || !key) {
    return "";
  }
  const int i = feat->GetFieldIndex(key);
  if (i < 0) {
    return "";
  }
  const char* v = feat->GetFieldAsString(i);
  return v ? v : "";
}

std::string feature_label_text(OGRFeature* feat) {
  const char* anno = field_or_empty(feat, "anno");
  if (anno && anno[0]) {
    return anno;
  }
  const char* name = field_or_empty(feat, "name");
  if (name && name[0]) {
    return name;
  }
  return {};
}

void add_feature_label(MapLabelBatch* labels, OGRFeature* feat,
                       OGRGeometry* geom, DemHeightField* dem) {
  if (!labels || !feat || !geom) {
    return;
  }
  const std::string text = feature_label_text(feat);
  if (text.empty()) {
    return;
  }
  const char* kind = field_or_empty(feat, "kind");
  const char* cls = field_or_empty(feat, "class");
  const char* adcode = field_or_empty(feat, "adcode");
  const int pri = label_priority_from_fields(text.c_str(), kind, cls, adcode);
  if (pri > 3) {
    return;
  }
  OGREnvelope env;
  geom->getEnvelope(&env);
  const double x = 0.5 * (env.MinX + env.MaxX);
  const double y = 0.5 * (env.MinY + env.MaxY);
  MapLabel lab;
  lab.text = text;
  lab.x = static_cast<float>(x);
  lab.z = static_cast<float>(y);
  lab.y = dem ? dem->sample(x, y) + dem->drape_lift() + 0.15f : 0.2f;
  lab.priority = pri;
  labels->add_label(lab);
}

void append_ogr_ring(OGRLinearRing* ring, std::vector<render::LonLatRing>* out) {
  if (!ring || !out || ring->getNumPoints() < 3) {
    return;
  }
  render::LonLatRing dst;
  const int n = ring->getNumPoints();
  dst.x.reserve(static_cast<size_t>(n));
  dst.y.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    dst.x.push_back(ring->getX(i));
    dst.y.push_back(ring->getY(i));
  }
  out->push_back(std::move(dst));
}

void collect_land_rings(GDALDataset* ds, std::vector<render::LonLatRing>* out) {
  if (!ds || !out) {
    return;
  }
  auto append_layer_polygons = [&](OGRLayer* layer) {
    if (!layer) {
      return;
    }
    layer->ResetReading();
    while (OGRFeature* feat = layer->GetNextFeature()) {
      OGRGeometry* geom = feat->GetGeometryRef();
      if (!geom) {
        OGRFeature::DestroyFeature(feat);
        continue;
      }
      const OGRwkbGeometryType gt = wkbFlatten(geom->getGeometryType());
      if (gt == wkbPolygon) {
        append_ogr_ring(static_cast<OGRPolygon*>(geom)->getExteriorRing(), out);
      } else if (gt == wkbMultiPolygon) {
        auto* mp = static_cast<OGRMultiPolygon*>(geom);
        for (int p = 0; p < mp->getNumGeometries(); ++p) {
          auto* poly = static_cast<OGRPolygon*>(mp->getGeometryRef(p));
          if (poly) {
            append_ogr_ring(poly->getExteriorRing(), out);
          }
        }
      }
      OGRFeature::DestroyFeature(feat);
    }
  };

  // Prefer the prefecture `area` layer so DEM outline matches the 2D map.
  OGRLayer* area = ds->GetLayerByName("area");
  if (area) {
    append_layer_polygons(area);
    if (!out->empty()) {
      return;
    }
  }
  for (int i = 0; i < ds->GetLayerCount(); ++i) {
    OGRLayer* layer = ds->GetLayer(i);
    if (!layer) {
      continue;
    }
    const char* lname = layer->GetName();
    if (lname) {
      const std::string name = lname;
      if (name == "line" || name == "point" || name == "text" ||
          name == "label" || name == "labels") {
        continue;
      }
    }
    append_layer_polygons(layer);
  }
}

// china_city area is MultiPolygon-heavy (~1000 rings). DEM mask only needs
// the largest landmasses; the rest is O(cells×rings) noise on the UI thread.
void trim_dem_mask_rings(std::vector<render::LonLatRing>* rings,
                         size_t max_keep) {
  if (!rings || rings->size() <= max_keep) {
    return;
  }
  struct Item {
    size_t i;
    double area;
  };
  std::vector<Item> items;
  items.reserve(rings->size());
  for (size_t i = 0; i < rings->size(); ++i) {
    (*rings)[i].prepare_bbox();
    const double area = ((*rings)[i].maxx - (*rings)[i].minx) *
                        ((*rings)[i].maxy - (*rings)[i].miny);
    items.push_back({i, area});
  }
  std::nth_element(items.begin(), items.begin() + static_cast<int>(max_keep),
                   items.end(),
                   [](const Item& a, const Item& b) { return a.area > b.area; });
  items.resize(max_keep);
  std::vector<render::LonLatRing> kept;
  kept.reserve(max_keep);
  for (const Item& it : items) {
    kept.push_back(std::move((*rings)[it.i]));
  }
  rings->swap(kept);
}

bool seed_stereo_underlay(LP3DRENDERDEVICE device, SmtScene* scene,
                          MapLabelBatch** out_labels,
                          const std::vector<render::LonLatRing>* rings) {
  // Device may be null in unit tests; still attach owned DEM to |scene|.
  if (!scene) {
    return false;
  }
  g_pending_labels = nullptr;
  // Heap field per seed; StereoTerrain adopts it so lifetime outlives Create.
  auto* dem = new DemHeightField();
  const std::string dem_path = find_sample_dem_path();
  const bool loaded_real =
      !dem_path.empty() && dem->load_gdal_raster(dem_path.c_str());
  if (!loaded_real) {
    dem->fill_synthetic_china();
  }
  // china_dem.tif from build_china_dem.py is already cutlined to the national
  // outline. Re-masking with prefecture rings is O(cells×rings) on the UI
  // thread and punches holes (trim keeps only 48 largest cities).
  const bool dem_already_cutlined =
      loaded_real && dem_path.find("china_dem") != std::string::npos;
  // Skip remask when rings miss the mainland (110E, 35N). trim_dem_mask_rings(48)
  // can keep only large western prefectures and punch Henan/the plains.
  if (!dem_already_cutlined && rings && !rings->empty() &&
      any_ring_contains(110.0, 35.0, *rings)) {
    dem->mask_outside_rings(*rings);
  }
  Vector3 pos(0.f, 0.f, 0.f);
  SmtMaterial mat;
  mat.SetAmbientValue(SmtColor(0.32f, 0.36f, 0.34f, 1.f));
  mat.SetDiffuseValue(SmtColor(0.78f, 0.80f, 0.70f, 1.f));
  mat.SetEmissiveValue(SmtColor(0.08f, 0.09f, 0.07f, 1.f));
  mat.SetSpecularValue(SmtColor(0.05f, 0.05f, 0.05f, 1.f));
  mat.SetShininessValue(4.f);
  auto* terrain = new StereoTerrain();
  terrain->adopt_height_field(dem);
  const std::string rs = find_sample_imagery_path();
  if (terrain->Init(pos, mat, rs.empty() ? "" : rs.c_str()) != SMT_ERR_NONE ||
      terrain->Create(device) != SMT_ERR_NONE) {
    delete terrain;
  } else {
    remember_dem_frame(*dem);
    g_active_dem = dem;
    seed_dem_into_map_world(*dem);
    terrain->SetVisible(true);
    scene->Add3DObject(terrain);
  }
  auto* labels = new MapLabelBatch();
  if (!device || labels->Init(pos, mat) != SMT_ERR_NONE ||
      labels->Create(device) != SMT_ERR_NONE) {
    delete labels;
    g_pending_labels = nullptr;
    if (out_labels) {
      *out_labels = nullptr;
    }
    return true;
  }
  labels->SetVisible(true);
  g_pending_labels = labels;
  if (out_labels) {
    *out_labels = labels;
  }
  return true;
}

}  // namespace

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
  // Geographic leftover DEM (lon 73–135) is ~150 units from the origin pose.
  // Keep a floor of 1000; frame_persp raises this from the DEM span.
  if (vp->fZFar < 1000.f) {
    vp->fZFar = 1000.f;
  }
}

void leftover_frame_pose(const Aabb& aabb, Vector3* eye, Vector3* target,
                         float* span) {
  Vector3 look(0.f, 0.f, 0.f);
  float s = 40.f;
  if (aabb.is_init()) {
    look = Vector3(aabb.vcCenter.x, aabb.vcCenter.y, aabb.vcCenter.z);
    const float dx = aabb.vcMax.x - aabb.vcMin.x;
    const float dy = aabb.vcMax.y - aabb.vcMin.y;
    const float dz = aabb.vcMax.z - aabb.vcMin.z;
    s = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (s < 1.f) {
      s = 40.f;
    }
  }
  if (target) {
    *target = look;
  }
  if (eye) {
    // Stand south of the look-at (+Y up, +Z north) so the view looks north:
    // geographic north sits toward the top of the screen, matching 2D maps.
    // The old +Z eye offset looked south and made DEM appear N/S flipped.
    *eye = look + Vector3(0.f, s * 0.75f, -s * 0.85f);
  }
  if (span) {
    *span = s;
  }
}

bool leftover_has_scene_dem() {
  // True when any seed has produced a DEM frame (last successful underlay).
  // Intentionally not a process-global "already seeded → skip next scene".
  return g_last_dem_frame.valid;
}

bool leftover_dem_aabb(Aabb* out) {
  if (!out || !g_last_dem_frame.valid) {
    return false;
  }
  const float ymin =
      g_last_dem_frame.min_m * g_last_dem_frame.vert_exag;
  const float ymax =
      g_last_dem_frame.max_m * g_last_dem_frame.vert_exag;
  // Leftover Y-up: lon→X, height→Y, lat→Z (north = +Z).
  out->vcMin.set(static_cast<float>(g_last_dem_frame.minx), ymin,
                 static_cast<float>(g_last_dem_frame.miny));
  out->vcMax.set(static_cast<float>(g_last_dem_frame.maxx), ymax,
                 static_cast<float>(g_last_dem_frame.maxy));
  out->vcCenter = (out->vcMax + out->vcMin) / 2.f;
  return out->is_init();
}

bool leftover_eye_misses_dem(const Vector3& eye) {
  Aabb dem;
  if (!leftover_dem_aabb(&dem) || !dem.is_init()) {
    return false;
  }
  const float cx = dem.vcCenter.x;
  const float cz = dem.vcCenter.z;
  const float dx = dem.vcMax.x - dem.vcMin.x;
  const float dz = dem.vcMax.z - dem.vcMin.z;
  const float span = std::sqrt(dx * dx + dz * dz);
  const float dist = std::sqrt((eye.x - cx) * (eye.x - cx) +
                               (eye.z - cz) * (eye.z - cz));
  return dist > span * 1.5f;
}

void frame_persp_camera_to_aabb(SmtPerspCamera* camera, Viewport3D* vp,
                                const Aabb& aabb) {
  if (!camera) {
    return;
  }
  Aabb use = aabb;
  Aabb dem;
  if (leftover_dem_aabb(&dem) && dem.is_init()) {
    // Prefer the DEM so an origin cube / empty scene AABB cannot pin the
    // leftover camera at (0,0,100) while China sits at lon/lat.
    use = dem;
  }
  Vector3 target;
  Vector3 eye;
  float span = 40.f;
  leftover_frame_pose(use, &eye, &target, &span);
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

int seed_ogr_layer_into_scene(LP3DRENDERDEVICE device, SmtScene* scene,
                              OGRLayer* layer) {
  if (!device || !scene || !layer) {
    return 0;
  }
  MapLabelBatch* labels = g_pending_labels;
  DemHeightField* dem =
      (g_active_dem && !g_active_dem->empty()) ? g_active_dem : nullptr;
  layer->ResetReading();
  int added = 0;
  while (OGRFeature* feat = layer->GetNextFeature()) {
    OGRGeometry* geom = feat->GetGeometryRef();
    if (!geom) {
      OGRFeature::DestroyFeature(feat);
      continue;
    }
    const OGRwkbGeometryType gt = wkbFlatten(geom->getGeometryType());
    add_feature_label(labels, feat, geom, dem);
    if (gt == wkbPoint || gt == wkbMultiPoint) {
      OGRFeature::DestroyFeature(feat);
      continue;
    }
    // Filled area polygons nearly coplanar with the DEM cause z-fighting
    // stripes; land shape already comes from the masked height field.
    if (gt == wkbPolygon || gt == wkbMultiPolygon) {
      OGRFeature::DestroyFeature(feat);
      continue;
    }
    SmtStyle style;
    gis::datasource::fill_default_draw_style(feat, &style, 1.f);
    Smt2DGeoObject* obj = new Smt2DGeoObject();
    Vector3 pos(0.f, 0.f, 0.f);
    SmtMaterial mat;
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
    } else {
      mat.SetAmbientValue(SmtColor(br * 0.38f, bg * 0.38f, bb * 0.38f, 1.f));
      mat.SetDiffuseValue(SmtColor(br * 0.92f, bg * 0.92f, bb * 0.92f, 1.f));
      mat.SetEmissiveValue(SmtColor(br * 0.12f, bg * 0.12f, bb * 0.12f, 1.f));
    }
    obj->Init(pos, mat);
    if (dem) {
      obj->SetHeightSampleFn(sample_draped_height, dem);
    }
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

int seed_geojson_into_scene(LP3DRENDERDEVICE device, SmtScene* scene,
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
  // Prefer pre-cutlined china_dem: skip collecting ~1000 prefecture rings.
  const std::string dem_path = find_sample_dem_path();
  const bool skip_mask =
      !dem_path.empty() && dem_path.find("china_dem") != std::string::npos;
  std::vector<render::LonLatRing> rings;
  if (!skip_mask) {
    collect_land_rings(ds, &rings);
    trim_dem_mask_rings(&rings, 48);
  }
  MapLabelBatch* labels = nullptr;
  seed_stereo_underlay(device, scene, &labels, skip_mask ? nullptr : &rings);
  int added = 0;
  for (int i = 0; i < ds->GetLayerCount(); ++i) {
    OGRLayer* layer = ds->GetLayer(i);
    if (!layer) {
      continue;
    }
    // area fills are skipped inside seed_ogr_layer; avoid walking them for
    // labels too (text/point layers carry place-names).
    const char* lname = layer->GetName();
    if (lname && std::strcmp(lname, "area") == 0) {
      continue;
    }
    added += seed_ogr_layer_into_scene(device, scene, layer);
  }
  if (labels) {
    scene->Add3DObject(labels);
    if (g_pending_labels == labels) {
      g_pending_labels = nullptr;
    }
  }
  // SP4: refresh World AABB mirror without requiring CreateOctTreeSceneMgr.
  seed_smt_scene_aabbs_into_world(&g_map_world, scene);
  GDALClose(ds);
  // Terrain counts as a seed even when the pack has no line features (area /
  // points are skipped). Otherwise view_3d adds an origin cube and the camera
  // stays at leftover (0,0,100) while China sits at lon/lat.
  if (g_active_dem && !g_active_dem->empty()) {
    ++added;
  }
  return added;
}

int seed_sample_map_into_scene(LP3DRENDERDEVICE device, SmtScene* scene) {
  // Null device allowed: DEM underlay still attaches owned StereoTerrain.
  if (!scene) {
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
      // Only treat THIS scene's seed success — do not short-circuit on a prior
      // view's leftover_has_scene_dem / framing cache.
      if (added > 0) {
        return added;
      }
    }
  }
  // No vector pack next to the exe: still seed China DEM so 3D is not a cube.
  if (seed_stereo_underlay(device, scene, nullptr, nullptr)) {
    return leftover_has_scene_dem() ? 1 : 0;
  }
  return 0;
}

gis::World* map_seeded_world() {
  return &g_map_world;
}

}  // namespace render
