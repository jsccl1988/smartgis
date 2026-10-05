// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_PLUGIN_HOST_H_
#define CONTENT_PUBLIC_PLUGIN_HOST_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "content/content_export.h"
#include "content/public/event_bus.h"
#include "content/public/gis_document.h"
#include "content/public/map_types.h"
#include "tool/command/command.h"

namespace plugin {
class ProcessingPool;
}

namespace content {

struct MenuContribution {
  std::string id;
  std::string title;
  std::string parent;  // "tools" / "file" / "view" / another menu id
};

struct DockContribution {
  std::string id;
  std::string title;
  std::string area;  // left | right | bottom | float
};

struct DialogContribution {
  std::string id;
  std::string title;
};

struct ProcessingContribution {
  std::string id;
  std::string title;
};

// Named Map2d export framing (lon/lat Extent2) contributed by a product plugin.
// Shell export_bmp(frame=<id>) looks this up; chrome must not hardcode product
// extents.
struct ExportFrameContribution {
  std::string id;
  double min_lon = 0.0;
  double min_lat = 0.0;
  double max_lon = 0.0;
  double max_lat = 0.0;
};

// Real MapContents lives in content/public/map_contents.h. Keep a forward
// declaration here so PluginHost TUs that must not include that header (and
// TUs that already include it) do not hit C2011 redefinition.
class MapContents;

class PluginHost;

using DialogFactory = std::function<void(PluginHost*)>;
using ProcessingFactory =
    std::function<bool(PluginHost*, std::string_view args_json)>;
// Header-only seam so content does not GN-dep plugin::ProcessingPool.
using ProcessingEnqueue = std::function<bool(
    std::string processing_id, std::string args_json, ProcessingFactory factory)>;
// Content must not #include ui/views. App shell installs painters via this
// callback (typically PainterRegistry::register_painter_for_plugin).
using UiPainterInstaller = std::function<void()>;
// App shell sets this so withdraw can clear Views painters without a content
// → ui/views include edge.
using UiWithdrawHook = std::function<void(std::string_view plugin_id)>;

class CONTENT_EXPORT PluginHost {
 public:
  virtual ~PluginHost() = default;

  virtual MapContents* map_contents() = 0;
  virtual EventBus* events() = 0;
  virtual tool::CommandCatalog* commands() = 0;

  virtual bool contribute_command(std::string_view plugin_id,
                                  std::string_view command_id,
                                  std::string_view title,
                                  std::string_view menu_id,
                                  tool::CommandHandler handler) = 0;
  virtual bool contribute_menu(std::string_view plugin_id,
                               const MenuContribution& menu) = 0;
  virtual bool contribute_dock(std::string_view plugin_id,
                               const DockContribution& dock,
                               DialogFactory factory) = 0;
  virtual bool contribute_dialog(std::string_view plugin_id,
                                 const DialogContribution& dialog,
                                 DialogFactory factory) = 0;
  virtual bool contribute_processing(std::string_view plugin_id,
                                     const ProcessingContribution& proc,
                                     ProcessingFactory factory) = 0;
  // Runs |install| when args are valid and records ownership for withdraw.
  virtual bool contribute_painter(std::string_view plugin_id,
                                  std::string_view role,
                                  UiPainterInstaller install) = 0;

  virtual bool execute(std::string_view command_id,
                       const tool::CommandArgs& args) = 0;
  virtual bool open_dialog(std::string_view dialog_id) = 0;
  virtual bool run_processing(std::string_view processing_id,
                              std::string_view args_json) = 0;

  virtual void withdraw(std::string_view plugin_id) = 0;
  virtual void set_ui_withdraw_hook(UiWithdrawHook hook) = 0;

  virtual plugin::ProcessingPool* processing_pool() = 0;
  virtual void set_processing_pool(plugin::ProcessingPool* pool) = 0;
  virtual void set_processing_enqueue(ProcessingEnqueue fn) = 0;

  // Commands still owned after contribute_command. Withdrawn ids are omitted.
  // |title| is the string passed to contribute_command (may be empty).
  // Appended after processing_* so cross-DLL PluginHost subclasses keep
  // withdraw / pool slots stable — never insert above.
  virtual void for_each_command(
      const std::function<void(std::string_view plugin_id,
                               std::string_view command_id,
                               std::string_view title)>& fn) const {
    (void)fn;
  }

  // Local HTML report browser (shell installs ReportPanel bridge). Append-only.
  using ReportOpenFn = std::function<bool(std::string_view report_dir)>;
  using ReportPostFn = std::function<bool(std::string_view json)>;
  using ReportCloseFn = std::function<void()>;
  virtual void set_report_bridge(ReportOpenFn open,
                                 ReportPostFn post,
                                 ReportCloseFn close) {
    (void)open;
    (void)post;
    (void)close;
  }
  virtual bool open_report(std::string_view report_dir) {
    (void)report_dir;
    return false;
  }
  virtual bool post_to_report(std::string_view json) {
    (void)json;
    return false;
  }
  virtual void close_report() {}

  // Processing / dialog / dock still owned after contribute_*. Appended after
  // report_* so cross-DLL PluginHost subclasses keep those slots stable.
  virtual void for_each_processing(
      const std::function<void(std::string_view plugin_id,
                               std::string_view processing_id,
                               std::string_view title)>& fn) const {
    (void)fn;
  }
  virtual void for_each_dialog(
      const std::function<void(std::string_view plugin_id,
                               std::string_view dialog_id,
                               std::string_view title)>& fn) const {
    (void)fn;
  }
  virtual void for_each_dock(
      const std::function<void(std::string_view plugin_id,
                               std::string_view dock_id,
                               std::string_view title)>& fn) const {
    (void)fn;
  }
  // Invokes the DialogFactory registered with contribute_dock. Append-only.
  virtual bool open_dock(std::string_view dock_id) {
    (void)dock_id;
    return false;
  }

  // Present surface for dataset results. Face selects Map vs Scene3D content;
  // surface selects where it paints.
  //   face 0 + surface 0 → main Map tab
  //   face 1 + surface 0 → main Scene3D tab
  //   face 0 + surface 1 → shared MapPreviewView window
  //   face 1 + surface 1 → shared WorldPreviewView window
  // |path| is optional vector sample (GeoJSON / GPKG / SHP); empty keeps the
  // current document and only switches the surface.
  using PresentDatasetFn = std::function<bool(std::string_view plugin_id,
                                              std::string_view path,
                                              int face,
                                              int surface)>;
  virtual void set_present_dataset_bridge(PresentDatasetFn fn) { (void)fn; }
  virtual bool present_dataset(std::string_view plugin_id,
                               std::string_view path, int face) {
    (void)plugin_id;
    (void)path;
    (void)face;
    return false;
  }

  // GIS document wrapping MapScene (not MapContents). Append-only after
  // present_dataset. Shell installs a MapSceneGisDocument; default is no-op.
  virtual GisDocument* gis_document() { return nullptr; }
  virtual void set_gis_document(GisDocument* doc) { (void)doc; }

  // Scene3D facade. Shell installs callbacks; product TUs never see Browser*.
  // Out-of-line members are CONTENT_EXPORT individually — marking the nested
  // class itself dllexport makes MSVC emit those symbols into every consumer
  // .obj (LNK2005 vs content_d.dll.lib).
  class Scene3dSink {
   public:
    using AddStandinMeshFn = std::function<bool(
        std::string_view name, double lon, double lat, double half_deg)>;
    using AttachTilesetFn = std::function<bool(std::string_view uri)>;
    using InvalidateFn = std::function<void()>;
    using OpenEarthFn = std::function<bool()>;
    using FlyToFn = std::function<bool(double lon, double lat, float distance,
                                       double span_deg)>;
    using LoadGlobalDemFn =
        std::function<bool(std::string_view path, std::string* result_json)>;
    using SetSatelliteCloudFn = std::function<bool(std::string_view path,
                                                   bool enabled,
                                                   std::string* result_json)>;
    using SetAtmosphereFn =
        std::function<bool(bool sky, bool ocean, bool cloud, bool fog)>;
    using SetOverlayTinMeshFn = std::function<bool(
        const float* xyz_lon_lat_elev, int point_count, const unsigned* indices,
        int index_count, const uint8_t* albedo_rgba)>;
    using SetOverlayTinDrapeFn = std::function<bool(
        const uint8_t* rgba, uint32_t width, uint32_t height, const float* uv,
        int uv_float_count)>;
    using ClearOverlayTinFn = std::function<void()>;

    CONTENT_EXPORT void set_bridges(AddStandinMeshFn mesh,
                                    AttachTilesetFn tileset,
                                    InvalidateFn invalidate);
    // Out-of-line in content.dll: assigning std::function into a Scene3dSink
    // that lives in content_d.dll from an inline SmartGIS method AVs when the
    // DLL/exe header views of the member layout diverge (0xCDCDCDCD _Tidy).
    CONTENT_EXPORT void set_overlay_bridges(SetOverlayTinMeshFn mesh,
                                            SetOverlayTinDrapeFn drape,
                                            ClearOverlayTinFn clear);
    CONTENT_EXPORT void set_earth_bridges(
        OpenEarthFn open_earth, FlyToFn fly_to, LoadGlobalDemFn load_global_dem,
        SetSatelliteCloudFn set_satellite_cloud,
        SetAtmosphereFn set_atmosphere);

    bool earth_bridges_installed() const { return earth_bridges_installed_; }

    bool add_standin_mesh(std::string_view name, double lon, double lat,
                          double half_deg) const {
      return add_mesh_ ? add_mesh_(name, lon, lat, half_deg) : false;
    }
    bool attach_tileset(std::string_view uri) const {
      return attach_tileset_ ? attach_tileset_(uri) : false;
    }
    CONTENT_EXPORT void invalidate() const;

    bool open_earth() const {
      return open_earth_ ? open_earth_() : false;
    }
    bool fly_to(double lon, double lat, float distance,
                double span_deg) const {
      return fly_to_ ? fly_to_(lon, lat, distance, span_deg) : false;
    }
    bool load_global_dem(std::string_view path,
                         std::string* result_json) const {
      return load_global_dem_ ? load_global_dem_(path, result_json) : false;
    }
    bool set_satellite_cloud(std::string_view path, bool enabled,
                             std::string* result_json) const {
      return set_satellite_cloud_
                 ? set_satellite_cloud_(path, enabled, result_json)
                 : false;
    }
    bool set_atmosphere(bool sky, bool ocean, bool cloud, bool fog) const {
      return set_atmosphere_ ? set_atmosphere_(sky, ocean, cloud, fog) : false;
    }

    CONTENT_EXPORT bool set_overlay_tin_mesh(
        const float* xyz_lon_lat_elev, int point_count,
        const unsigned* indices, int index_count,
        const uint8_t* albedo_rgba) const;
    CONTENT_EXPORT bool set_overlay_tin_drape(const uint8_t* rgba,
                                              uint32_t width, uint32_t height,
                                              const float* uv,
                                              int uv_float_count) const;
    CONTENT_EXPORT void clear_overlay_tin_mesh() const;

   private:
    AddStandinMeshFn add_mesh_;
    AttachTilesetFn attach_tileset_;
    InvalidateFn invalidate_;
    OpenEarthFn open_earth_;
    FlyToFn fly_to_;
    LoadGlobalDemFn load_global_dem_;
    SetSatelliteCloudFn set_satellite_cloud_;
    SetAtmosphereFn set_atmosphere_;
    // Fn suffix: avoid MSVC lookup colliding with method names above.
    SetOverlayTinMeshFn overlay_tin_mesh_fn_;
    SetOverlayTinDrapeFn overlay_tin_drape_fn_;
    ClearOverlayTinFn clear_overlay_tin_fn_;
    bool earth_bridges_installed_ = false;
  };

  virtual Scene3dSink* scene3d_sink() { return nullptr; }

  // Product-agnostic playback frames. ResultPlayback UI only ticks index.
  class Playback {
   public:
    void push_frame(std::string json) { frames_.push_back(std::move(json)); }
    size_t frame_count() const { return frames_.size(); }
    void set_index(size_t i) {
      if (frames_.empty()) {
        index_ = 0;
        return;
      }
      index_ = i < frames_.size() ? i : frames_.size() - 1;
    }
    size_t index() const { return index_; }
    std::string_view frame_json(size_t i) const {
      if (i >= frames_.size()) {
        return {};
      }
      return frames_[i];
    }
    void clear() {
      frames_.clear();
      index_ = 0;
    }

   private:
    std::vector<std::string> frames_;
    size_t index_ = 0;
  };

  virtual Playback* playback() { return nullptr; }

  // Named export frames for harness export_bmp. Append-only after playback().
  virtual bool contribute_export_frame(std::string_view plugin_id,
                                       const ExportFrameContribution& frame) {
    (void)plugin_id;
    (void)frame;
    return false;
  }
  virtual bool lookup_export_frame(std::string_view frame_id,
                                   double* min_lon,
                                   double* min_lat,
                                   double* max_lon,
                                   double* max_lat) const {
    (void)frame_id;
    (void)min_lon;
    (void)min_lat;
    (void)max_lon;
    (void)max_lat;
    return false;
  }

  // Sticky present surface for 3-arg present_dataset (0=main, 1=preview).
  // Append-only after lookup_export_frame.
  virtual void set_present_surface(int surface) { (void)surface; }
  virtual int present_surface() const { return 0; }
  // Per-call surface; does not change the sticky default.
  virtual bool present_dataset(std::string_view plugin_id,
                               std::string_view path, int face, int surface) {
    (void)surface;
    return present_dataset(plugin_id, path, face);
  }
};

CONTENT_EXPORT PluginHost* create_plugin_host(tool::CommandCatalog* catalog,
                                              EventBus* events,
                                              MapContents* maps);

}  // namespace content

#endif  // CONTENT_PUBLIC_PLUGIN_HOST_H_
