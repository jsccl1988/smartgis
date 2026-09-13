<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Net ASIO + cpp-httplib Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> User directed **no further confirmation** and **parallel execution**. Stay on `master`. Do **not** commit unless the user asks.

**Goal:** Replace homemade `SmtNetCore` with standalone ASIO sockets plus cpp-httplib HTTP and an ASIO FnRPC client (CRLF pickle).

**Architecture:** `SmtNetCore` remains `//src/net:net`. ASIO and httplib are private GN configs. `net::HttpClient` / `net::RpcClient` are the product facades. **Map-server UDP / `net::UdpSocket` / `src/web` are cancelled** (stack deleted 2026-09-13; UDP facade removed 2026-09-14 — do not restore). See the spec.

**Tech Stack:** C++20 MSVC v145, standalone ASIO `asio-1-30-2`, cpp-httplib v0.18.3, GN `smt_shared_library` + `test()`.

**Spec:** `docs/superpowers/specs/2026-09-13-net-asio-httplib-design.md`

## Global Constraints

- Stay on `master`; do not create topic branches; do not commit unless the user asks.
- Copyright: `// Copyright (c) 2026 The Mogu Authors.` on every new/touched engineering file.
- New C++: `snake_case` functions; public namespaces at most `net`; internals in `net::detail` or anonymous namespace. Types PascalCase.
- `cc_std` is the repo default (`c++20` / MSVC `/std:c++20`). No `/std:c++17`.
- Comments in English.
- Do not vendor Chromium net, Boost.Asio, gRPC, brpc, Qt.
- `asio.hpp` and `httplib.h` are **private** to `src/net` `.cpp` files.
- `dll_stem` stays `SmtNetCore`.
- Build output only under repo-root `out/` via `build.bat`.
- No `Co-authored-by: Cursor`.
- Do **not** restore `src/net/udp`, `SmtMapServer`, or WMS-over-UDP.

## File map

| Path | Responsibility |
| --- | --- |
| `third_party/manifest.json` | asio + cpp-httplib GitHub pins, `install_skip` |
| `third_party/BUILD.gn` | `:asio` / `:cpp_httplib` configs |
| `src/net/BUILD.gn` | DLL sources + `net_test` |
| `src/net/http.h/.cpp` | HTTP facade |
| `src/net/rpc.h/.cpp` | RPC slot |
| `src/net/net_test.cc` | loopback tests (no UDP) |
| `src/sdb/datasource/ws/BUILD.gn` | drop unused net dep |
| `BUILD.gn` | `test_all` += net_test |
| `docs/README.md`, root `README.md` | index |
| ~~`src/net/udp.h/.cpp`~~ | **Superseded by deletion** — do not recreate |

---

### Task 1: pin ASIO + cpp-httplib

**Files:**
- Modify: `third_party/manifest.json`
- Modify: `third_party/BUILD.gn`

**Interfaces:**
- Produces: `//third_party:asio` (include `asio.hpp`, defines `ASIO_STANDALONE`, `ASIO_NO_DEPRECATED`)
- Produces: `//third_party:cpp_httplib` (include `httplib.h`)

- [ ] **Step 1:** Set asio `git_url` to `https://github.com/chriskohlhoff/asio.git`, `git_ref` `asio-1-30-2`, `install_skip: true`. Set cpp-httplib `git_url` to `https://github.com/yhirose/cpp-httplib.git`, keep `v0.18.3`, `install_skip: true`.
- [ ] **Step 2:** Fetch:

```bat
py -3 third_party\tools\fetch.py --package asio --package cpp-httplib
```

Expected: `third_party/.src/asio/asio/include/asio.hpp` and `third_party/.src/cpp-httplib/httplib.h` exist.

- [ ] **Step 3:** GN groups with those include dirs. Do not add them to `//build:legacy`.

---

### Task 2: replace `src/net`

**Files:**
- Create: `src/net/http.h`, `http.cpp`, `rpc.h`, `rpc.cpp`, `net_test.cc`
- Modify: `BUILD.gn`
- Delete: WebAppLib sources listed in the spec; **`src/net/udp/`** (cancelled product UDP)

**Interfaces:**
- Consumes: Task 1 groups (private_deps / private configs)
- Produces: `net::HttpClient::get/post`; `net::RpcClient`
- ~~Produces: `net::UdpSocket`~~ — **Cancelled / superseded by deletion**

Public HTTP:

```cpp
namespace net {
struct HttpResult {
  bool ok = false;
  int status = 0;
  std::string body;
  std::string error;
};
class HttpClient {
 public:
  HttpResult get(const std::string& url, int timeout_sec = 5);
  HttpResult post(const std::string& url, const std::string& body,
                  const std::string& content_type, int timeout_sec = 5);
};
}
```

- [ ] **Step 1:** Write `net_test.cc` (HTTP loopback + RPC + pickle). Build test — fail until sources exist.
- [x] **Cancelled:** ASIO `UdpSocket` pimpl / UDP echo tests — facade removed; do not restore.
- [ ] **Step 2:** Implement `HttpClient`; RPC. Delete WebAppLib files.
- [ ] **Step 3:** `ninja -C out net_test` then `out\net_test.exe` — exit 0.
- [ ] **Step 4:** Public headers must not contain `asio.hpp` or `httplib.h`.

---

### Task 3: callers

**Files:**
- Modify: `src/sdb/datasource/ws/BUILD.gn`

- [x] **Cancelled:** `src/web/cgi/main.cpp` — web stack deleted.
- [ ] **Step 2:** Remove `//src/net:net` from `sde_ws` deps.
- [x] **Cancelled / superseded:** `web/server` includes `udp.h` — web + UDP deleted; do not wire.

---

### Task 4: docs + `test_all`

**Files:**
- Modify: `BUILD.gn`, `docs/README.md`, `README.md`

- [ ] **Step 1:** `test_all` += `//src/net:net_test`.
- [ ] **Step 2:** Index the spec and plan. Refresh README “最后更新” to 2026-09-13.
- [ ] **Step 3:** `.\build.bat` then `.\build.bat te` until green (`out/build.log` exit 0).

## Self-review

1. Spec coverage: pins, ASIO TCP, HTTP, RPC slot, WebAppLib removal, callers, tests, isolation. UDP/MapServer marked cancelled.
2. No TBD wire format.
3. Names: `HttpClient::get` / `post`, `HttpResult`, `rpc_transport_traits`.
