<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Net stack: standalone ASIO + cpp-httplib (FnRPC slot)

**Date:** 2026-09-13  
**Status:** approved (source of truth; UDP / MapServer facts corrected 2026-09-14)  
**Scope:** replace homemade `SmtNetCore` (Winsock 1.1 + WebAppLib) with OSS. HTTP this pass; FnRPC later on the same ASIO IO. This document does not implement C++.

## Goal

Stop owning a socket/HTTP/CGI kit. `src/net` stays the product facade (`dll_stem` `SmtNetCore`). IO is **standalone ASIO** (IOCP on Windows). HTTP is **cpp-httplib** (already in `manifest.json`). RPC is `net::RpcClient` on the same ASIO: production `net::fn` framing (CRLF + pickle) without copying mogu `net/` or using Chromium `net/`.

## Non-goals

- Do not vendor Chromium `net/`, Boost.Asio, libhv, curl, CivetWeb, gRPC, brpc, ZeroMQ, or Qt Network.
- Do not `#include` mogu `net/tcp`, `net/rpc/fn`, `base/io`, `base/core/log.h`, `<sys/epoll.h>`, or `<sys/eventfd.h>`.
- Do not treat Mojo as service RPC (Mojo is same-PE `--type=` only; see `2026-09-13-base-ipc-mojom-design.md`).
- Do not implement Windows `fn_server`, live tabled `:9030` in `net_test`, or copy mogu `net/`.
- Do not use `fnA`/`fnC` length-prefix frames against tabled `:9030`.
- Do not keep WebAppLib (`webapp::Cgi` / Template / ConfigFile / DateTime / Encode / TextFile / `net_string`).
- Do not change `dll_stem` (`SmtNetCore`). Do not change `content/public`.
- Do not put `asio.hpp` or `httplib.h` on `src_all` public include paths.
- Qt is banned.
- **Do not restore** `net::UdpSocket` / `src/net/udp`, `SmtMapServer`, homemade WMS-over-UDP, or the deleted `src/web` / `plugin/map_service` stack. Those paths are gone; as-built is HTTP + RPC only (see `docs/README.md`, `docs/build/ui-shell-multiprocess.md`).

## Architecture

```
sdb/datasource/ws     later fnw / tabled client
        |
        |  product headers (`net/http/http.h`, `net/rpc/rpc.h`)
        v
src/net  namespace net
        |
        +-- TCP         standalone ASIO (private)
        +-- HTTP        cpp-httplib (private; this pass)
        +-- RPC         `net::RpcClient` (ASIO TCP, tabled CRLF pickle)
        |
        x  no UDP product facade, no web/MapServer, no //mogu net, no Chromium net/, no Boost
```

Same layering as mogu: contract → framing → reactor. This repo replaces only the reactor (ASIO) and HTTP library. Linux tabled remains the FnRPC peer.

| Channel | Role | This change |
| --- | --- | --- |
| ASIO TCP | sockets; future FnRPC IO | yes |
| cpp-httplib | HTTP client facade | yes |
| Chromium Mojo | browser/renderer/gpu | out of scope |
| Chromium `net/` | browser stack | not this pass |
| Product UDP / WMS | former MapServer path | **removed** — do not revive |

## Components

| Unit | Role | Depends on |
| --- | --- | --- |
| `//third_party:asio` | header-only standalone ASIO; `ASIO_STANDALONE` | fetch `third_party/.src/asio` |
| `//third_party:cpp_httplib` | header-only HTTP; no OpenSSL | fetch `third_party/.src/cpp-httplib` |
| `net::HttpClient` | GET/POST; host/path parse in `.cc` | cpp-httplib private |
| `net::rpc_transport_traits` / `RpcClient` | ASIO TCP + CRLF pickle; `AsioTcp` supported | ASIO private |
| `//src/net:net` | `SmtNetCore` DLL | asio + httplib **private** configs; `ws2_32` |

Public C++ stays two levels: `net`. Helpers in `net::detail` or an anonymous namespace in `.cpp`.

### Traits (C++20)

```cpp
namespace net {

struct AsioTcp {};

template <typename Transport>
struct rpc_transport_traits {
  static constexpr bool supported = false;
};

template <>
struct rpc_transport_traits<AsioTcp> {
  static constexpr bool supported = true;
};

template <typename Transport>
concept rpc_transport = rpc_transport_traits<Transport>::supported;

}  // namespace net
```

A later tabled live task can point `RpcClient` at `:9030`. This pass loopback-tests the same CRLF pickle; it does not copy mogu `net/` or add `fnw` sources.

## Data flow

**UDP / MapServer (historical — superseded):** `SmtMapServer`, `src/web`, and `plugin/map_service` are deleted. The former `net::UdpSocket` facade (`src/net/udp`) and WMS-over-UDP protocol are **removed**. Do not reintroduce them; there is no product UDP caller.

**HTTP:** callers construct `net::HttpClient` and `get`/`post` a URL. The `.cc` uses `httplib::Client`. Timeouts are seconds. Errors become `HttpResult.ok == false` plus `error` text. No exceptions cross the DLL boundary.

**RPC:** `RpcClient::connect` opens ASIO TCP. Framing is a binary-only subset of mogu `base/archive` + `net/pack/pickle`（本仓：`src/base/archive/` + `src/net/pack/pickle.h`，`src/net/rpc/wire.h`）: native-endian `BinarySink` atoms（`base::`）, strings as `size_t` length + bytes (cap 64MiB), `message_t` writes the value only when `error_code == 0`. `call(method, json)` pickles `RpcMessage{head, inner}` where inner is method name + JSON string, then writes payload + CRLF. Responses unpickle a JSON string. Unknown methods return `error_code == 2` (not bound). No JSON/YAML/Text sinks. No live tabled `:9030` in `net_test`. A1 hoist：BinarySink/Serializer 在 `base/archive`；`net::Pickle` 仍在 `net/pack`。

## Error handling

- `HttpResult.status` is the HTTP status (0 if no response). `error` is a short English string (`connect failed`, `timeout`, `invalid url`).
- ASIO `error_code` stays inside Impl / `.cpp` for RPC sockets.
- Do not throw from exported functions.

## Testing

`src/net/net_test.cc` via `test("net_test")`, registered in `//:test_all`.

| Case | Pass |
| --- | --- |
| Pickle | method+JSON roundtrip; `RpcMessage` envelope; error skips value; oversize string rejected |
| HTTP loopback | in-process httplib server + `net::HttpClient::get` body match |
| RPC loopback | in-process ASIO server; `connect` + `call("echo")` JSON match; unbound method `error_code==2` |
| Isolation | `asio.hpp` / `httplib.h` not in public headers |

No UDP loopback (facade removed). No live tabled `:9030` in this pass. No `FNW_TABLED_REQUIRE`.

## Pins

| Package | Ref | Notes |
| --- | --- | --- |
| asio | `https://github.com/chriskohlhoff/asio.git` tag `asio-1-30-2` | `install_skip`; `#include <asio.hpp>` |
| cpp-httplib | `https://github.com/yhirose/cpp-httplib.git` tag `v0.18.3` | `install_skip`; no OpenSSL |

Gitea URLs may remain as comments; fetch must succeed from GitHub (or `MOGU_GITHUB_MIRROR`).

## Callers this pass

| Caller | Change |
| --- | --- |
| `web/server`, `web/cgi` | **Cancelled / deleted** — no web stack; do not wire UDP |
| `sdb/datasource/ws` | drop unused `//src/net:net` dep (no HTTP yet) |

## File map

| Path | Responsibility |
| --- | --- |
| `third_party/manifest.json` | asio + cpp-httplib pins |
| `third_party/BUILD.gn` | private configs + groups |
| `src/net/BUILD.gn` | one `SmtNetCore` DLL; sources in subdirs (`http`, `rpc`; no `udp`) |
| `src/net/http/http.h/.cpp` | `net::HttpClient` |
| `src/base/archive/archive.h` | mogu-aligned BinarySink / Serializer（`base::`；A1） |
| `src/net/pack/pickle.h` | `net::Pickle`（deps → `base/archive`） |
| `src/net/rpc/rpc.h/.cpp` / `wire.h` | ASIO FnRPC + CRLF envelope |
| `src/net/net_test.cc` | loopback tests (pickle / HTTP / RPC) |
| delete | `cgi.cpp/.h`, `template.*`, `config_file.*`, `date_time.*`, `encode.*`, `text_file.*`, `utility.*`, `net_string.*`, `http_client.*`, `web_app_lib.h` |
| delete (2026-09-14) | `src/net/udp/udp.h/.cpp` — product UDP facade removed |

## Self-review

1. Placeholders: none. FnRPC is an explicit later slot, not TBD wire.
2. Isolation matches Mojo: Chromium/ASIO/httplib headers are private.
3. Scope is one DLL + two header-only pins. No MapServer / WMS rewrite.
4. Product surface is `HttpClient` + `RpcClient` only; UDP is not part of the live contract.
