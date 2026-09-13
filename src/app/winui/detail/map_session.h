// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_WINUI_DETAIL_MAP_SESSION_H_
#define APP_WINUI_DETAIL_MAP_SESSION_H_

// Prefer src/content/public (mgis-style content::MapSession / MapView).
// This shim exists so chrome can still compile if that tree is absent.

#if defined(__has_include)
#if __has_include("content/public/map_contents.h")
#include "content/public/map_contents.h"
#define SMT_WINUI_HAS_CONTENT_PUBLIC 1
#endif
#endif

#ifndef SMT_WINUI_HAS_CONTENT_PUBLIC

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#include <windows.h>
#else
typedef void* HWND;
#endif

namespace content {

enum class ViewKind { kMapEdit, kMapData, kScene3d };
enum class PresentMode { kSharedTexture, kChildHwnd, kSoftwareDib };

struct Extent2 {
  double xmin;
  double ymin;
  double xmax;
  double ymax;
};

struct InputEvent {
  enum class Kind {
    kMouseMove,
    kWheel,
    kLDown,
    kLUp,
    kLDClick,
    kRDown,
    kRUp,
    kRDClick,
    kKeyDown,
    kTextCommit
  };
  Kind kind;
  uint32_t flags;
  int32_t x_px;
  int32_t y_px;
  int32_t wheel;
  uint32_t key;
  char32_t text[8];
  uint64_t t_qpc;
};

struct FeatureId {
  uint8_t bytes[32];
  uint8_t len;
};

struct SharedSurface {
  uint32_t generation;
  void* nt_handle;
  uint32_t width_px;
  uint32_t height_px;
  uint32_t format;
};

class MapView {
 public:
  struct CreateParams {
    void* parent_hwnd;
    CreateParams() : parent_hwnd(nullptr) {}
  };

  struct Preferences {};

  virtual ~MapView() = default;

  virtual void Create(const CreateParams& params,
                      const Preferences& preferences) = 0;
  virtual void Destroy() = 0;

  virtual uint32_t view_id() const = 0;
  virtual void resize(int width_px, int height_px, float dpi) = 0;
  virtual void set_present_mode(PresentMode mode) = 0;
  virtual void set_visible(bool visible) = 0;
  virtual SharedSurface latest() const = 0;
};

class MapSession {
 public:
  virtual ~MapSession() = default;
  virtual bool start_render_process() = 0;
  virtual void shutdown() = 0;
  virtual bool is_oop_render() const = 0;
  virtual uint32_t open_view(ViewKind kind) = 0;
  virtual void close_view(uint32_t view_id) = 0;
  virtual MapView* attach_surface(uint32_t view_id, PresentMode mode) = 0;
  virtual MapView* map_view(uint32_t view_id) = 0;
  virtual void set_extent(uint32_t view_id, const Extent2& e) = 0;
  virtual Extent2 extent(uint32_t view_id) const = 0;
  virtual void set_selection(uint32_t view_id,
                             const FeatureId* ids,
                             size_t n) = 0;
  virtual void legend_snapshot(uint32_t view_id) = 0;
  virtual void catalog_call(const char* json_op) = 0;
  virtual void activate_tool(uint32_t view_id, const char* tool_id) = 0;
  virtual void dispatch(uint32_t view_id, const InputEvent& e) = 0;
};

MapSession* create_map_session();

}  // namespace content

#endif  // SMT_WINUI_HAS_CONTENT_PUBLIC

#endif  // APP_WINUI_DETAIL_MAP_SESSION_H_
