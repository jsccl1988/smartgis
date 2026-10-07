// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_SESSION_BROWSER_SESSION_H_
#define CONTENT_BROWSER_SESSION_BROWSER_SESSION_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/camera/view_navigation.h"
#include "content/browser/document/gis_scene.h"
#include "content/browser/session/gis_hwnd_gestures.h"
#include "content/browser/present/host/blit_frame_cache.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_stereo_session.h"

namespace content {

class GisContents;
class GisContentsObserver;
class ToolSession;

// In-process browser session (Chromium WebContents analogue for Views).
// Owns the document, camera, map2d and scene3d present facades, HWND
// gestures, ToolSessions, and the optional GisContents OOP/GPU pipe.
// Not absorbed into content.dll — linked via //src/content:browser_session
// (see shell §Content sink C5/C6).
class BrowserSession {
 public:
  // Heap-allocate in this TU. Embedding BrowserSession by value in app::Browser
  // used browser.cc sizeof vs this TU's member ctors and smashed a freed CRT
  // block (HEAP: modified after free → string member in Browser()).
  static std::unique_ptr<BrowserSession> create();

  BrowserSession();
  ~BrowserSession();

  BrowserSession(const BrowserSession&) = delete;
  BrowserSession& operator=(const BrowserSession&) = delete;

  // Create ToolSessions. create_gis_contents is deferred past PluginShell catalog
  // init (eager Create during init_tool_sessions heap-corrupted the next CRT alloc).
  // Call ensure_gis_contents() after PluginShell::init; StartRenderProcess
  // still waits for ensure_oop_render_process unless ENABLE_OOP_RENDER=1.
  void init_tool_sessions();

  // Create GisContents without starting the OOP GPU child. Idempotent.
  bool ensure_gis_contents();

  // Lazily start the OOP GPU child. No-op when already running or when
  // DISABLE_OOP_RENDER=1. Returns true when IsOopRender().
  bool ensure_oop_render_process();

  // Detach gestures / abandon mesh / release stereo before HWND teardown.
  void prepare_close();

  void set_gis_contents_observer(GisContentsObserver* observer);
  void clear_gis_contents_observer();

  GisScene& document() { return document_; }
  const GisScene& document() const { return document_; }
  ViewFrame& view_frame() { return view_frame_; }
  const ViewFrame& view_frame() const { return view_frame_; }
  OrbitFrame& orbit_frame() { return orbit_; }
  const OrbitFrame& orbit_frame() const { return orbit_; }
  Map2dPresenter& map2d() {
    if (!map2d_) {
      map2d_ = Map2dPresenter::create();
    }
    return *map2d_;
  }
  const Map2dPresenter& map2d() const { return const_cast<BrowserSession*>(this)->map2d(); }
  Scene3dPresenter& scene3d() {
    if (!scene3d_) {
      scene3d_ = Scene3dPresenter::create();
    }
    return *scene3d_;
  }
  const Scene3dPresenter& scene3d() const {
    return const_cast<BrowserSession*>(this)->scene3d();
  }
  Scene3dStereoSession& scene3d_stereo() { return scene3d_stereo_; }
  BlitFrameCache& blit() { return blit_; }
  ViewNavigation& navigation() { return navigation_; }
  const ViewNavigation& navigation() const { return navigation_; }

  GisHwndGestures& edit_gestures() { return edit_gestures_; }
  GisHwndGestures& data_gestures() { return data_gestures_; }
  GisHwndGestures& scene_gestures() { return scene_gestures_; }

  // Non-const pointers from const BrowserSession match std::unique_ptr::get().
  GisContents* gis_contents() const { return gis_contents_.get(); }
  ToolSession* edit_tool_session() const { return edit_tool_session_.get(); }
  ToolSession* data_tool_session() const { return data_tool_session_.get(); }
  ToolSession* scene_tool_session() const { return scene_tool_session_.get(); }

  // Content mutations. App keeps product policy and widget updates and calls
  // these instead of driving GisScene, presenters, camera, gestures, or blit.

  static bool is_extent_nonempty(const content::Extent2& e);
  static bool extent_looks_like_china(const content::Extent2& e);
  static content::Extent2 china_lon_lat_extent();
  static content::Extent2 china_map2d_frame_extent();

  static void use_scene3d_engine_flycube();
  static void use_scene3d_engine_gdi();
  static void use_scene3d_engine_stereo_gl();
  static void use_scene3d_engine_scenic();
  static bool prefers_scene3d_flycube();
  static bool prefers_scene3d_stereo_gl();
  static bool prefers_scene3d_gdi();
  static bool prefers_scene3d_scenic();
  static bool prefers_map2d_scenic();
  static bool scene3d_engine_selected_from_env();

  static bool debug_console_env_enabled();
  static void start_debug_agent();

  bool open_document(const std::string& path);
  bool open_document(const std::string& path, bool load_accompanying_style);
  bool write_document(const std::string& path) const;
  void seed_default_document(bool allow_china);
  bool create_layer(const std::string& name, const std::string& geometry_type);
  bool remove_layer(const std::string& id);
  bool move_layer(const std::string& id, int delta);
  bool select_layer(const std::string& id);
  bool set_layer_visible(const std::string& id, bool visible);
  void set_basemap_provider(std::shared_ptr<gis::tile::TileProvider> provider);
  void clear_selection();
  void clear_style_document();
  bool load_style_path(const std::string& path);
  bool select_feature(const content::FeatureId& id);
  bool update_feature_field(const std::string& token, const std::string& field,
                            const std::string& value);
  content::FeatureId append_from_draft(const tool::Draft& draft,
                                       const char* tool_id);
  content::FeatureId move_selected_vertex(double map_x, double map_y,
                                          double tol_map);
  content::FeatureId select_feature_at(double map_x, double map_y, double tol_map);
  bool copy_feature_xy(const content::FeatureId& id,
                       std::vector<std::pair<double, double>>* out) const;

  bool document_has_china_extent() const;
  std::size_t document_layer_count() const;
  std::size_t document_feature_count() const;
  bool document_last_open_was_ogr() const;
  content::Extent2 document_world_extent() const;
  bool document_compute_extent(double* min_x, double* min_y, double* max_x,
                               double* max_y) const;
  bool document_active_layer_world_extent(content::Extent2* out) const;
  bool document_selection_world_extent(content::Extent2* out) const;
  bool document_has_style() const;
  const gis::style::StyleDocument* document_style() const;
  std::shared_ptr<gis::style::StyleDocument> style_document_shared() const;
  std::vector<content::LayerDesc> document_layer_descs() const;

  void apply_view_pan(int dx_px, int dy_px);
  void apply_view_zoom_at(int view_x, int view_y, double factor);
  void apply_view_pinch(int view_x, int view_y, double scale);
  void apply_view_world_extent(const content::Extent2& e, int view_w, int view_h);
  void reapply_view_world_extent(int view_w, int view_h);
  void view_to_map(int view_x, int view_y, double* map_x, double* map_y) const;
  double view_scale() const;
  content::Extent2 view_world_extent(int view_w, int view_h) const;
  void frame_view_and_orbit_to_document(int view_w, int view_h);
  void frame_china_map2d_extents(int view_w, int view_h);

  void reset_orbit();
  void apply_orbit_world_extent(const content::Extent2& e);
  void apply_orbit_pan(int dx_px, int dy_px);
  void apply_orbit_pinch(int view_x, int view_y, double scale, int view_w,
                         int view_h);
  void set_orbit_extent(const content::Extent2& e);
  void set_orbit_distance(float distance);
  void set_orbit_pitch(float pitch);
  void set_orbit_yaw(float yaw);
  content::Extent2 orbit_world_extent() const;
  content::Extent2 orbit_extent() const;
  float orbit_yaw() const;
  float orbit_pitch() const;
  float orbit_distance() const;
  void apply_china_scene3d_orbit();
  void pull_orbit_extent_from_document();
  content::Extent2 orbit_extent_for_scene_tab();
  content::Extent2 adopt_orbit_from_view(int view_w, int view_h);

  bool navigation_commit(const content::Extent2& extent);
  void navigation_reset(const content::Extent2& extent);
  bool navigation_previous();
  bool navigation_next();
  bool navigation_zoom_layer(const content::Extent2* target);
  bool navigation_zoom_selection(const content::Extent2* target);
  void navigation_note_no_feature();
  void navigation_add_bookmark();
  bool navigation_go_bookmark(std::size_t index);
  const std::string& navigation_status() const;
  const content::Extent2& navigation_extent() const;
  const std::vector<content::ViewBookmark>& navigation_bookmarks() const;

  void blit_begin_pan(int view_w, int view_h, int dx_px, int dy_px);
  void blit_begin_zoom(int view_w, int view_h, int cursor_x, int cursor_y,
                       double factor);
  void blit_end_preview();
  bool blit_in_preview() const;
  bool blit_present(HDC dst, int view_w, int view_h) const;
  void blit_capture(HDC src, int w, int h);

  void invalidate_map2d_frame_cache();
  void note_map2d_surface_reset();
  void map2d_paint(HDC hdc, int width_px, int height_px,
                   bool fill_background) const;
  void map2d_paint_annotation(HDC hdc, int width_px, int height_px) const;
  void map2d_paint_flash(HDC hdc, int width_px, int height_px) const;
  bool map2d_export_bmp(const std::string& path, int width_px,
                        int height_px) const;
  bool map2d_present_gpu(render::rhi::Device* device, std::uint32_t width_px,
                         std::uint32_t height_px,
                         const ui::gfx::ShellRaster* shell = nullptr,
                         std::uint64_t shell_generation = 0);
  bool map2d_last_gpu_present_ok() const;
  bool map2d_last_gpu_present_drew() const;
  std::uint64_t map2d_layout_build_count() const;
  bool map2d_last_present_reused_layout() const;

  void bind_presenters();
  void bind_scene3d_document();
  void bind_scene3d_contents(GisContents* contents, std::uint32_t view_id);
  void scene3d_apply_draft(const tool::Draft& draft);
  void scene3d_paint(HDC hdc, int width_px, int height_px,
                     bool fill_background) const;
  void scene3d_paint_hud(HDC hdc, int width_px, int height_px) const;
  void scene3d_set_render_engine_name(const char* name);
  bool scene3d_present_gpu(render::rhi::Device* device, std::uint32_t width_px,
                           std::uint32_t height_px,
                           const ui::gfx::ShellRaster* shell = nullptr,
                           std::uint64_t shell_generation = 0);
  void abandon_scene3d_mesh();
  void select_scene3d_flycube(const char* hud_label);
  void select_scene3d_stereo_gl(const char* hud_label);
  void select_scene3d_gdi(const char* hud_label);
  void set_scene3d_look_atmosphere();
  void set_scene3d_look_legacy();
  void seed_scene3d_procedural(bool with_land_rings);
  void seed_scene3d_procedural_if_empty();
  void set_scene3d_atmosphere_layers(bool ocean, bool cloud, bool sky, bool fog);
  void set_scene3d_globe_enabled(bool on);
  void set_scene3d_cloud_enabled(bool on);
  void set_scene3d_wind_overlay(bool on);
  bool apply_scene3d_contour_suite_or_elevation();
  bool ensure_scene3d_legacy_overlays();
  bool load_scene3d_fields(std::string_view spec);
  void set_scene3d_time_sec(double t);
  double scene3d_time_sec() const;
  bool scene3d_time_range(double* t_min, double* t_max) const;
  void set_scene3d_tileset_content_root(const std::string& root);
  bool attach_scene3d_tileset_json(const char* json, std::size_t len,
                                   const char* name);
  void set_scene3d_overlay_tin_mesh(const float* xyz, int point_count,
                                    const unsigned* indices, int index_count,
                                    const std::uint8_t* albedo);
  void set_scene3d_overlay_tin_drape(const std::uint8_t* rgba,
                                     std::uint32_t width, std::uint32_t height,
                                     const float* uv, int uv_float_count);
  void clear_scene3d_overlay_tin();

  void release_scene3d_stereo();
  void abandon_scene3d_stereo();
  bool try_attach_scene3d_stereo(HWND hwnd);
  bool try_present_scene3d_stereo(HWND hwnd, HDC hdc, int width_px,
                                  int height_px);

  void attach_gestures(GisHwndGestures* gestures, HWND hwnd,
                       GisHwndGestures::PinchFn on_pinch,
                       GisHwndGestures::PanFn on_pan);
  void configure_gestures(GisHwndGestures* gestures,
                          GisHwndGestures::RightClickFn on_right_click,
                          GisHwndGestures::ExtentWatchFn on_extent_watch,
                          GisHwndGestures::ResizeFn on_resized);

 private:
  // Hosts / GisContents first. Map2d/Scene3d presenters are heap Ptrs created
  // in their own TUs so a stale sizeof cannot overflow this object into the
  // CRT heap (0xC0000374 during ToolSession / Workspace::register_builtins).
  std::unique_ptr<ToolSession> edit_tool_session_;
  std::unique_ptr<ToolSession> data_tool_session_;
  std::unique_ptr<ToolSession> scene_tool_session_;
  std::unique_ptr<GisContents> gis_contents_;
  bool prepare_close_done_ = false;

  GisScene document_;
  ViewFrame view_frame_;
  OrbitFrame orbit_;
  Map2dPresenter::Ptr map2d_;
  Scene3dPresenter::Ptr scene3d_;
  Scene3dStereoSession scene3d_stereo_;
  BlitFrameCache blit_;
  ViewNavigation navigation_;
  GisHwndGestures edit_gestures_;
  GisHwndGestures data_gestures_;
  GisHwndGestures scene_gestures_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SESSION_BROWSER_SESSION_H_
