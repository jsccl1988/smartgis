// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scene3d/scene/map_to_scene.h"

#include "gdal.h"
#include "gdal_priv.h"
#include "gis/datasource/ogr/ogr_feature_codec.h"
#include "vista/terrain/dem/dem_frame.h"
#include "scenic/scene3d/primitive/feature/feature_mesh.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_ogr.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_pod.h"
#include "scenic/scene3d/primitive/feature/map_label_batch.h"
#include "scenic/scene3d/scene/scene_to_world.h"
#include "vista/world/dem_seed.h"
#include "scenic/scene3d/primitive/surface/terrain.h"
#include "scenic/scene3d/primitive/surface/pointcloud.h"
#include "scenic/scene3d/primitive/feature/geo_object.h"
#include "scenic/scene3d/primitive/mesh/cube.h"
#include "scenic/scene3d/primitive/mesh/sphere.h"
#include "scenic/scene3d/primitive/mesh/water.h"
#include "ogrsf_frmts.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include "base/process/switches.h"

using namespace base;

namespace scenic {
namespace detail {
namespace {

using render::DemHeightField;
using render::find_sample_dem_path;
using render::find_sample_imagery_path;
using render::label_priority_from_fields;

// Framing cache for leftover_dem_aabb / leftover_has_scene_dem. Updated on each
// successful seed; does NOT block re-seed of another Scene (no sticky skip).
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
// Non-owning: points at the DemHeightField adopted by the last Terrain
// (scene-owned). Used for drape/labels during the same seed_* call.
DemHeightField* g_active_dem = nullptr;
MapLabelBatch* g_pending_labels = nullptr;
vista::World g_map_world;

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
    const vista::Node* n = g_map_world.node_at(0);
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

bool utf8_has_cjk(const char* text) {
  if (!text || !text[0]) {
    return false;
  }
  const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
  while (*p) {
    if (*p < 0x80) {
      ++p;
      continue;
    }
    uint32_t cp = 0;
    if ((*p & 0xe0) == 0xc0 && p[1]) {
      cp = (static_cast<uint32_t>(p[0] & 0x1f) << 6) |
           static_cast<uint32_t>(p[1] & 0x3f);
      p += 2;
    } else if ((*p & 0xf0) == 0xe0 && p[1] && p[2]) {
      cp = (static_cast<uint32_t>(p[0] & 0x0f) << 12) |
           (static_cast<uint32_t>(p[1] & 0x3f) << 6) |
           static_cast<uint32_t>(p[2] & 0x3f);
      p += 3;
    } else if ((*p & 0xf8) == 0xf0 && p[1] && p[2] && p[3]) {
      cp = (static_cast<uint32_t>(p[0] & 0x07) << 18) |
           (static_cast<uint32_t>(p[1] & 0x3f) << 12) |
           (static_cast<uint32_t>(p[2] & 0x3f) << 6) |
           static_cast<uint32_t>(p[3] & 0x3f);
      p += 4;
    } else {
      ++p;
      continue;
    }
    // CJK Unified + extensions / Hangul / fullwidth �?china place names.
    if ((cp >= 0x2e80 && cp <= 0x9fff) || (cp >= 0xac00 && cp <= 0xd7af) ||
        (cp >= 0xf900 && cp <= 0xfaff) || (cp >= 0xff01 && cp <= 0xff60) ||
        (cp >= 0x20000 && cp <= 0x2fa1f)) {
      return true;
    }
  }
  return false;
}

std::string feature_label_text(OGRFeature* feat) {
  const char* anno = field_or_empty(feat, "anno");
  if (anno && anno[0] && utf8_has_cjk(anno)) {
    return anno;
  }
  const char* name = field_or_empty(feat, "name");
  if (name && name[0] && utf8_has_cjk(name)) {
    return name;
  }
  // Prefer CJK; fall back to Latin only when no Chinese field exists.
  if (anno && anno[0]) {
    return anno;
  }
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
  // Country DEM view: place-name labels from points only. Line midpoints pull
  // in Latin hydro names (Oka / Ravi / Son / Betwa) from china_city and steal
  // the declutter budget from Chinese cities.
  const OGRwkbGeometryType gt = wkbFlatten(geom->getGeometryType());
  if (gt != wkbPoint && gt != wkbMultiPoint) {
    return;
  }
  if (!utf8_has_cjk(text.c_str())) {
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
  lab.x = vista::dem_lon_to_x(x);
  lab.z = static_cast<float>(y);
  lab.y = dem ? dem->sample(x, y) + dem->drape_lift() + 0.15f : 0.2f;
  lab.priority = pri;
  labels->add_label(lab);
}

void append_ogr_ring(OGRLinearRing* ring,
                     std::vector<render::LonLatRing>* out) {
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
// the largest landmasses; the rest is O(cells脳rings) noise on the UI thread.
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
  std::nth_element(
      items.begin(), items.begin() + static_cast<int>(max_keep), items.end(),
      [](const Item& a, const Item& b) { return a.area > b.area; });
  items.resize(max_keep);
  std::vector<render::LonLatRing> kept;
  kept.reserve(max_keep);
  for (const Item& it : items) {
    kept.push_back(std::move((*rings)[it.i]));
  }
  rings->swap(kept);
}

bool seed_stereo_underlay(LP3DRENDERDEVICE device, Scene* scene,
                          MapLabelBatch** out_labels,
                          const std::vector<render::LonLatRing>* rings) {
  // Device may be null in unit tests; still attach owned DEM to |scene|.
  if (!scene) {
    return false;
  }
  g_pending_labels = nullptr;
  // Heap field per seed; Terrain adopts it so lifetime outlives Create.
  auto* dem = new DemHeightField();
  const std::string dem_path = find_sample_dem_path();
  const bool loaded_real =
      !dem_path.empty() && dem->load_gdal_raster(dem_path.c_str()) &&
      !dem->empty();
  if (!loaded_real) {
    // Real-data policy: no synthetic China DEM stand-in.
    delete dem;
    return false;
  }
  // china_dem.tif from build_china_dem.py is already cutlined to the national
  // outline. Re-masking with prefecture rings is O(cells×rings) on the UI
  // thread and punches holes (trim keeps only 48 largest cities).
  const bool dem_already_cutlined =
      dem_path.find("china_dem") != std::string::npos;
  // Skip remask when rings miss the mainland (110E, 35N).
  // trim_dem_mask_rings(48) can keep only large western prefectures and punch
  // Henan/the plains.
  if (!dem_already_cutlined && rings && !rings->empty() &&
      any_ring_contains(110.0, 35.0, *rings)) {
    dem->mask_outside_rings(*rings);
  }
  Vector3 pos(0.f, 0.f, 0.f);
  Material mat;
  // Vertex RGB carries the atlas; material only scales lit response.
  // Keep diffuse below 1.0 �?D3D * snow albedo otherwise blows west peaks.
  mat.SetAmbientValue(Color(0.40f, 0.42f, 0.44f, 1.f));
  mat.SetDiffuseValue(Color(0.82f, 0.82f, 0.80f, 1.f));
  mat.SetEmissiveValue(Color(0.03f, 0.035f, 0.03f, 1.f));
  mat.SetSpecularValue(Color(0.06f, 0.06f, 0.05f, 1.f));
  mat.SetShininessValue(10.f);
  auto* terrain = new Terrain();
  terrain->adopt_height_field(dem);
  const std::string rs = find_sample_imagery_path();
  if (terrain->Init(pos, mat, rs.empty() ? "" : rs.c_str()) != kErrNone ||
      terrain->Create(device) != kErrNone) {
    delete terrain;
  } else {
    remember_dem_frame(*dem);
    g_active_dem = dem;
    seed_dem_into_map_world(*dem);
    terrain->SetVisible(true);
    scene->Add3DObject(terrain);
  }
  auto* labels = new MapLabelBatch();
  if (!device || labels->Init(pos, mat) != kErrNone ||
      labels->Create(device) != kErrNone) {
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
  // Geographic leftover DEM (lon 73�?35) is ~150 units from the origin pose.
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
    // Offset length is ~1.56*span. A 45�?frustum needs >= ~1.31*span to hold
    // the AABB sphere; the old 1.13*span pose clipped China to a patch and
    // sliced the near edge into vertical streaks.
    *eye = look + Vector3(0.f, s * 1.00f, -s * 1.20f);
  }
  if (span) {
    *span = s;
  }
}

bool leftover_has_scene_dem() {
  // True when any seed has produced a DEM frame (last successful underlay).
  // Intentionally not a process-global "already seeded �?skip next scene".
  return g_last_dem_frame.valid;
}

bool leftover_dem_aabb(Aabb* out) {
  if (!out || !g_last_dem_frame.valid) {
    return false;
  }
  const float ymin = g_last_dem_frame.min_m * g_last_dem_frame.vert_exag;
  const float ymax = g_last_dem_frame.max_m * g_last_dem_frame.vert_exag;
  // Leftover Y-up: X=-lon, height鈫扽, lat鈫抁 (north = +Z). min/max swap under
  // negation so AABB stays axis-aligned.
  out->vcMin.set(vista::dem_lon_to_x(g_last_dem_frame.maxx), ymin,
                 static_cast<float>(g_last_dem_frame.miny));
  out->vcMax.set(vista::dem_lon_to_x(g_last_dem_frame.minx), ymax,
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
  const float dist =
      std::sqrt((eye.x - cx) * (eye.x - cx) + (eye.z - cz) * (eye.z - cz));
  return dist > span * 1.5f;
}

void frame_persp_camera_to_aabb(PerspCamera* camera, Viewport3D* vp,
                                const ::base::Aabb& aabb) {
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
  camera->set_etu(eye, target, up);
  camera->set_move_step(span / 100.f);
  if (vp) {
    if (vp->fZNear <= 0.f) {
      vp->fZNear = 0.1f;
    }
    const float need_far = span * 4.f + 10.f;
    if (vp->fZFar < need_far) {
      vp->fZFar = need_far;
    }
    camera->set_viewport(*vp);
  }
}

int seed_ogr_layer_into_scene(LP3DRENDERDEVICE device, Scene* scene,
                              OGRLayer* layer) {
  if (!device || !scene || !layer) {
    return 0;
  }
  MapLabelBatch* labels = g_pending_labels;
  DemHeightField* dem =
      (g_active_dem && !g_active_dem->empty()) ? g_active_dem : nullptr;
  layer->ResetReading();
  int added = 0;
  // china_city line layer is ~1.7k features; per-feature D3D DrawPrimitives
  // dominated present time (~100ms). Opt-in with SMT_SCENE3D_SEED_LINES=1.
  const bool seed_lines = []() {
    const char* e = base::switch_cstr("scene3d-seed-lines");
    return e && e[0] && e[0] != '0' && e[0] != 'n' && e[0] != 'N';
  }();
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
    if ((gt == wkbLineString || gt == wkbMultiLineString) && !seed_lines) {
      OGRFeature::DestroyFeature(feat);
      continue;
    }
    Style style;
    gis::datasource::fill_default_draw_style(feat, &style, 1.f);
    GeoObject* obj = new GeoObject();
    Vector3 pos(0.f, 0.f, 0.f);
    Material mat;
    const COLORREF brush = style.get_brush_desc().lBrushColor;
    const COLORREF pen = style.get_pen_desc().lPenColor;
    const float br = GetRValue(brush) / 255.f;
    const float bg = GetGValue(brush) / 255.f;
    const float bb = GetBValue(brush) / 255.f;
    const float pr = GetRValue(pen) / 255.f;
    const float pg = GetGValue(pen) / 255.f;
    const float pb = GetBValue(pen) / 255.f;
    if (gt == wkbLineString || gt == wkbMultiLineString) {
      mat.SetAmbientValue(Color(pr * 0.5f, pg * 0.5f, pb * 0.5f, 1.f));
      mat.SetDiffuseValue(Color(pr, pg, pb, 1.f));
      mat.SetEmissiveValue(Color(pr * 0.35f, pg * 0.35f, pb * 0.35f, 1.f));
    } else {
      mat.SetAmbientValue(Color(br * 0.38f, bg * 0.38f, bb * 0.38f, 1.f));
      mat.SetDiffuseValue(Color(br * 0.92f, bg * 0.92f, bb * 0.92f, 1.f));
      mat.SetEmissiveValue(Color(br * 0.12f, bg * 0.12f, bb * 0.12f, 1.f));
    }
    obj->Init(pos, mat);
    if (dem) {
      obj->set_height_sample(sample_draped_height, dem);
    }
    obj->set_geometry(geom);
    obj->set_style(&style);
    if (obj->Create(device) == kErrNone) {
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

int seed_geojson_into_scene(LP3DRENDERDEVICE device, Scene* scene,
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
  const std::string dem_path = render::find_sample_dem_path();
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

int seed_sample_map_into_scene(LP3DRENDERDEVICE device, Scene* scene) {
  // Null device allowed: DEM underlay still attaches owned Terrain.
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
      "..\\data\\china_city.gpkg",
      "..\\data\\china_city.geojson",
      "..\\data\\china_plp.geojson",
      "data\\china_city.gpkg",
      "data\\china_city.geojson",
      "data\\china_plp.geojson",
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
      // Only treat THIS scene's seed success �?do not short-circuit on a prior
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

void clear_leftover_dem_frame() {
  g_last_dem_frame = DemFrameCache{};
  g_active_dem = nullptr;
  g_pending_labels = nullptr;
}

const char* showcase_mode_from_env() {
  if (const char* mode = base::switch_cstr("scene3d-showcase-mode")) {
    if (mode[0]) {
      return mode;
    }
  }
  return "china";
}

namespace {

bool mode_is(const char* mode, const char* want) {
  return mode && want && _stricmp(mode, want) == 0;
}

int seed_terrain_only(LP3DRENDERDEVICE device, Scene* scene) {
  if (!seed_stereo_underlay(device, scene, nullptr, nullptr)) {
    return 0;
  }
  return leftover_has_scene_dem() ? 1 : 0;
}

int seed_cube_object(LP3DRENDERDEVICE device, Scene* scene) {
  clear_leftover_dem_frame();
  Vector3 center(0.f, 0.f, 0.f);
  auto* cube = new Cube(center, 24.f);
  Material mat;
  if (cube->Init(center, mat) != kErrNone ||
      cube->Create(device) != kErrNone) {
    delete cube;
    return 0;
  }
  cube->SetVisible(true);
  scene->Add3DObject(cube);
  return 1;
}

int seed_sphere_object(LP3DRENDERDEVICE device, Scene* scene) {
  clear_leftover_dem_frame();
  Vector3 pos(0.f, 0.f, 0.f);
  auto* sphere = new Sphere(12.f, 24);
  Material mat;
  mat.SetAmbientValue(Color(0.15f, 0.25f, 0.45f, 1.f));
  mat.SetDiffuseValue(Color(0.35f, 0.55f, 0.95f, 1.f));
  mat.SetSpecularValue(Color(0.8f, 0.8f, 0.9f, 1.f));
  mat.SetEmissiveValue(Color(0.05f, 0.08f, 0.12f, 1.f));
  mat.SetShininessValue(28.f);
  if (sphere->Init(pos, mat) != kErrNone ||
      sphere->Create(device) != kErrNone) {
    delete sphere;
    return 0;
  }
  sphere->SetVisible(true);
  scene->Add3DObject(sphere);
  return 1;
}

int seed_water_object(LP3DRENDERDEVICE device, Scene* scene) {
  clear_leftover_dem_frame();
  Vector3 pos(0.f, 0.f, 0.f);
  auto* water = new Water();
  // Wider / taller waves so the silhouette reads as water under default orbit.
  water->set_x_scale(0.55f);
  water->set_y_scale(0.85f);
  water->set_z_scale(0.55f);
  Material mat;
  mat.SetAmbientValue(Color(0.08f, 0.22f, 0.32f, 1.f));
  mat.SetDiffuseValue(Color(0.18f, 0.55f, 0.78f, 1.f));
  mat.SetSpecularValue(Color(0.75f, 0.85f, 0.95f, 1.f));
  mat.SetEmissiveValue(Color(0.03f, 0.08f, 0.12f, 1.f));
  mat.SetShininessValue(64.f);
  if (water->Init(pos, mat) != kErrNone ||
      water->Create(device) != kErrNone) {
    delete water;
    return 0;
  }
  // Advance the ripple sim a few ticks so the first BMP is not the seed frame.
  if (device) {
    for (int i = 0; i < 8; ++i) {
      water->Update(device, 0.016f);
    }
  }
  water->SetVisible(true);
  scene->Add3DObject(water);
  return 1;
}

bool write_synthetic_pointcloud_csv(const char* path) {
  if (!path || !path[0]) {
    return false;
  }
  std::ofstream out(path, std::ios::out | std::ios::trunc);
  if (!out) {
    return false;
  }
  // Read3DPointCloud: x,z,y,r,g,b then *10 �?denser grid so BMP non_black
  // clears the mesh gate (~0.12) under default orbit.
  int n = 0;
  for (int iz = -12; iz <= 12; ++iz) {
    for (int ix = -12; ix <= 12; ++ix) {
      const float x = static_cast<float>(ix) * 0.28f;
      const float z = static_cast<float>(iz) * 0.28f;
      const float y = 0.25f * std::sin(x * 1.3f) * std::cos(z * 1.2f);
      const int r = 50 + (ix + 12) * 7;
      const int g = 110 + (iz + 12) * 5;
      const int b = 60 + ((ix + iz + 24) % 11) * 12;
      out << x << ',' << z << ',' << y << ',' << r << ',' << g << ',' << b
          << '\n';
      ++n;
    }
  }
  return n >= 100;
}

int seed_pointcloud_object(LP3DRENDERDEVICE device, Scene* scene) {
  clear_leftover_dem_frame();
  char tmp[MAX_PATH] = {};
  const DWORD n = GetTempPathA(MAX_PATH, tmp);
  if (n == 0 || n >= MAX_PATH) {
    return 0;
  }
  char csv[MAX_PATH] = {};
  sprintf_s(csv, "%ssmt_scene3d_showcase_pc.csv", tmp);
  if (!write_synthetic_pointcloud_csv(csv)) {
    return 0;
  }
  Vector3 pos(0.f, 0.f, 0.f);
  Material mat;
  mat.SetDiffuseValue(Color(0.8f, 0.85f, 0.7f, 1.f));
  auto* cloud = new PointCloud3d();
  if (!cloud->read_point_cloud(csv) || cloud->Init(pos, mat) != kErrNone ||
      cloud->Create(device) != kErrNone) {
    delete cloud;
    DeleteFileA(csv);
    return 0;
  }
  cloud->set_show_bounds(false);
  cloud->SetVisible(true);
  scene->Add3DObject(cloud);
  DeleteFileA(csv);
  return 1;
}

int seed_northarray_framing(Scene* scene) {
  // Compass HUD is created in Scene::Setup; give orbit a local AABB.
  clear_leftover_dem_frame();
  Aabb aabb;
  aabb.vcMin.set(-20.f, -8.f, -20.f);
  aabb.vcMax.set(20.f, 8.f, 20.f);
  aabb.vcCenter = (aabb.vcMax + aabb.vcMin) * 0.5f;
  scene->SetAabb(aabb);
  return 1;
}

}  // namespace

int seed_showcase_mode_into_scene(LP3DRENDERDEVICE device, Scene* scene,
                                  const char* mode) {
  if (!scene) {
    return 0;
  }
  const char* m = (mode && mode[0]) ? mode : "china";
  if (mode_is(m, "china")) {
    return seed_sample_map_into_scene(device, scene);
  }
  if (mode_is(m, "terrain")) {
    return seed_terrain_only(device, scene);
  }
  if (!device) {
    return 0;
  }
  if (mode_is(m, "cube")) {
    return seed_cube_object(device, scene);
  }
  if (mode_is(m, "sphere")) {
    return seed_sphere_object(device, scene);
  }
  if (mode_is(m, "water")) {
    return seed_water_object(device, scene);
  }
  if (mode_is(m, "pointcloud")) {
    return seed_pointcloud_object(device, scene);
  }
  if (mode_is(m, "northarray")) {
    return seed_northarray_framing(scene);
  }
  std::fprintf(stderr, "seed_showcase_mode: unknown mode=%s (use china)\n", m);
  return seed_sample_map_into_scene(device, scene);
}

vista::World* map_seeded_world() { return &g_map_world; }

}  // namespace detail
}  // namespace scenic
