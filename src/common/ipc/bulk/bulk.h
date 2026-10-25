// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef COMMON_IPC_BULK_BULK_H_
#define COMMON_IPC_BULK_BULK_H_

#include <cstdint>
#include <string>
#include <vector>

#include "common/ipc/channel/channel.h"
#include "common/ipc/handle/handle.h"

namespace common {
namespace ipc {

// Payloads larger than this ride a DataPipe; the control frame stays small.
inline constexpr uint32_t k_bulk_inline_max = 48u * 1024u;

// Control-plane offer: original message type + byte count; 3 DataPipe handles.
inline constexpr uint16_t k_msg_data_pipe_offer = 0xFF30;

struct DataPipeOfferBody {
  uint16_t original_type = 0;
  uint32_t byte_count = 0;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(original_type, byte_count);
  }
};

// Sends |bytes| inline when small; otherwise creates a DataPipe, writes the
// bytes, and sends k_msg_data_pipe_offer with the three consumer handles.
bool send_bulk(Channel* channel,
               uint16_t original_type,
               uint32_t view_id,
               const void* bytes,
               uint32_t byte_count);

inline bool send_bulk(Channel* channel,
                      uint16_t original_type,
                      uint32_t view_id,
                      const std::string& bytes) {
  return send_bulk(channel, original_type, view_id, bytes.data(),
                   static_cast<uint32_t>(bytes.size()));
}

// If |header.type| is k_msg_data_pipe_offer, adopts the three handles, reads
// |byte_count| into |out_payload|, and sets |out_original_type|. Returns true
// when a bulk offer was consumed (caller should dispatch as original type).
bool take_bulk_offer(const Frame& header,
                     const std::vector<uint8_t>& payload,
                     std::vector<PlatformHandle>* handles,
                     uint16_t* out_original_type,
                     std::vector<uint8_t>* out_payload);

}  // namespace ipc
}  // namespace common

#endif  // COMMON_IPC_BULK_BULK_H_
