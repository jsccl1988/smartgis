// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_GIS_CONTENTS_H
#define CONTENT_PUBLIC_GIS_CONTENTS_H

#include <cstddef>
#include <cstdint>
#include <memory>

#include "content/content_export.h"
#include "content/public/types.h"
#include "content/public/widget_host_view.h"

namespace tool {
class CommandCatalog;
}  // namespace tool

// Core embedder capability host for one GIS document + viewport session
// (views, present pipe, owned PluginHost + GisDocument). Distinct from
// GisDocument (narrow layer/feature/catalog/selection API) and from
// GisContentsClient (content->app process hooks). Document lifecycle
// notifications also mirror EventBus domain facts for shells that prefer
// virtual callbacks over subscribe().
namespace content {

class EventBus;
class GisDocument;
class PluginHost;

// Shell implements this. GisContents does not include mojo.
// Frame / extent / death come from the present pipe; selection / layers /
// style / edit / backend mirror EventBus (and document/pipe mutators).
class GisContentsObserver {
 public:
  virtual ~GisContentsObserver() = default;

  virtual void OnFrameReady(uint32_t view_id, uint32_t generation) {}
  virtual void OnExtentChanged(uint32_t view_id, const Extent2& e) {}
  virtual void OnSelectionChanged(uint32_t view_id,
                                  const FeatureId* ids,
                                  size_t n) {}
  virtual void OnLayersChanged(uint32_t view_id, uint32_t layer_count) {}
  // Fired after a successful style apply (JSON or path load via document).
  virtual void OnStyleChanged(uint32_t view_id) {}
  // |op| matches EditCommitted::Op (0=append, 1=delete, 2=modify).
  virtual void OnEditCommitted(uint32_t view_id,
                               const FeatureId& id,
                               int op) {}
  virtual void OnRenderBackendChanged(uint32_t view_id, uint32_t kind) {}
  virtual void OnRenderDied() {}
};

// Capability root: viewport pipe + owned PluginHost + GisDocument.
// PluginHost does not own this GisContents (non-owning back-pointer only).
// Selection / catalog / legend / plugin dispatch / tool activate live on
// GisDocument, PluginHost, or ToolSession — not on this surface.
class CONTENT_EXPORT GisContents {
 public:
  virtual ~GisContents() = default;

  virtual bool StartRenderProcess() = 0;
  virtual void Shutdown() = 0;
  virtual bool IsOopRender() const = 0;
  virtual const wchar_t* PresentStatus() const = 0;

  virtual uint32_t OpenView(ViewKind kind) = 0;
  virtual void CloseView(uint32_t view_id) = 0;
  virtual WidgetHostView* AttachSurface(uint32_t view_id,
                                        PresentMode mode) = 0;
  virtual WidgetHostView* HostView(uint32_t view_id) = 0;

  virtual void SetExtent(uint32_t view_id, const Extent2& e) = 0;
  virtual Extent2 Extent(uint32_t view_id) const = 0;

  virtual void Dispatch(uint32_t view_id, const InputEvent& e) = 0;

  // Tell --type=gpu which 2D paint path to use. 0 = Track B RHI, 1 = Track A
  // MapLibre. Hot-swap; both tracks paint into PresentTarget (no GL/DX restart).
  virtual void SetRenderBackend(uint32_t kind) = 0;
  virtual uint32_t RenderBackend() const = 0;

  virtual void SetObserver(GisContentsObserver* observer) = 0;
  virtual bool WaitFrameReady(uint32_t view_id, uint32_t timeout_ms) = 0;

  // GIS document capability (layers / features / style / catalog / selection).
  // Non-owning set for tests; take_ transfers ownership to this GisContents.
  virtual GisDocument* gis_document() = 0;
  virtual void set_gis_document(GisDocument* doc) = 0;
  virtual void take_gis_document(std::unique_ptr<GisDocument> doc) = 0;

  // Plugin contribution host. Owned by this GisContents; created on first
  // ensure_plugin_host. Idempotent — same pointer thereafter.
  virtual PluginHost* plugin_host() = 0;
  virtual PluginHost* ensure_plugin_host(tool::CommandCatalog* catalog,
                                         EventBus* events) = 0;
};

// Factory (snake_case). Caller owns the returned pointer.
CONTENT_EXPORT GisContents* create_gis_contents();

}  // namespace content

#endif  // CONTENT_PUBLIC_GIS_CONTENTS_H
