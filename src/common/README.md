<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/common`

Cross-process / shared substrate that is **not** product `base` foundation headers and **not** `content/` wire structs.

| Module | Tree | GN | Role |
| --- | --- | --- | --- |
| **ipc** | `ipc/{codec,handle,channel,endpoint,data_pipe,invitation,portal,broker,bulk,proxy,receiver}` | `//src/common/ipc:ipc` | Win32 named pipe + BinarySink pickle frames；invitation / DataPipe / Node+Portal / PendingRemote 形状（无 IDL）。`#include "common/ipc/…"`；namespace `common::ipc`。Static；非 DLL。 |

`content/common/` remains the **content** host-protocol / thin pipe wrapper layer and may include `common/ipc`.

Layout: [`docs/superpowers/src-layout.md`](../../docs/superpowers/src-layout.md).
