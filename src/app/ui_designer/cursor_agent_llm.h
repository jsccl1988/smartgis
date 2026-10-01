// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_UI_DESIGNER_CURSOR_AGENT_LLM_H_
#define APP_UI_DESIGNER_CURSOR_AGENT_LLM_H_

#include <string>

#include <windows.h>

#include "ui/views/text2ui/llm_backend.h"

namespace app {

// LlmBackend that shells out to Cursor Agent CLI (`agent -p`).
// Ensures install + CURSOR_API_KEY (paste via InputTextDialog when missing).
class CursorAgentLlmBackend : public ui::views::LlmBackend {
 public:
  explicit CursorAgentLlmBackend(HWND owner);

  bool complete(const std::string& system_prompt,
                const std::string& user_prompt,
                std::string* out_text,
                std::string* error) override;

  // Detect/install agent; prompt for API key if needed. Returns false on abort.
  bool ensure_ready(std::string* error);

 private:
  HWND owner_ = nullptr;
  std::wstring agent_path_;
};

}  // namespace app

#endif  // APP_UI_DESIGNER_CURSOR_AGENT_LLM_H_
