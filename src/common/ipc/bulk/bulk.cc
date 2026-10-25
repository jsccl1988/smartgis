// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "common/ipc/bulk/bulk.h"

#include "common/ipc/codec/codec.h"
#include "common/ipc/data_pipe/data_pipe.h"

namespace common {
namespace ipc {

bool send_bulk(Channel* channel,
               uint16_t original_type,
               uint32_t view_id,
               const void* bytes,
               uint32_t byte_count) {
  if (!channel || !channel->is_open()) {
    return false;
  }
  if (byte_count == 0) {
    return channel->send(original_type, view_id, nullptr, 0);
  }
  if (byte_count <= k_bulk_inline_max) {
    return channel->send(original_type, view_id, bytes, byte_count);
  }
  if (byte_count > k_max_payload_bytes) {
    return false;
  }

  DataPipeProducer producer;
  DataPipeConsumer consumer;
  const uint32_t capacity =
      byte_count < 64u ? 64u : byte_count;
  if (!create_data_pipe(capacity, &producer, &consumer)) {
    return false;
  }
  uint32_t written = 0;
  if (!producer.write(bytes, byte_count, &written) || written != byte_count) {
    return false;
  }

  PlatformHandle exported[k_data_pipe_handles];
  if (!consumer.export_handles(exported)) {
    return false;
  }

  DataPipeOfferBody offer;
  offer.original_type = original_type;
  offer.byte_count = byte_count;
  const std::string body = encode(offer);
  // Keep producer/consumer mapped until send returns so the ring stays alive
  // while wrap_into duplicates the exported handles into the peer.
  return channel->send(k_msg_data_pipe_offer, view_id, 0, body.data(),
                       static_cast<uint32_t>(body.size()), exported,
                       k_data_pipe_handles);
}

bool take_bulk_offer(const Frame& header,
                     const std::vector<uint8_t>& payload,
                     std::vector<PlatformHandle>* handles,
                     uint16_t* out_original_type,
                     std::vector<uint8_t>* out_payload) {
  if (!handles || !out_original_type || !out_payload) {
    return false;
  }
  if (header.type != k_msg_data_pipe_offer) {
    return false;
  }
  DataPipeOfferBody offer;
  if (!decode(payload.data(), payload.size(), &offer) ||
      offer.byte_count == 0 || offer.byte_count > k_max_payload_bytes) {
    return false;
  }
  if (handles->size() < k_data_pipe_handles) {
    return false;
  }
  PlatformHandle in[k_data_pipe_handles];
  for (uint32_t i = 0; i < k_data_pipe_handles; ++i) {
    in[i] = std::move((*handles)[i]);
  }
  handles->clear();

  DataPipeConsumer consumer;
  if (!consumer.adopt_handles(in)) {
    return false;
  }
  out_payload->assign(offer.byte_count, 0);
  uint32_t got = 0;
  if (!consumer.read(out_payload->data(), offer.byte_count, &got) ||
      got != offer.byte_count) {
    out_payload->clear();
    return false;
  }
  *out_original_type = offer.original_type;
  return true;
}

}  // namespace ipc
}  // namespace common
