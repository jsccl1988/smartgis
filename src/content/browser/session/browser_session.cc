// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/session/browser_session.h"

#include <cstdlib>
#include <cstring>

#include "base/process/switches.h"
#include "base/trace/event/process_trace.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/debug/debug_agent.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "content/public/map_contents.h"
#include "content/public/view_host.h"
#include "vista/component/world/atmosphere/environment.h"

namespace content {
namespace {

bool env_flag_on(const char* name) {
  const char* env = base::switch_cstr(name);
  return env && env[0] == '1' && env[1] == '\0';
}

// Explicit opt-in to start OOP GPU at init_hosts (legacy / debug).
// Default is delay until ensure_oop_render_process().
bool want_oop_at_init() {
  if (env_flag_on("disable-oop-render")) {
    return false;
  }
  return env_flag_on("enable-oop-render");
}

}  // namespace

std::unique_ptr<BrowserSession> BrowserSession::create() {
  return std::make_unique<BrowserSession>();
}

BrowserSession::BrowserSession() = default;

BrowserSession::~BrowserSession() {
  prepare_close();
  clear_map_contents_observer();
}

void BrowserSession::init_hosts() {
  edit_host_ = std::make_unique<ViewHost>();
  data_host_ = std::make_unique<ViewHost>();
  scene_host_ = std::make_unique<ViewHost>();
  // OOP MapContents is optional for in-process present (atmosphere / map2d
  // showcase). Create the session object eagerly; StartRenderProcess is
  // deferred until ensure_oop_render_process() (or ENABLE_OOP_RENDER=1).
  // Deferred: MapContents::Create during init_hosts was heap-corrupting the
  // next CRT alloc in PluginShell::CommandCatalog (0xC0000374). Create on
  // first ensure_oop_render_process / present attach instead.
  if (want_oop_at_init()) {
    if (!ensure_oop_render_process()) {
      map_contents_.reset();
    }
  }
}

bool BrowserSession::ensure_oop_render_process() {
  if (!map_contents_) {
    BASE_TRACE_EVENT("MapContents.Create", "startup");
    map_contents_.reset(MapContents::Create());
    if (!map_contents_) {
      return false;
    }
  }
  if (map_contents_->IsOopRender()) {
    return true;
  }
  if (env_flag_on("disable-oop-render")) {
    return false;
  }
  bool ok = false;
  {
    BASE_TRACE_EVENT("StartRenderProcess", "startup");
    ok = map_contents_->StartRenderProcess();
  }
  if (!ok) {
    // Keep the MapContents object for CatalogCall no-ops; callers that need
    // a live pipe check IsOopRender(). Dropping here matches historical
    // init_hosts failure (clear session) only when Create was for OOP-at-init.
    return false;
  }
  return true;
}

void BrowserSession::prepare_close() {
  if (prepare_close_done_) {
    return;
  }
  prepare_close_done_ = true;
  edit_gestures_.detach();
  data_gestures_.detach();
  scene_gestures_.detach();
  // Caller (Browser::prepare_close) must detach MapViewport / join Display
  // before this so abandon does not race a live present_mu_ holder.
  if (scene3d_) {
    scene3d_->abandon_mesh();
  }
  scene3d_stereo_.release();
}

void BrowserSession::set_map_contents_observer(MapContentsObserver* observer) {
  if (map_contents_) {
    map_contents_->SetObserver(observer);
  }
}

void BrowserSession::clear_map_contents_observer() {
  if (map_contents_) {
    map_contents_->SetObserver(nullptr);
  }
}

bool BrowserSession::is_extent_nonempty(const content::Extent2& e) {
  return content::extent_nonempty(e);
}

bool BrowserSession::extent_looks_like_china(const content::Extent2& e) {
  return content::extent_looks_like_china(e);
}

content::Extent2 BrowserSession::china_lon_lat_extent() {
  return content::kChinaLonLatExtent;
}

content::Extent2 BrowserSession::china_map2d_frame_extent() {
  return content::kChinaMap2dFrameExtent;
}

void BrowserSession::use_scene3d_engine_flycube() {
  content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
}

void BrowserSession::use_scene3d_engine_gdi() {
  content::set_scene3d_engine(content::Scene3dEngine::kGdi);
}

void BrowserSession::use_scene3d_engine_stereo_gl() {
  content::set_scene3d_engine(content::Scene3dEngine::kStereoGl);
}

void BrowserSession::use_scene3d_engine_scenic() {
  content::set_scene3d_engine(content::Scene3dEngine::kScenic);
}

bool BrowserSession::prefers_scene3d_flycube() {
  return content::prefer_scene3d_flycube();
}

bool BrowserSession::prefers_scene3d_stereo_gl() {
  return content::prefer_scene3d_stereo_gl();
}

bool BrowserSession::prefers_scene3d_gdi() {
  return content::prefer_scene3d_gdi();
}

bool BrowserSession::prefers_scene3d_scenic() {
  return content::prefer_scene3d_scenic();
}

bool BrowserSession::prefers_map2d_scenic() {
  return content::prefer_map2d_scenic();
}

bool BrowserSession::scene3d_engine_selected_from_env() {
  return content::apply_scene3d_engine_from_env();
}

bool BrowserSession::debug_console_env_enabled() {
  return content::debug_console_env_enabled();
}

void BrowserSession::start_debug_agent() {
  content::debug_agent().start();
}

bool BrowserSession::open_document(const std::string& path) {
  return document_.open_path(path);
}

bool BrowserSession::open_document(const std::string& path,
                                   bool load_accompanying_style) {
  return document_.open_path(path, load_accompanying_style);
}

bool BrowserSession::write_document(const std::string& path) const {
  return document_.write_path(path);
}

void BrowserSession::seed_default_document(bool allow_china) {
  document_.seed_default(allow_china);
}

bool BrowserSession::create_layer(const std::string& name,
                                  const std::string& geometry_type) {
  return document_.create_layer(name, geometry_type);
}

bool BrowserSession::remove_layer(const std::string& id) {
  return document_.remove_layer(id);
}

bool BrowserSession::move_layer(const std::string& id, int delta) {
  return document_.move_layer(id, delta);
}

bool BrowserSession::select_layer(const std::string& id) {
  return document_.select_layer(id);
}

bool BrowserSession::set_layer_visible(const std::string& id, bool visible) {
  return document_.set_layer_visible(id, visible);
}

void BrowserSession::set_basemap_provider(
    std::shared_ptr<gis::tile::TileProvider> provider) {
  document_.set_basemap_provider(std::move(provider));
}

void BrowserSession::clear_selection() {
  document_.clear_selection();
}

void BrowserSession::clear_style_document() {
  document_.clear_style_document();
}

bool BrowserSession::load_style_path(const std::string& path) {
  return document_.load_style_path(path);
}

bool BrowserSession::select_feature(const content::FeatureId& id) {
  return document_.select_feature(id);
}

bool BrowserSession::update_feature_field(const std::string& token,
                                          const std::string& field,
                                          const std::string& value) {
  return document_.update_feature_field(token, field, value);
}

content::FeatureId BrowserSession::append_from_draft(const tool::Draft& draft,
                                                     const char* tool_id) {
  return document_.append_from_draft(
      draft, tool_id,
      [this](int view_x, int view_y, double* map_x, double* map_y) {
        view_frame_.view_to_map(view_x, view_y, map_x, map_y);
      });
}

content::FeatureId BrowserSession::move_selected_vertex(double map_x,
                                                        double map_y,
                                                        double tol_map) {
  return document_.move_selected_vertex(map_x, map_y, tol_map);
}

content::FeatureId BrowserSession::select_feature_at(double map_x, double map_y,
                                                     double tol_map) {
  const MapScene::Feature* hit = document_.hit_test(map_x, map_y, tol_map);
  return hit ? hit->id : content::FeatureId{};
}

bool BrowserSession::copy_feature_xy(
    const content::FeatureId& id,
    std::vector<std::pair<double, double>>* out) const {
  return document_.copy_feature_xy(id, out);
}

bool BrowserSession::document_has_china_extent() const {
  return document_.has_china_extent();
}

std::size_t BrowserSession::document_layer_count() const {
  return document_.layer_count();
}

std::size_t BrowserSession::document_feature_count() const {
  return document_.feature_count();
}

bool BrowserSession::document_last_open_was_ogr() const {
  return document_.last_open_was_ogr();
}

content::Extent2 BrowserSession::document_world_extent() const {
  return document_.world_extent();
}

bool BrowserSession::document_compute_extent(double* min_x, double* min_y,
                                             double* max_x, double* max_y) const {
  return document_.compute_extent(min_x, min_y, max_x, max_y);
}

bool BrowserSession::document_active_layer_world_extent(
    content::Extent2* out) const {
  return document_.active_layer_world_extent(out);
}

bool BrowserSession::document_selection_world_extent(
    content::Extent2* out) const {
  return document_.selection_world_extent(out);
}

bool BrowserSession::document_has_style() const {
  return document_.has_style_document();
}

const gis::style::StyleDocument* BrowserSession::document_style() const {
  return document_.style_document();
}

std::shared_ptr<gis::style::StyleDocument>
BrowserSession::style_document_shared() const {
  return document_.style_document_shared();
}

std::vector<content::LayerDesc> BrowserSession::document_layer_descs() const {
  return document_.layer_descs();
}

void BrowserSession::apply_view_pan(int dx_px, int dy_px) {
  view_frame_.apply_pan(dx_px, dy_px);
}

void BrowserSession::apply_view_zoom_at(int view_x, int view_y, double factor) {
  view_frame_.apply_zoom_at(view_x, view_y, factor);
}

void BrowserSession::apply_view_pinch(int view_x, int view_y, double scale) {
  view_frame_.apply_pinch(view_x, view_y, scale);
}

void BrowserSession::apply_view_world_extent(const content::Extent2& e,
                                             int view_w, int view_h) {
  view_frame_.apply_world_extent(e, view_w, view_h);
}

void BrowserSession::reapply_view_world_extent(int view_w, int view_h) {
  view_frame_.apply_world_extent(view_frame_.view_world_extent(view_w, view_h),
                                 view_w, view_h);
}

void BrowserSession::view_to_map(int view_x, int view_y, double* map_x,
                                 double* map_y) const {
  view_frame_.view_to_map(view_x, view_y, map_x, map_y);
}

double BrowserSession::view_scale() const {
  return view_frame_.scale();
}

content::Extent2 BrowserSession::view_world_extent(int view_w,
                                                   int view_h) const {
  return view_frame_.view_world_extent(view_w, view_h);
}

void BrowserSession::frame_view_and_orbit_to_document(int view_w, int view_h) {
  view_frame_.fit_extent(document_, view_w, view_h);
  orbit_.apply_world_extent(document_.world_extent());
}

void BrowserSession::frame_china_map2d_extents(int view_w, int view_h) {
  view_frame_.apply_world_extent(kChinaMap2dFrameExtent, view_w, view_h);
  orbit_.apply_world_extent(kChinaLonLatExtent);
}

void BrowserSession::reset_orbit() {
  orbit_.reset();
}

void BrowserSession::apply_orbit_world_extent(const content::Extent2& e) {
  orbit_.apply_world_extent(e);
}

void BrowserSession::apply_orbit_pan(int dx_px, int dy_px) {
  orbit_.apply_pan(dx_px, dy_px);
}

void BrowserSession::apply_orbit_pinch(int view_x, int view_y, double scale,
                                       int view_w, int view_h) {
  orbit_.apply_pinch(view_x, view_y, scale, view_w, view_h);
}

void BrowserSession::set_orbit_extent(const content::Extent2& e) {
  orbit_.set_extent(e);
}

void BrowserSession::set_orbit_distance(float distance) {
  orbit_.set_distance(distance);
}

void BrowserSession::set_orbit_pitch(float pitch) {
  orbit_.set_pitch(pitch);
}

void BrowserSession::set_orbit_yaw(float yaw) {
  orbit_.set_yaw(yaw);
}

content::Extent2 BrowserSession::orbit_world_extent() const {
  return orbit_.world_extent();
}

content::Extent2 BrowserSession::orbit_extent() const {
  return orbit_.extent();
}

float BrowserSession::orbit_yaw() const {
  return orbit_.yaw();
}

float BrowserSession::orbit_pitch() const {
  return orbit_.pitch();
}

float BrowserSession::orbit_distance() const {
  return orbit_.distance();
}

void BrowserSession::apply_china_scene3d_orbit() {
  orbit_.reset();
  orbit_.apply_world_extent(kChinaLonLatExtent);
  orbit_.set_distance(1.45f);
  orbit_.set_pitch(0.52f);
  orbit_.set_yaw(kScene3dDefaultYaw);
}

void BrowserSession::pull_orbit_extent_from_document() {
  const content::Extent2 doc = document_.world_extent();
  if (content::extent_nonempty(doc)) {
    orbit_.set_extent(doc);
    return;
  }
  if (!content::extent_nonempty(orbit_.extent())) {
    orbit_.set_extent(kChinaLonLatExtent);
  }
}

content::Extent2 BrowserSession::orbit_extent_for_scene_tab() {
  content::Extent2 e = orbit_.world_extent();
  if (!content::extent_looks_like_china(e)) {
    e = kChinaLonLatExtent;
    orbit_.apply_world_extent(e);
  }
  return e;
}

content::Extent2 BrowserSession::adopt_orbit_from_view(int view_w, int view_h) {
  content::Extent2 e = view_frame_.view_world_extent(view_w, view_h);
  if (!content::extent_looks_like_china(e)) {
    const content::Extent2 world = document_.world_extent();
    e = content::extent_looks_like_china(world) ? world : kChinaLonLatExtent;
  }
  orbit_.apply_world_extent(e);
  return e;
}

bool BrowserSession::navigation_commit(const content::Extent2& extent) {
  return navigation_.commit(extent);
}

void BrowserSession::navigation_reset(const content::Extent2& extent) {
  navigation_.reset(extent);
}

bool BrowserSession::navigation_previous() {
  return navigation_.previous();
}

bool BrowserSession::navigation_next() {
  return navigation_.next();
}

bool BrowserSession::navigation_zoom_layer(const content::Extent2* target) {
  return navigation_.zoom_layer(target);
}

bool BrowserSession::navigation_zoom_selection(const content::Extent2* target) {
  return navigation_.zoom_selection(target);
}

void BrowserSession::navigation_note_no_feature() {
  navigation_.note_no_feature();
}

void BrowserSession::navigation_add_bookmark() {
  navigation_.add_bookmark();
}

bool BrowserSession::navigation_go_bookmark(std::size_t index) {
  return navigation_.go_bookmark(index);
}

const std::string& BrowserSession::navigation_status() const {
  return navigation_.status();
}

const content::Extent2& BrowserSession::navigation_extent() const {
  return navigation_.extent();
}

const std::vector<content::ViewBookmark>&
BrowserSession::navigation_bookmarks() const {
  return navigation_.bookmarks();
}

void BrowserSession::blit_begin_pan(int view_w, int view_h, int dx_px,
                                    int dy_px) {
  blit_.begin_pan(view_w, view_h, dx_px, dy_px);
}

void BrowserSession::blit_begin_zoom(int view_w, int view_h, int cursor_x,
                                     int cursor_y, double factor) {
  blit_.begin_zoom(view_w, view_h, cursor_x, cursor_y, factor);
}

void BrowserSession::blit_end_preview() {
  blit_.end_preview();
}

bool BrowserSession::blit_in_preview() const {
  return blit_.in_preview();
}

bool BrowserSession::blit_present(HDC dst, int view_w, int view_h) const {
  return blit_.present(dst, view_w, view_h);
}

void BrowserSession::blit_capture(HDC src, int w, int h) {
  blit_.capture(src, w, h);
}

void BrowserSession::invalidate_map2d_frame_cache() {
  map2d().invalidate_frame_cache();
}

void BrowserSession::note_map2d_surface_reset() {
  map2d().note_surface_reset();
}

void BrowserSession::map2d_paint(HDC hdc, int width_px, int height_px,
                                 bool fill_background) const {
  map2d().paint(hdc, width_px, height_px, fill_background);
}

void BrowserSession::map2d_paint_annotation(HDC hdc, int width_px,
                                            int height_px) const {
  map2d().paint_annotation_overlay(hdc, width_px, height_px);
}

void BrowserSession::map2d_paint_flash(HDC hdc, int width_px,
                                       int height_px) const {
  map2d().paint_flash_overlay(hdc, width_px, height_px);
}

bool BrowserSession::map2d_export_bmp(const std::string& path, int width_px,
                                      int height_px) const {
  return map2d().export_bmp(path, width_px, height_px);
}

bool BrowserSession::map2d_present_gpu(render::rhi::Device* device,
                                       std::uint32_t width_px,
                                       std::uint32_t height_px,
                                       const ui::gfx::ShellRaster* shell,
                                       std::uint64_t shell_generation) {
  return map2d().present_gpu(device, width_px, height_px, shell,
                             shell_generation);
}

bool BrowserSession::map2d_last_gpu_present_ok() const {
  return map2d().last_gpu_present_ok();
}

bool BrowserSession::map2d_last_gpu_present_drew() const {
  return map2d().last_gpu_present_drew();
}

std::uint64_t BrowserSession::map2d_layout_build_count() const {
  return map2d().layout_build_count();
}

bool BrowserSession::map2d_last_present_reused_layout() const {
  return map2d().last_present_reused_layout();
}

void BrowserSession::bind_map_presenters() {
  map2d().bind(&document_, &view_frame_);
  scene3d().bind_orbit(&orbit_);
  scene3d().bind_label_frame(&view_frame_);
  scene3d().bind_map(&document_);
}

void BrowserSession::bind_scene3d_document() {
  scene3d().bind_map(&document_);
}

void BrowserSession::bind_scene3d_contents(MapContents* contents,
                                           std::uint32_t view_id) {
  scene3d().bind_contents(contents, view_id);
}

void BrowserSession::scene3d_apply_draft(const tool::Draft& draft) {
  scene3d().apply_draft(draft);
}

void BrowserSession::scene3d_paint(HDC hdc, int width_px, int height_px,
                                   bool fill_background) const {
  scene3d().paint(hdc, width_px, height_px, fill_background);
}

void BrowserSession::scene3d_paint_hud(HDC hdc, int width_px,
                                       int height_px) const {
  scene3d().paint_hud(hdc, width_px, height_px);
}

void BrowserSession::scene3d_set_render_engine_name(const char* name) {
  scene3d().set_render_engine_name(name);
}

bool BrowserSession::scene3d_present_gpu(render::rhi::Device* device,
                                         std::uint32_t width_px,
                                         std::uint32_t height_px,
                                         const ui::gfx::ShellRaster* shell,
                                         std::uint64_t shell_generation) {
  return scene3d().present_gpu(device, width_px, height_px, shell,
                               shell_generation);
}

void BrowserSession::abandon_scene3d_mesh() {
  scene3d().abandon_mesh();
}

void BrowserSession::select_scene3d_flycube(const char* hud_label) {
  use_scene3d_engine_flycube();
  scene3d().set_render_engine_name(hud_label);
}

void BrowserSession::select_scene3d_stereo_gl(const char* hud_label) {
  use_scene3d_engine_stereo_gl();
  scene3d().set_render_engine_name(hud_label);
}

void BrowserSession::select_scene3d_gdi(const char* hud_label) {
  use_scene3d_engine_gdi();
  scene3d().set_render_engine_name(hud_label);
}

void BrowserSession::set_scene3d_look_atmosphere() {
  scene3d().set_look_preset(Scene3dLookPreset::kAtmosphere);
}

void BrowserSession::set_scene3d_look_legacy() {
  scene3d().set_look_preset(Scene3dLookPreset::kLegacyStereo);
}

void BrowserSession::seed_scene3d_procedural(bool with_land_rings) {
  scene3d().atmosphere_session().seed_procedural(with_land_rings);
}

void BrowserSession::seed_scene3d_procedural_if_empty() {
  AtmosphereSession& atmo = scene3d().atmosphere_session();
  const vista::atmosphere::Environment* env = atmo.environment();
  if (!env || env->field_store().layer_count() == 0) {
    atmo.seed_procedural();
  }
}

void BrowserSession::set_scene3d_atmosphere_layers(bool ocean, bool cloud,
                                                   bool sky, bool fog) {
  AtmosphereSession& atmo = scene3d().atmosphere_session();
  atmo.set_ocean_enabled(ocean);
  atmo.set_cloud_enabled(cloud);
  atmo.set_sky_enabled(sky);
  atmo.set_fog_enabled(fog);
}

void BrowserSession::set_scene3d_globe_enabled(bool on) {
  scene3d().atmosphere_session().set_globe_enabled(on);
}

void BrowserSession::set_scene3d_cloud_enabled(bool on) {
  scene3d().atmosphere_session().set_cloud_enabled(on);
}

void BrowserSession::set_scene3d_wind_overlay(bool on) {
  scene3d().atmosphere_session().set_wind_overlay_enabled(on);
}

bool BrowserSession::apply_scene3d_contour_suite_or_elevation() {
  AtmosphereSession& atmo = scene3d().atmosphere_session();
  if (atmo.apply_contour_suite_defaults()) {
    return true;
  }
  atmo.set_elevation_overlay(true, true);
  return false;
}

bool BrowserSession::ensure_scene3d_legacy_overlays() {
  return scene3d().ensure_legacy_overlays();
}

bool BrowserSession::load_scene3d_fields(std::string_view spec) {
  return scene3d().atmosphere_session().load_fields(spec);
}

void BrowserSession::set_scene3d_time_sec(double t) {
  scene3d().atmosphere_session().set_time_sec(t);
}

double BrowserSession::scene3d_time_sec() const {
  return scene3d().atmosphere_session().time_sec();
}

bool BrowserSession::scene3d_time_range(double* t_min, double* t_max) const {
  if (!t_min || !t_max) {
    return false;
  }
  const vista::atmosphere::Environment* env =
      scene3d().atmosphere_session().environment();
  if (!env) {
    return false;
  }
  static const vista::atmosphere::FieldChannel kRangeOrder[] = {
      vista::atmosphere::FieldChannel::kWaveHs,
      vista::atmosphere::FieldChannel::kCloudCover,
      vista::atmosphere::FieldChannel::kWindU,
      vista::atmosphere::FieldChannel::kWindV,
      vista::atmosphere::FieldChannel::kWaveDir,
      vista::atmosphere::FieldChannel::kCloudBase,
      vista::atmosphere::FieldChannel::kCloudTop,
      vista::atmosphere::FieldChannel::kSeaMask,
  };
  for (vista::atmosphere::FieldChannel ch : kRangeOrder) {
    if (env->timed_field_range(ch, t_min, t_max)) {
      return true;
    }
  }
  return false;
}

void BrowserSession::set_scene3d_tileset_content_root(const std::string& root) {
  scene3d().gpu().set_tileset_content_root(root);
}

bool BrowserSession::attach_scene3d_tileset_json(const char* json,
                                                 std::size_t len,
                                                 const char* name) {
  return scene3d().gpu().attach_tileset_json(json, len, name);
}

void BrowserSession::set_scene3d_overlay_tin_mesh(
    const float* xyz, int point_count, const unsigned* indices,
    int index_count, const std::uint8_t* albedo) {
  scene3d().set_overlay_tin_mesh(xyz, point_count, indices, index_count, albedo);
}

void BrowserSession::set_scene3d_overlay_tin_drape(
    const std::uint8_t* rgba, std::uint32_t width, std::uint32_t height,
    const float* uv, int uv_float_count) {
  scene3d().set_overlay_tin_drape(rgba, width, height, uv, uv_float_count);
}

void BrowserSession::clear_scene3d_overlay_tin() {
  scene3d().clear_overlay_tin_mesh();
}

void BrowserSession::release_scene3d_stereo() {
  scene3d_stereo_.release();
}

void BrowserSession::abandon_scene3d_stereo() {
  scene3d_stereo_.abandon();
}

bool BrowserSession::try_attach_scene3d_stereo(HWND hwnd) {
  return scene3d_stereo_.try_attach(hwnd);
}

bool BrowserSession::try_present_scene3d_stereo(HWND hwnd, HDC hdc, int width_px,
                                                int height_px) {
  return scene3d_stereo_.try_present_sot(hwnd, hdc, width_px, height_px,
                                         orbit_.yaw(), orbit_.pitch(),
                                         orbit_.distance());
}

void BrowserSession::attach_gestures(MapHwndGestures* gestures, HWND hwnd,
                                     MapHwndGestures::PinchFn on_pinch,
                                     MapHwndGestures::PanFn on_pan) {
  if (!gestures || !hwnd) {
    return;
  }
  gestures->attach(hwnd, std::move(on_pinch), std::move(on_pan));
}

void BrowserSession::configure_gestures(
    MapHwndGestures* gestures, MapHwndGestures::RightClickFn on_right_click,
    MapHwndGestures::ExtentWatchFn on_extent_watch,
    MapHwndGestures::ResizeFn on_resized) {
  if (!gestures) {
    return;
  }
  gestures->set_right_click(std::move(on_right_click));
  gestures->set_extent_watch(std::move(on_extent_watch));
  gestures->set_viewport_resized(std::move(on_resized));
}

}  // namespace content
