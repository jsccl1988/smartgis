// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/ui_designer/llm/cursor_agent.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "ui/views/dialogs/input_text_dialog.h"
#include "ui/views/dialogs/message_box.h"

namespace app {
namespace {

std::wstring utf8_to_wide(const std::string& utf8) {
  if (utf8.empty()) {
    return {};
  }
  const int n =
      MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
  if (n <= 0) {
    return {};
  }
  std::wstring out(static_cast<size_t>(n - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, out.data(), n);
  return out;
}

bool file_exists_w(const std::wstring& path) {
  const DWORD attrs = GetFileAttributesW(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES &&
         (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring join_path(const std::wstring& a, const std::wstring& b) {
  if (a.empty()) {
    return b;
  }
  if (a.back() == L'\\' || a.back() == L'/') {
    return a + b;
  }
  return a + L"\\" + b;
}

std::wstring find_agent_on_path() {
  wchar_t buf[MAX_PATH] = {};
  const DWORD n = SearchPathW(nullptr, L"agent.exe", nullptr, MAX_PATH, buf,
                              nullptr);
  if (n > 0 && n < MAX_PATH) {
    return std::wstring(buf);
  }
  wchar_t home[MAX_PATH] = {};
  const DWORD home_n = GetEnvironmentVariableW(L"USERPROFILE", home, MAX_PATH);
  if (home_n == 0 || home_n >= MAX_PATH) {
    return {};
  }
  const std::wstring candidates[] = {
      join_path(join_path(home, L".local\\bin"), L"agent.exe"),
      join_path(join_path(home, L".local\\bin"), L"agent.cmd"),
  };
  for (const auto& c : candidates) {
    if (file_exists_w(c)) {
      return c;
    }
  }
  return {};
}

bool run_process_capture(const std::wstring& exe, const std::wstring& cmdline,
                         DWORD timeout_ms, std::string* stdout_utf8,
                         std::string* stderr_utf8, DWORD* exit_code) {
  SECURITY_ATTRIBUTES sa = {};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  HANDLE out_r = nullptr;
  HANDLE out_w = nullptr;
  HANDLE err_r = nullptr;
  HANDLE err_w = nullptr;
  if (!CreatePipe(&out_r, &out_w, &sa, 0) ||
      !CreatePipe(&err_r, &err_w, &sa, 0)) {
    return false;
  }
  SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = out_w;
  si.hStdError = err_w;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

  PROCESS_INFORMATION pi = {};
  std::wstring mutable_cmd = cmdline;
  const BOOL ok =
      CreateProcessW(exe.empty() ? nullptr : exe.c_str(), mutable_cmd.data(),
                     nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                     nullptr, &si, &pi);
  CloseHandle(out_w);
  CloseHandle(err_w);
  if (!ok) {
    CloseHandle(out_r);
    CloseHandle(err_r);
    return false;
  }

  auto read_all = [](HANDLE h) -> std::string {
    std::string out;
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(h, buf, sizeof(buf), &n, nullptr) && n > 0) {
      out.append(buf, buf + n);
    }
    return out;
  };

  const DWORD wait = WaitForSingleObject(pi.hProcess, timeout_ms);
  if (wait == WAIT_TIMEOUT) {
    TerminateProcess(pi.hProcess, 1);
    if (stderr_utf8) {
      *stderr_utf8 = "agent timed out";
    }
    CloseHandle(out_r);
    CloseHandle(err_r);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return false;
  }

  if (stdout_utf8) {
    *stdout_utf8 = read_all(out_r);
  } else {
    (void)read_all(out_r);
  }
  if (stderr_utf8) {
    *stderr_utf8 = read_all(err_r);
  } else {
    (void)read_all(err_r);
  }
  CloseHandle(out_r);
  CloseHandle(err_r);

  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  if (exit_code) {
    *exit_code = code;
  }
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}

bool install_agent_windows(std::string* error) {
  // Official Cursor CLI installer (Windows native).
  const wchar_t* ps =
      L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
      L"\"irm 'https://cursor.com/install?win32=true' | iex\"";
  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};
  std::wstring cmd = ps;
  if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      nullptr, &si, &pi)) {
    if (error) {
      *error = "failed to launch Cursor Agent installer";
    }
    return false;
  }
  WaitForSingleObject(pi.hProcess, 10 * 60 * 1000);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  if (code != 0) {
    if (error) {
      *error = "Cursor Agent installer exited with code " +
               std::to_string(static_cast<int>(code));
    }
    return false;
  }
  return true;
}

bool persist_cursor_api_key(const std::string& key, std::string* error) {
  if (key.empty()) {
    if (error) {
      *error = "empty API key";
    }
    return false;
  }
  const std::wstring wkey = utf8_to_wide(key);
  if (!SetEnvironmentVariableW(L"CURSOR_API_KEY", wkey.c_str())) {
    if (error) {
      *error = "SetEnvironmentVariable CURSOR_API_KEY failed";
    }
    return false;
  }
  // User-level persistence for future processes.
  std::wstring setx_cmd =
      L"setx CURSOR_API_KEY \"" + wkey + L"\"";
  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};
  if (CreateProcessW(nullptr, setx_cmd.data(), nullptr, nullptr, FALSE,
                     CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    WaitForSingleObject(pi.hProcess, 15000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
  return true;
}

std::wstring quote_arg(const std::wstring& s) {
  std::wstring out = L"\"";
  for (wchar_t c : s) {
    if (c == L'"') {
      out += L"\\\"";
    } else {
      out += c;
    }
  }
  out += L"\"";
  return out;
}

}  // namespace

CursorAgentLlmBackend::CursorAgentLlmBackend(HWND owner) : owner_(owner) {}

bool CursorAgentLlmBackend::ensure_ready(std::string* error) {
  agent_path_ = find_agent_on_path();
  if (agent_path_.empty()) {
    ui::views::show_message_box(
        ui::views::MessageBoxKind::kInfo,
        "Cursor Agent CLI not found. Installing via official installer…",
        owner_);
    if (!install_agent_windows(error)) {
      return false;
    }
    agent_path_ = find_agent_on_path();
    if (agent_path_.empty()) {
      if (error) {
        *error =
            "agent still not on PATH after install; add %USERPROFILE%\\.local\\"
            "bin to PATH and retry";
      }
      return false;
    }
  }

  wchar_t key_buf[2048] = {};
  const DWORD kn =
      GetEnvironmentVariableW(L"CURSOR_API_KEY", key_buf, 2048);
  if (kn == 0 || kn >= 2048) {
    std::string key;
    if (!ui::views::InputTextDialog::run(
            owner_, L"Cursor API Key",
            "Paste Cursor User API Key (Dashboard → API Keys). "
            "Stored in CURSOR_API_KEY (user env).",
            &key)) {
      if (error) {
        *error = "API key entry cancelled";
      }
      return false;
    }
    // Trim whitespace.
    while (!key.empty() &&
           (key.back() == ' ' || key.back() == '\n' || key.back() == '\r')) {
      key.pop_back();
    }
    if (!persist_cursor_api_key(key, error)) {
      return false;
    }
  }
  return true;
}

bool CursorAgentLlmBackend::complete(const std::string& system_prompt,
                                     const std::string& user_prompt,
                                     std::string* out_text,
                                     std::string* error) {
  std::string ready_err;
  if (!ensure_ready(&ready_err)) {
    if (error) {
      *error = ready_err;
    }
    return false;
  }

  const std::string combined =
      system_prompt + "\n\nUser request:\n" + user_prompt;
  const std::wstring wprompt = utf8_to_wide(combined);

  // Headless print: agent -p --output-format text "<prompt>"
  std::wstring cmdline = quote_arg(agent_path_);
  cmdline += L" -p --output-format text ";
  cmdline += quote_arg(wprompt);

  std::string stdout_s;
  std::string stderr_s;
  DWORD code = 1;
  if (!run_process_capture(agent_path_, cmdline, 120000, &stdout_s, &stderr_s,
                           &code)) {
    if (error) {
      *error = stderr_s.empty() ? "failed to run agent" : stderr_s;
    }
    return false;
  }
  if (code != 0) {
    if (error) {
      *error = "agent exit " + std::to_string(static_cast<int>(code)) + ": " +
               (stderr_s.empty() ? stdout_s : stderr_s);
    }
    return false;
  }
  if (out_text) {
    *out_text = std::move(stdout_s);
  }
  return true;
}

}  // namespace app
