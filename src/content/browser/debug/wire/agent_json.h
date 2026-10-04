// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_AGENT_JSON_H_
#define CONTENT_BROWSER_DEBUG_AGENT_JSON_H_

#include <string>

namespace content {
namespace detail {

// NDJSON / RapidJSON helpers shared by DebugAgent domain handlers.
std::string json_escape(const std::string& s);

bool extract_string_field(const std::string& json,
                          const char* key,
                          std::string* out);

bool extract_int_field(const std::string& json, const char* key, int* out);

std::string ok_result(int id, const std::string& result_obj);
std::string err_result(int id, const std::string& error);
std::string ui_text_result(int id, const std::string& text);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_AGENT_JSON_H_
