// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_TEXT2UI_LLM_BACKEND_H_
#define UI_VIEWS_TEXT2UI_LLM_BACKEND_H_

#include "ui/ui_export.h"
#include <string>

namespace ui {
namespace views {

// Host-injected LLM completion (Cursor Agent, HTTP, mock). Not implemented
// inside ui_views — keeps net/CLI deps out of the toolkit DLL.
class UI_EXPORT LlmBackend {
 public:
  virtual ~LlmBackend() = default;

  // Writes model text to |out_text|. On failure sets |error| and returns false.
  virtual bool complete(const std::string& system_prompt,
                        const std::string& user_prompt,
                        std::string* out_text,
                        std::string* error) = 0;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_TEXT2UI_LLM_BACKEND_H_
