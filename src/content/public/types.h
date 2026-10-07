// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_TYPES_H_
#define CONTENT_PUBLIC_TYPES_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "gis/feature/attrs.h"

// Host ABI value types for shell (Views). Shell must not include gis_map.h or
// rd_renderdevice.h. Catalog LayerDesc and map view / input / extent types
// share this header so embedders include one public set. FeatureId /
// NamedField / token helpers live in gis/feature/attrs.h and are re-exported
// here as content::* for existing callers.
//
// Do not include content_export.h here: tool/gpu TUs use the POD subset without
// linking content.dll. Export catalog helpers only when compiling the content
// DLL (CONTENT_EXPORTS).
#if defined(CONTENT_EXPORTS)
#define CONTENT_TYPES_EXPORT __declspec(dllexport)
#else
#define CONTENT_TYPES_EXPORT
#endif

namespace content {

using FeatureId = gis::FeatureId;
using NamedField = gis::NamedField;
using gis::encode_feature_token;
using gis::decode_feature_token;
using gis::apply_named_field;

enum class ViewKind { kMapEdit, kMapData, kScene3d };

enum class PresentMode { kSharedTexture, kChildHwnd, kSoftwareDib };

struct Extent2 {
  double xmin;
  double ymin;
  double xmax;
  double ymax;
};

// Bits for InputEvent::flags beyond Win32 MK_* (high range).
namespace input_flags {
// WM_MOUSEHWHEEL / trackpad two-finger horizontal → pan (not zoom).
constexpr uint32_t kHorizontalWheel = 0x02000000u;
}  // namespace input_flags

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
  // Active contacts for this sample. 0/1 = mouse or single finger; >=2 =
  // multitouch with (x_px, y_px) at the contact midpoint (or host average).
  uint32_t pointer_count;
};

inline bool is_multitouch(const InputEvent& e) {
  return e.pointer_count >= 2;
}

inline bool is_horizontal_wheel(const InputEvent& e) {
  return e.kind == InputEvent::Kind::kWheel &&
         (e.flags & input_flags::kHorizontalWheel) != 0;
}

// Latest shared pixels. nt_handle is valid in the shell process
// (DuplicateHandle from gpu). After Resize, ignore until FrameReady.
// Try ID3D11Device::OpenSharedResource1 first; if that fails, MapViewOfFile
// (software DIB, DXGI_FORMAT_B8G8R8A8_UNORM tightly packed).
struct SharedSurface {
  uint32_t generation;
  void* nt_handle;
  uint32_t width_px;
  uint32_t height_px;
  uint32_t format;
};

// Catalog / LayerTree node kind. Hosts may leave kUnknown for flat mirrors.
enum class LayerKind {
  kUnknown = 0,
  kGroup,
  kVector,
  kRaster,
};

// Opaque Catalog / LayerTree row for shell mirrors. No GIS pointers, no HWND.
// Nested |children| are optional; empty children keep flat-list wire callers.
struct LayerDesc {
  std::string id;
  std::string name;
  bool visible = true;
  bool active = false;
  LayerKind kind = LayerKind::kUnknown;
  bool expanded = true;
  std::vector<LayerDesc> children;
};

// Escape a string for embedding inside a JSON double-quoted value.
CONTENT_TYPES_EXPORT std::string json_escape_string(std::string_view text);

// Serialize |layers| to a JSON array consumed by CEF CatalogDelta / LegendSnapshot:
// [{"id":"...","name":"...","visible":true}, ...]
// |active|, |kind|, |expanded|, and |children| are intentionally omitted to
// match the existing CEF wire format (flat id/name/visible only).
CONTENT_TYPES_EXPORT std::string layers_to_catalog_json(
    const std::vector<LayerDesc>& layers);

}  // namespace content

#endif  // CONTENT_PUBLIC_TYPES_H_
