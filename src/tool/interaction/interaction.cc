// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/interaction/interaction.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tool {
namespace {

class WheelZoom final : public Interaction {
 public:
  const char* id() const override { return "wheel.zoom"; }
  bool on_input(const content::InputEvent& e) override {
    return e.kind == content::InputEvent::Kind::kWheel;
  }
};

class HoverCursor final : public Interaction {
 public:
  const char* id() const override { return "hover.cursor"; }
  bool on_input(const content::InputEvent& e) override {
    (void)e;
    return false;
  }
};

}  // namespace

struct InteractionRegistry::Impl {
  std::map<std::string, InteractionFactory> factories;
};

InteractionRegistry::InteractionRegistry() : impl_(std::make_unique<Impl>()) {}

InteractionRegistry::~InteractionRegistry() = default;

bool InteractionRegistry::add(std::string_view id, InteractionFactory factory) {
  if (!impl_ || id.empty() || !factory) {
    return false;
  }
  std::string key(id);
  if (impl_->factories.find(key) != impl_->factories.end()) {
    return false;
  }
  impl_->factories.emplace(std::move(key), std::move(factory));
  return true;
}

std::unique_ptr<Interaction> InteractionRegistry::make(
    std::string_view id) const {
  if (!impl_) {
    return nullptr;
  }
  auto it = impl_->factories.find(std::string(id));
  if (it == impl_->factories.end()) {
    return nullptr;
  }
  return it->second();
}

struct InteractionStack::Impl {
  std::vector<std::unique_ptr<Interaction>> stack;
};

InteractionStack::InteractionStack() : impl_(std::make_unique<Impl>()) {}

InteractionStack::~InteractionStack() = default;

Interaction* InteractionStack::current() const {
  if (!impl_ || impl_->stack.empty()) {
    return nullptr;
  }
  return impl_->stack.back().get();
}

bool InteractionStack::activate(std::string_view id,
                                const InteractionRegistry& registry) {
  if (!impl_) {
    return false;
  }
  std::unique_ptr<Interaction> next = registry.make(id);
  if (!next) {
    return false;
  }
  while (!impl_->stack.empty()) {
    impl_->stack.back()->deactivate();
    impl_->stack.pop_back();
  }
  next->activate();
  impl_->stack.push_back(std::move(next));
  return true;
}

bool InteractionStack::push(std::string_view id,
                            const InteractionRegistry& registry) {
  if (!impl_) {
    return false;
  }
  std::unique_ptr<Interaction> next = registry.make(id);
  if (!next) {
    return false;
  }
  if (!impl_->stack.empty()) {
    impl_->stack.back()->deactivate();
  }
  next->activate();
  impl_->stack.push_back(std::move(next));
  return true;
}

bool InteractionStack::pop() {
  if (!impl_ || impl_->stack.empty()) {
    return false;
  }
  impl_->stack.back()->deactivate();
  impl_->stack.pop_back();
  if (!impl_->stack.empty()) {
    impl_->stack.back()->activate();
  }
  return true;
}

struct InputRouter::Impl {
  std::vector<std::unique_ptr<Interaction>> always_on;
  InteractionStack* stack = nullptr;
};

InputRouter::InputRouter() : impl_(std::make_unique<Impl>()) {}

InputRouter::~InputRouter() = default;

void InputRouter::add_always_on(std::unique_ptr<Interaction> handler) {
  if (impl_ && handler) {
    impl_->always_on.push_back(std::move(handler));
  }
}

void InputRouter::set_stack(InteractionStack* stack) {
  if (impl_) {
    impl_->stack = stack;
  }
}

bool InputRouter::dispatch(const content::InputEvent& e) {
  if (!impl_) {
    return false;
  }
  for (auto& handler : impl_->always_on) {
    if (handler && handler->on_input(e)) {
      return true;
    }
  }
  if (impl_->stack) {
    if (Interaction* cur = impl_->stack->current()) {
      if (cur->on_input(e)) {
        return true;
      }
    }
  }
  return false;
}

std::unique_ptr<Interaction> make_wheel_zoom() {
  return std::make_unique<WheelZoom>();
}

std::unique_ptr<Interaction> make_hover_cursor() {
  return std::make_unique<HoverCursor>();
}

}  // namespace tool
