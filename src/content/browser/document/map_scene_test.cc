// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "base/trace/event/process_trace.h"
#include "gis/carto/style/paint_resolve.h"
#include "gis/carto/style/style_document.h"
#include "gis/carto/style/style_rules.h"
#include "vista/frame/detail/carto_filter.h"
#include "vista/frame/frame.h"
#include "vista/world/terrain/process/land_mask.h"
#include "tool/draft/draft.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

uint32_t layer_paint_color(const gis::style::StyleDocument& doc,
                           const char* layer_id, const char* key) {
  for (const gis::style::StyleLayer& layer : doc.layers) {
    if (layer.id != layer_id) {
      continue;
    }
    const auto it = layer.paint.find(key);
    if (it == layer.paint.end()) {
      return 0;
    }
    uint32_t argb = 0;
    if (!gis::style::parse_color(it->second, &argb)) {
      return 0;
    }
    return argb;
  }
  return 0;
}

size_t find_rel(const std::vector<std::string>& rels, const char* needle) {
  for (size_t i = 0; i < rels.size(); ++i) {
    if (rels[i].find(needle) != std::string::npos) {
      return i;
    }
  }
  return static_cast<size_t>(-1);
}

// Two distant mainland-sized parts: keeping only the largest would leave a hole.
bool write_multipart_geojson(const char* path) {
  FILE* f = nullptr;
  if (fopen_s(&f, path, "wb") != 0 || !f) {
    return false;
  }
  static const char kJson[] =
      "{\"type\":\"FeatureCollection\",\"name\":\"multipart_area\","
      "\"features\":[{"
      "\"type\":\"Feature\","
      "\"properties\":{\"name\":\"west-east\",\"adcode\":\"999001\","
      "\"kind\":\"area\"},"
      "\"geometry\":{\"type\":\"MultiPolygon\",\"coordinates\":["
      "[[[73.0,40.0],[75.0,40.0],[75.0,42.0],[73.0,42.0],[73.0,40.0]]],"
      "[[[85.0,40.0],[88.0,40.0],[88.0,43.0],[85.0,43.0],[85.0,40.0]]]"
      "]}}]}";
  const size_t n = std::fwrite(kJson, 1, sizeof(kJson) - 1, f);
  std::fclose(f);
  return n == sizeof(kJson) - 1;
}

bool write_style_sidecar(const char* path) {
  FILE* f = nullptr;
  if (fopen_s(&f, path, "wb") != 0 || !f) {
    return false;
  }
  static const char kJson[] =
      "{\"version\":8,\"name\":\"sidecar\",\"layers\":[{\"id\":\"ln\","
      "\"type\":\"line\",\"source-layer\":\"sidecar_data\","
      "\"paint\":{\"line-color\":\"#FF00AA\",\"line-width\":3}}]}";
  const size_t n = std::fwrite(kJson, 1, sizeof(kJson) - 1, f);
  std::fclose(f);
  return n == sizeof(kJson) - 1;
}

bool write_line_with_siberia_stub(const char* path) {
  FILE* f = nullptr;
  if (fopen_s(&f, path, "wb") != 0 || !f) {
    return false;
  }
  // Layer name comes from file stem "line" (china_city convention).
  static const char kJson[] =
      "{\"type\":\"FeatureCollection\",\"name\":\"line\","
      "\"features\":[{"
      "\"type\":\"Feature\","
      "\"properties\":{\"name\":\"stub\",\"kind\":\"river\"},"
      "\"geometry\":{\"type\":\"LineString\",\"coordinates\":["
      "[100.0,40.0],[110.0,40.0],[110.0,58.0],[100.0,58.0]"
      "]}}]}";
  const size_t n = std::fwrite(kJson, 1, sizeof(kJson) - 1, f);
  std::fclose(f);
  return n == sizeof(kJson) - 1;
}

}  // namespace

int run_map2d_presenter_tests();

int main() {
  base::trace::maybe_init_tracing_from_env();
  struct TraceDumpOnExit {
    ~TraceDumpOnExit() { base::trace::maybe_dump_tracing_to_env(); }
  } trace_dump_on_exit;
  (void)trace_dump_on_exit;

  const std::vector<std::string> rels = content::china_seed_relative_paths();
  expect(!rels.empty(), "seed relatives non-empty");

  const size_t city_gpkg = find_rel(rels, "china_city.gpkg");
  const size_t city_json = find_rel(rels, "china_city.geojson");
  const size_t plp = find_rel(rels, "china_plp.geojson");
  expect(city_gpkg != static_cast<size_t>(-1), "lists china_city.gpkg");
  expect(city_json != static_cast<size_t>(-1), "lists china_city.geojson");
  expect(plp != static_cast<size_t>(-1), "lists china_plp fallback");
  expect(city_gpkg < plp, "china_city.gpkg before china_plp");
  expect(city_json < plp, "china_city.geojson before china_plp");
  expect(city_gpkg < city_json, "gpkg before geojson twin");

  // Palette lives only in the default style document.
  gis::style::StyleDocument carto;
  expect(gis::style::parse_style_document(vista::default_carto_style_json(),
                                          &carto),
         "default carto style parses");
  const uint32_t land = layer_paint_color(carto, "land", "fill-color");
  const uint32_t river = layer_paint_color(carto, "river", "line-color");
  const uint32_t admin = layer_paint_color(carto, "admin", "line-color");
  const uint32_t road = layer_paint_color(carto, "road", "line-color");
  const uint32_t bg = layer_paint_color(carto, "background", "background-color");
  const uint32_t label = layer_paint_color(carto, "label", "text-color");
  expect(land == 0xFFF5F3E9u, "Baidu cream land");
  expect(river == 0xFF4A8AB8u, "river blue");
  expect(admin == 0xFFC4BEB0u, "admin stroke");
  expect(label == 0xFF141820u, "label ink");
  expect(bg == 0xFFAAD3DFu, "ocean bg");

  // MultiPolygon must expand every part (Xinjiang/Qinghai holes otherwise).
  {
    char tmp[MAX_PATH] = {};
    const DWORD n = GetTempPathA(MAX_PATH, tmp);
    expect(n > 0 && n < MAX_PATH, "temp path");
    std::string path = std::string(tmp) + "map_scene_multipart_area.geojson";
    expect(write_multipart_geojson(path.c_str()), "write multipart geojson");
    content::MapScene scene;
    expect(scene.open_path(path), "open multipart geojson");
    expect(scene.last_open_was_ogr(), "multipart open via OGR");
    expect(scene.feature_count() >= 2,
           "MultiPolygon expands to one feature per part");
    std::vector<vista::LonLatRing> rings;
    scene.export_land_rings(&rings);
    expect(rings.size() >= 2, "land rings cover every MultiPolygon part");
    DeleteFileA(path.c_str());
  }

  // NE 10m china_city: area is MultiPolygon; expand must grow land rings past
  // the OGR feature count. Skip quietly when data is not beside cwd.
  {
    // Prefer out/data (GN china_map_samples). Bare out/china_city.* can be an
    // older stub with fewer features and fails the Multi* expand floor.
    const char* city_candidates[] = {
        "..\\data\\china_city.gpkg",
        "..\\data\\china_city.geojson",
        "out\\data\\china_city.gpkg",
        "out\\data\\china_city.geojson",
        "testing\\data\\china_city.gpkg",
        "testing\\data\\china_city.geojson",
        "china_city.gpkg",
        "china_city.geojson",
    };
    for (const char* cand : city_candidates) {
      content::MapScene scene;
      if (!scene.open_path(cand) || !scene.last_open_was_ogr()) {
        continue;
      }
      std::vector<vista::LonLatRing> rings;
      scene.export_land_rings(&rings);
      // OGR area rows are MultiPolygons; each exterior becomes one land ring.
      expect(rings.size() > 48,
             "china_city MultiPolygon parts expanded beyond OGR area count");
      // Ingest expands Multi* parts but also drops Siberia stubs / empties —
      // expect well above OGR area count, not the raw OGR feature sum.
      expect(scene.feature_count() > 1000,
             "china_city total features after Multi* expand");
      break;
    }
  }

  // china_city "line" layer: Siberia stub must be clipped to mainland.
  {
    char tmp[MAX_PATH] = {};
    expect(GetTempPathA(MAX_PATH, tmp) > 0, "temp path for line clip");
    std::string dir = std::string(tmp) + "map_scene_line_clip_dir";
    CreateDirectoryA(dir.c_str(), nullptr);
    std::string path = dir + "\\line.geojson";
    expect(write_line_with_siberia_stub(path.c_str()), "write line.geojson");
    content::MapScene scene;
    expect(scene.open_path(path), "open line.geojson");
    expect(scene.last_open_was_ogr(), "line clip via OGR");
    const content::Extent2 world = scene.world_extent();
    expect(world.ymax <= 54.5, "Siberia stub clipped from line layer");
    expect(world.ymin >= 18.0 && world.ymax <= 54.0,
           "remaining run is mainland lat");
    DeleteFileA(path.c_str());
    RemoveDirectoryA(dir.c_str());
  }

  // M0: append line → write_path GeoJSON → reopen via OGR.
  {
    char tmp[MAX_PATH] = {};
    const DWORD n = GetTempPathA(MAX_PATH, tmp);
    expect(n > 0 && n < MAX_PATH, "temp path for write_path");
    std::string out = std::string(tmp) + "map_scene_m0_write.geojson";
    DeleteFileA(out.c_str());

    content::MapScene a;
    expect(a.create_layer("edit_line", "LineString"), "create line layer");
    tool::Draft draft{};
    draft.kind = tool::DraftKind::kLineString;
    draft.points = {{10, 10}, {200, 150}};
    const content::FeatureId id =
        a.append_from_draft(draft, "draw.linestring");
    expect(id.len != 0, "append line id");
    expect(a.write_path(out), "write_path");
    expect(GetFileAttributesA(out.c_str()) != INVALID_FILE_ATTRIBUTES,
           "file exists");

    content::MapScene b;
    expect(b.open_path(out), "reopen written");
    expect(b.last_open_was_ogr(), "reopen via OGR");
    expect(b.feature_count() >= 1, "reopen feature");
    DeleteFileA(out.c_str());
  }

  // M1: StyleDocument resolve + basemap underlay count + export BMP magic.
  {
    const char* kStyle =
        "{"
        "\"version\":8,"
        "\"name\":\"m1\","
        "\"layers\":[{"
        "\"id\":\"area-fill\","
        "\"type\":\"fill\","
        "\"source-layer\":\"area\","
        "\"paint\":{\"fill-color\":\"#c8e6c9\"}"
        "},{"
        "\"id\":\"line-default\","
        "\"type\":\"line\","
        "\"source-layer\":\"line\","
        "\"paint\":{\"line-color\":\"#1565c0\",\"line-width\":2}"
        "},{"
        "\"id\":\"point-circle\","
        "\"type\":\"circle\","
        "\"source-layer\":\"point\","
        "\"paint\":{\"circle-color\":\"#e65100\"}"
        "}]"
        "}";
    auto doc = std::make_shared<gis::style::StyleDocument>();
    expect(gis::style::parse_style_document(kStyle, doc.get()),
           "parse m1 style");
    content::MapScene scene;
    scene.set_style_document(doc);
    expect(scene.has_style_document(), "has style");
    gis::style::AttrMap attrs;
    gis::style::ResolvedPaint paint;
    expect(scene.resolve_style_for_test("area", attrs, 10.0, &paint),
           "resolve area");
    expect(paint.fill_color == 0xFFC8E6C9u, "area fill #c8e6c9");
    expect(paint.fill_color != 0xFFF5F3E9u, "area fill != Baidu cream");

    // Optional: load shipped china_city.style.json (GN → out/data/).
    const char* style_cands[] = {
        "..\\data\\china_city.style.json",
        "out\\data\\china_city.style.json",
        "china_city.style.json",
        "testing\\data\\china_city.style.json",
    };
    for (const char* cand : style_cands) {
      content::MapScene styled;
      if (styled.load_style_path(cand)) {
        expect(styled.has_style_document(), "load china_city.style.json");
        // File style keys line-water / line-road with filters; match river.
        gis::style::AttrMap line_attrs;
        line_attrs["kind"] = "river";
        gis::style::ResolvedPaint rp;
        expect(styled.resolve_style_for_test("line", line_attrs, 8.0, &rp),
               "resolve line from file");
        expect(rp.line_color == 0xFF1565C0u, "line #1565c0");
        break;
      }
    }

    // Phase 2b: inspector lists ResolvedPaint before legacy GDI hints.
    // Style dumps are opt-in (SMT_FEATURE_INFO_STYLE_DEBUG) for Identify UX.
    _putenv_s("SMT_FEATURE_INFO_STYLE_DEBUG", "1");
    content::MapScene inspector_scene;
    inspector_scene.set_style_document(doc);
    char tmp_path[MAX_PATH] = {};
    expect(GetTempPathA(MAX_PATH, tmp_path) > 0, "temp path");
    const std::string line_json =
        std::string(tmp_path) + "smartgis_phase2b_line.geojson";
    DeleteFileA(line_json.c_str());
    expect(write_line_with_siberia_stub(line_json.c_str()), "write line json");
    inspector_scene.open_path(line_json);
    expect(inspector_scene.feature_count() >= 1, "line feature for inspector");
    const content::MapScene::Feature* line_feat = nullptr;
    for (const content::MapScene::Layer& layer : inspector_scene.layers()) {
      if (layer.name == "line" && !layer.features.empty()) {
        line_feat = &layer.features.front();
        break;
      }
    }
    expect(line_feat != nullptr, "line layer feature");
    std::vector<std::pair<std::string, std::string>> info;
    inspector_scene.fill_feature_info_fields(*line_feat, &info, "line", 10.0);
    bool saw_style_header = false;
    bool saw_line_color = false;
    bool saw_legacy_note = false;
    for (const auto& row : info) {
      if (row.first.find("Style (ResolvedPaint)") != std::string::npos) {
        saw_style_header = true;
      }
      if (row.first == "line-color" && row.second == "#1565C0") {
        saw_line_color = true;
      }
      if (row.first == "note" &&
          row.second.find("edit_config_dock_bar") != std::string::npos) {
        saw_legacy_note = true;
      }
    }
    expect(saw_style_header, "inspector style section");
    expect(saw_line_color, "inspector line-color from ResolvedPaint");
    expect(saw_legacy_note, "legacy render params demoted");
    DeleteFileA(line_json.c_str());

    const std::string sidecar_data =
        std::string(tmp_path) + "sidecar_data.geojson";
    const std::string sidecar_style =
        std::string(tmp_path) + "sidecar_data.style.json";
    DeleteFileA(sidecar_data.c_str());
    DeleteFileA(sidecar_style.c_str());
    {
      FILE* f = nullptr;
      expect(fopen_s(&f, sidecar_data.c_str(), "wb") == 0 && f,
             "sidecar data geojson");
      if (f) {
        static const char kJson[] =
            "{\"type\":\"FeatureCollection\",\"name\":\"sidecar_data\","
            "\"features\":[{\"type\":\"Feature\",\"properties\":{\"kind\":"
            "\"river\"},\"geometry\":{\"type\":\"LineString\",\"coordinates\":["
            "[1.0,1.0],[2.0,2.0]]}}]}";
        const size_t n = std::fwrite(kJson, 1, sizeof(kJson) - 1, f);
        std::fclose(f);
        expect(n == sizeof(kJson) - 1, "sidecar geojson bytes");
      }
    }
    expect(write_style_sidecar(sidecar_style.c_str()), "sidecar style json");
    content::MapScene opened;
    expect(opened.open_path(sidecar_data), "open sidecar data");
    expect(opened.has_style_document(), "open_path loads .style.json");
    gis::style::ResolvedPaint sidecar_paint;
    expect(opened.resolve_style_for_test("sidecar_data", {}, 8.0, &sidecar_paint),
           "resolve sidecar layer");
    expect(sidecar_paint.line_color == 0xFFFF00AAu, "sidecar line #ff00aa");
    DeleteFileA(sidecar_data.c_str());
    DeleteFileA(sidecar_style.c_str());
  }

  // Label collision and scale gates (no golden pixels).
  {
    const vista::detail::LabelScreenBox a{0, 0, 40, 16};
    const vista::detail::LabelScreenBox b{30, 0, 70, 16};
    const vista::detail::LabelScreenBox c{80, 0, 120, 16};
    expect(vista::detail::label_boxes_overlap(a, b), "label boxes overlap");
    expect(!vista::detail::label_boxes_overlap(a, c), "separated boxes");
    const vista::detail::LabelScreenBox stacked[] = {a, b, a};
    expect(vista::detail::accept_label_count(stacked, 3) == 1,
           "overlapping labels collapse to one");
    const vista::detail::LabelScreenBox apart[] = {a, c};
    expect(vista::detail::accept_label_count(apart, 2) == 2,
           "separated labels both accepted");

    expect(vista::detail::label_min_importance(12.0) == 2,
           "country scale keeps capitals and prefectures");
    expect(vista::detail::label_min_importance(30.0) == 1,
           "mid scale adds counties");
    expect(vista::detail::label_min_importance(70.0) == 0,
           "closer scale allows POI text");
    expect(vista::detail::label_min_importance(120.0) == 0,
           "close scale allows POI text");
    expect(vista::detail::place_name_importance("北京市") == 3,
           "municipality is country rank");
    expect(vista::detail::place_name_importance("苏州市") == 2,
           "prefecture city is mid rank");
    expect(vista::detail::place_name_importance("黑龙江省") == 3,
           "province name is country rank");
    expect(vista::detail::place_name_importance("北京市") >=
               vista::detail::label_min_importance(12.0),
           "capital survives country gate");
    expect(vista::detail::place_name_importance("苏州市") >=
               vista::detail::label_min_importance(12.0),
           "prefecture survives country gate");
    expect(vista::detail::place_name_importance("吴中区") <
               vista::detail::label_min_importance(12.0),
           "district hidden at country scale");

    expect(vista::detail::line_role("river", nullptr) == vista::detail::LineRole::kWater,
           "river role");
    expect(vista::detail::line_role("line", "road") == vista::detail::LineRole::kRoad,
           "road class");
    expect(vista::detail::line_role("lake", nullptr) == vista::detail::LineRole::kWater,
           "lake is water");
    expect(!vista::detail::line_visible_at_scale(vista::detail::LineRole::kWater, 0.5,
                                                false, 12.0),
           "short river hidden at country scale");
    expect(vista::detail::line_visible_at_scale(vista::detail::LineRole::kWater, 8.0,
                                               false, 12.0),
           "long river kept at country scale");
    expect(vista::detail::line_visible_at_scale(vista::detail::LineRole::kWater, 0.5,
                                               false, 120.0),
           "short river returns when zoomed in");
    expect(!vista::detail::line_visible_at_scale(vista::detail::LineRole::kRoad, 0.4,
                                                false, 12.0),
           "non-major road hidden at country scale");
    // National frame keeps major/secondary arterials (score + visual review);
    // tiny stubs stay culled so gold casing does not wash the cream land.
    // National frame floor is 0.12°; stubs under that stay culled.
    expect(!vista::detail::line_visible_at_scale(vista::detail::LineRole::kRoad, 0.1,
                                                true, 12.0),
           "tiny major stub hidden at country scale");
    expect(vista::detail::line_visible_at_scale(vista::detail::LineRole::kRoad, 0.4,
                                               true, 12.0),
           "major-class road kept at country scale");
    expect(vista::detail::line_visible_at_scale(vista::detail::LineRole::kRoad, 1.0,
                                               true, 48.0),
           "major-class road remains past country scale");
    expect(road != river, "road ink differs from river");
    expect(vista::detail::line_stroke_px(vista::detail::LineRole::kWater, 8.0, 12.0) !=
               vista::detail::line_stroke_px(vista::detail::LineRole::kWater, 8.0,
                                            60.0),
           "river width changes with scale");
    expect(vista::detail::line_stroke_px(vista::detail::LineRole::kRoad, 8.0, 12.0) >= 1,
           "road stroke is positive");

    // length is the cartographic span (deg); endpoints are documentary only.
    // Country water gate uses min_len=0.8 at scale<22; stem aggregation joins
    // same-name pieces so short segments of a major river stay visible.
    vista::detail::StemSpan parts[] = {
        {"ChangJiang", 2.0, 100.0, 30.0, 102.0, 30.0},
        {"ChangJiang", 2.0, 110.0, 30.0, 112.0, 30.0},
        {"ChangJiang", 2.0, 120.0, 30.0, 122.0, 30.0},
        {"ChangJiang", 2.0, 130.0, 30.0, 132.0, 30.0},
    };
    const double stem =
        vista::detail::stem_length(parts, 4, 0, 0.05);
    expect(stem > 7.9 && stem < 8.1, "same-name pieces form one stem");
    expect(!vista::detail::line_visible_at_scale(vista::detail::LineRole::kWater, 0.5,
                                                false, 12.0),
           "one short piece fails the country gate");
    expect(vista::detail::line_visible_at_scale(vista::detail::LineRole::kWater, 2.0,
                                               false, 12.0),
           "mid piece alone clears the relaxed country gate");
    expect(vista::detail::line_visible_at_scale(vista::detail::LineRole::kWater, stem,
                                               false, 12.0),
           "stem length passes the country gate");
    vista::detail::StemSpan tributary[] = {
        {"ChangJiang", 2.0, 0.0, 0.0, 2.0, 0.0},
        {"Jialing", 0.4, 2.0, 0.0, 2.4, 0.0},
    };
    const double trib =
        vista::detail::stem_length(tributary, 2, 1, 0.05);
    expect(trib > 0.3 && trib < 0.5, "named tributary stays separate");

    const double xs[] = {0.0, 4.0, 4.0};
    const double ys[] = {0.0, 0.0, 2.0};
    const vista::detail::LineLabelAnchor anchor =
        vista::detail::line_label_anchor(xs, ys, 3);
    expect(anchor.ok, "line label anchor");
    expect(anchor.x > 2.9 && anchor.x < 3.1 && anchor.y > -0.05 &&
               anchor.y < 0.05,
           "label sits at mid-length, not a later vertex");
    expect(anchor.angle_deg > -1.0 && anchor.angle_deg < 1.0,
           "tangent follows the mid segment");
    expect(anchor.y < 0.2, "not the vertex centroid");

    expect(vista::detail::extent_is_lonlat(73.0, 18.0, 135.0, 54.0),
           "China bbox is lon/lat");
    expect(!vista::detail::extent_is_lonlat(400000.0, 3000000.0, 500000.0,
                                           3500000.0),
           "meter window is not lon/lat");
    // Country water gate needs >=4° (~445 km at 111320 m/deg).
    const double long_m =
        vista::detail::length_as_degrees(500000.0, false);
    const double short_m = vista::detail::length_as_degrees(400.0, false);
    expect(vista::detail::line_visible_at_scale(vista::detail::LineRole::kWater, long_m,
                                               false, 12.0),
           "long meter river still passes country gate");
    expect(!vista::detail::line_visible_at_scale(vista::detail::LineRole::kWater,
                                                short_m, false, 12.0),
           "short meter creek is culled");
  }

  g_fails += run_map2d_presenter_tests();
  if (g_fails) {
    std::fprintf(stderr, "%d map_scene_test fail(s)\n", g_fails);
    return 1;
  }
  std::printf("map_scene_test ok\n");
  return 0;
}
