<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Legacy XML

Vendored TinyXML under this directory was **removed** (2026-09-28).

Product XML parsing/writing uses **pugixml** via `//third_party:pugixml`
(see `docs/superpowers/specs/2026-09-28-third-party-json-xml-protobuf-design.md`).

The GN group `:xml` forwards to pugixml for transitional deps.
