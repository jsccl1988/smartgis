// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_PLUGIN_HOST_H_
#define CONTENT_PUBLIC_PLUGIN_HOST_H_

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "content/content_export.h"
#include "content/public/event_bus.h"
#include "content/public/gis_document.h"
#include "tool/command/command.h"

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
// Shell export_bmp(frame=<id>) looks this up; horizon must not hardcode product
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

// QgsInterface analogue: contribution points plus opaque capabilities.
// Product plugin names and package-specific facades must not appear here —
// chrome / plugin_host.dll register interfaces by reverse-DNS id.
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

  virtual void set_processing_enqueue(ProcessingEnqueue fn) = 0;

  // Commands still owned after contribute_command. Withdrawn ids are omitted.
  // |title| is the string passed to contribute_command (may be empty).
  virtual void for_each_command(
      const std::function<void(std::string_view plugin_id,
                               std::string_view command_id,
                               std::string_view title)>& fn) const {
    (void)fn;
  }

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
  // Invokes the DialogFactory registered with contribute_dock.
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

  // GIS document wrapping MapScene (not MapContents). Shell installs a
  // MapSceneGisDocument; default is no-op.
  virtual GisDocument* gis_document() { return nullptr; }
  virtual void set_gis_document(GisDocument* doc) { (void)doc; }

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

  // Named export frames for harness export_bmp.
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
  virtual void set_present_surface(int surface) { (void)surface; }
  virtual int present_surface() const { return 0; }
  // Per-call surface; does not change the sticky default.
  virtual bool present_dataset(std::string_view plugin_id,
                               std::string_view path, int face, int surface) {
    (void)surface;
    return present_dataset(plugin_id, path, face);
  }

  // Opaque capability table. |capability_id| is reverse-DNS owned by the
  // registrant (chrome or plugin_host.dll). |iface| nullptr unregisters.
  // Content never interprets the pointer or the id string.
  virtual bool set_capability(std::string_view capability_id, void* iface) {
    (void)capability_id;
    (void)iface;
    return false;
  }
  virtual void* query_capability(std::string_view capability_id) const {
    (void)capability_id;
    return nullptr;
  }
};

CONTENT_EXPORT PluginHost* create_plugin_host(tool::CommandCatalog* catalog,
                                              EventBus* events,
                                              MapContents* maps);

}  // namespace content

#endif  // CONTENT_PUBLIC_PLUGIN_HOST_H_
