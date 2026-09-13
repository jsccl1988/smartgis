// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/interaction.h"

#include <memory>

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

bool InteractionRegistry::add(std::string_view id, InteractionFactory factory) {
  if (id.empty() || !factory) {
    return false;
  }
  std::string key(id);
  if (factories_.find(key) != factories_.end()) {
    return false;
  }
  factories_.emplace(std::move(key), std::move(factory));
  return true;
}

std::unique_ptr<Interaction> InteractionRegistry::make(
    std::string_view id) const {
  auto it = factories_.find(std::string(id));
  if (it == factories_.end()) {
    return nullptr;
  }
  return it->second();
}

Interaction* InteractionStack::current() const {
  if (stack_.empty()) {
    return nullptr;
  }
  return stack_.back().get();
}

bool InteractionStack::activate(std::string_view id,
                                 const InteractionRegistry& registry) {
  std::unique_ptr<Interaction> next = registry.make(id);
  if (!next) {
    return false;
  }
  while (!stack_.empty()) {
    stack_.back()->deactivate();
    stack_.pop_back();
  }
  next->activate();
  stack_.push_back(std::move(next));
  return true;
}

bool InteractionStack::push(std::string_view id,
                            const InteractionRegistry& registry) {
  std::unique_ptr<Interaction> next = registry.make(id);
  if (!next) {
    return false;
  }
  if (!stack_.empty()) {
    stack_.back()->deactivate();
  }
  next->activate();
  stack_.push_back(std::move(next));
  return true;
}

bool InteractionStack::pop() {
  if (stack_.empty()) {
    return false;
  }
  stack_.back()->deactivate();
  stack_.pop_back();
  if (!stack_.empty()) {
    stack_.back()->activate();
  }
  return true;
}

void InputRouter::add_always_on(std::unique_ptr<Interaction> handler) {
  if (handler) {
    always_on_.push_back(std::move(handler));
  }
}

void InputRouter::set_stack(InteractionStack* stack) {
  stack_ = stack;
}

bool InputRouter::dispatch(const content::InputEvent& e) {
  for (auto& handler : always_on_) {
    if (handler && handler->on_input(e)) {
      return true;
    }
  }
  if (stack_) {
    if (Interaction* cur = stack_->current()) {
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
