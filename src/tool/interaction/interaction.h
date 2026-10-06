// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_INTERACTION_H_
#define TOOL_INTERACTION_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "content/public/map_layer_types.h"
#include "tool/tool_export.h"

// Exclusive map gesture. Yields a draft; does not write the document.
namespace tool {

// Pixel-space rubber-band. Paint stays leftover; this type has no HWND.
struct AuxPoint {
  int32_t x_px = 0;
  int32_t y_px = 0;
};

struct AuxOverlay {
  enum class Kind { kNone, kRect, kPolyline };
  Kind kind = Kind::kNone;
  std::vector<AuxPoint> points;
};

// Class-exported for vtable; no STL data members.
class TOOL_EXPORT Interaction {
 public:
  virtual ~Interaction() = default;
  virtual const char* id() const = 0;
  virtual void activate() {}
  virtual void deactivate() {}
  virtual bool on_input(const content::InputEvent& e) = 0;
  // Refresh live rubber-band geometry. Does not paint.
  virtual void aux_draw() {}
  virtual const AuxOverlay* aux_overlay() const { return nullptr; }
};

using InteractionFactory = std::function<std::unique_ptr<Interaction>()>;

// Methods exported; class not — avoids C4251 on pimpl.
class InteractionRegistry {
 public:
  TOOL_EXPORT InteractionRegistry();
  TOOL_EXPORT ~InteractionRegistry();

  InteractionRegistry(const InteractionRegistry&) = delete;
  InteractionRegistry& operator=(const InteractionRegistry&) = delete;

  TOOL_EXPORT bool add(std::string_view id, InteractionFactory factory);
  TOOL_EXPORT std::unique_ptr<Interaction> make(std::string_view id) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

class InteractionStack {
 public:
  TOOL_EXPORT InteractionStack();
  TOOL_EXPORT ~InteractionStack();

  InteractionStack(const InteractionStack&) = delete;
  InteractionStack& operator=(const InteractionStack&) = delete;

  TOOL_EXPORT Interaction* current() const;
  TOOL_EXPORT bool activate(std::string_view id,
                            const InteractionRegistry& registry);
  TOOL_EXPORT bool push(std::string_view id,
                        const InteractionRegistry& registry);
  TOOL_EXPORT bool pop();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

class InputRouter {
 public:
  TOOL_EXPORT InputRouter();
  TOOL_EXPORT ~InputRouter();

  InputRouter(const InputRouter&) = delete;
  InputRouter& operator=(const InputRouter&) = delete;

  TOOL_EXPORT void add_always_on(std::unique_ptr<Interaction> handler);
  TOOL_EXPORT void set_stack(InteractionStack* stack);
  TOOL_EXPORT bool dispatch(const content::InputEvent& e);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

TOOL_EXPORT std::unique_ptr<Interaction> make_wheel_zoom();
TOOL_EXPORT std::unique_ptr<Interaction> make_hover_cursor();

}  // namespace tool

#endif  // TOOL_INTERACTION_H_
