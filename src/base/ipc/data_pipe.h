// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_IPC_DATA_PIPE_H
#define BASE_IPC_DATA_PIPE_H

#include <cstdint>

#include "base/ipc/handle.h"

namespace base {
namespace ipc {

inline constexpr uint32_t k_data_pipe_handles = 3;

// Unidirectional byte stream (Mojo DataPipe). Ring in shared memory + events.
class DataPipeProducer {
 public:
  DataPipeProducer() = default;
  ~DataPipeProducer();
  DataPipeProducer(const DataPipeProducer&) = delete;
  DataPipeProducer& operator=(const DataPipeProducer&) = delete;
  DataPipeProducer(DataPipeProducer&& other) noexcept;
  DataPipeProducer& operator=(DataPipeProducer&& other) noexcept;

  bool write(const void* data, uint32_t bytes, uint32_t* written);
  bool export_handles(PlatformHandle out[k_data_pipe_handles]);
  bool adopt_handles(PlatformHandle in[k_data_pipe_handles]);
  bool is_valid() const;

 private:
  friend bool create_data_pipe(uint32_t, DataPipeProducer*, class DataPipeConsumer*);
  void close();

  HANDLE section_ = nullptr;
  HANDLE data_ev_ = nullptr;
  HANDLE space_ev_ = nullptr;
  void* view_ = nullptr;
};

class DataPipeConsumer {
 public:
  DataPipeConsumer() = default;
  ~DataPipeConsumer();
  DataPipeConsumer(const DataPipeConsumer&) = delete;
  DataPipeConsumer& operator=(const DataPipeConsumer&) = delete;
  DataPipeConsumer(DataPipeConsumer&& other) noexcept;
  DataPipeConsumer& operator=(DataPipeConsumer&& other) noexcept;

  bool read(void* data, uint32_t bytes, uint32_t* got);
  bool export_handles(PlatformHandle out[k_data_pipe_handles]);
  bool adopt_handles(PlatformHandle in[k_data_pipe_handles]);
  bool is_valid() const;

 private:
  friend bool create_data_pipe(uint32_t, DataPipeProducer*, DataPipeConsumer*);
  void close();

  HANDLE section_ = nullptr;
  HANDLE data_ev_ = nullptr;
  HANDLE space_ev_ = nullptr;
  void* view_ = nullptr;
};

bool create_data_pipe(uint32_t capacity,
                      DataPipeProducer* producer,
                      DataPipeConsumer* consumer);

}  // namespace ipc
}  // namespace base

#endif  // BASE_IPC_DATA_PIPE_H
