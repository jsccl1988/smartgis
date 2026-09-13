// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_NET_ARCHIVE_H
#define SMT_NET_ARCHIVE_H

// Compatibility shim: BinarySink/archive moved to base/archive (mogu layout).
#include "base/archive/archive.h"

namespace net {

// Convenience aliases only. Do not specialize templates via these names —
// specialize base::binary_format_traits / base::BinarySink in namespace base.
using base::BinarySink;
using base::Deserializer;
using base::FieldSink;
using base::InArchiver;
using base::OutArchiver;
using base::Serializer;
using base::binary_format;
using base::k_binary_wire_max_string_bytes;
using base::read_value;
using base::write_value;

}  // namespace net

#endif  // SMT_NET_ARCHIVE_H
