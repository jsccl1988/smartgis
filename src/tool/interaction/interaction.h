// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_INTERACTION_H_
#define TOOL_INTERACTION_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "content/public/map_types.h"
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

class SMT_TOOL_EXPORT Interaction {
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

class SMT_TOOL_EXPORT InteractionRegistry {
 public:
  bool add(std::string_view id, InteractionFactory factory);
  std::unique_ptr<Interaction> make(std::string_view id) const;

 private:
  std::map<std::string, InteractionFactory> factories_;
};

class SMT_TOOL_EXPORT InteractionStack {
 public:
  InteractionStack() = default;
  InteractionStack(const InteractionStack&) = delete;
  InteractionStack& operator=(const InteractionStack&) = delete;

  Interaction* current() const;
  bool activate(std::string_view id, const InteractionRegistry& registry);
  bool push(std::string_view id, const InteractionRegistry& registry);
  bool pop();

 private:
  std::vector<std::unique_ptr<Interaction>> stack_;
};

class SMT_TOOL_EXPORT InputRouter {
 public:
  InputRouter() = default;
  InputRouter(const InputRouter&) = delete;
  InputRouter& operator=(const InputRouter&) = delete;

  void add_always_on(std::unique_ptr<Interaction> handler);
  void set_stack(InteractionStack* stack);
  bool dispatch(const content::InputEvent& e);

 private:
  std::vector<std::unique_ptr<Interaction>> always_on_;
  InteractionStack* stack_ = nullptr;
};

SMT_TOOL_EXPORT std::unique_ptr<Interaction> make_wheel_zoom();
SMT_TOOL_EXPORT std::unique_ptr<Interaction> make_hover_cursor();

}  // namespace tool

#endif  // TOOL_INTERACTION_H_
