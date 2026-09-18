// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/ipc/data_pipe.h"

#include <cstring>

namespace base {
namespace ipc {
namespace {

struct RingHeader {
  uint32_t capacity = 0;
  uint32_t read_pos = 0;
  uint32_t write_pos = 0;
  uint32_t producer_closed = 0;
  uint32_t consumer_closed = 0;
};

uint32_t used(const RingHeader* h) {
  return (h->write_pos + h->capacity - h->read_pos) % h->capacity;
}

uint8_t* bytes_of(RingHeader* h) {
  return reinterpret_cast<uint8_t*>(h + 1);
}

bool map_view(HANDLE section, void** view) {
  *view = MapViewOfFile(section, FILE_MAP_ALL_ACCESS, 0, 0, 0);
  return *view != nullptr;
}

bool export_three(HANDLE section, HANDLE data_ev, HANDLE space_ev,
                  PlatformHandle out[k_data_pipe_handles]) {
  if (!section || !data_ev || !space_ev || !out) {
    return false;
  }
  HANDLE dup_s = nullptr;
  HANDLE dup_d = nullptr;
  HANDLE dup_p = nullptr;
  if (!DuplicateHandle(GetCurrentProcess(), section, GetCurrentProcess(),
                       &dup_s, 0, FALSE, DUPLICATE_SAME_ACCESS) ||
      !DuplicateHandle(GetCurrentProcess(), data_ev, GetCurrentProcess(),
                       &dup_d, 0, FALSE, DUPLICATE_SAME_ACCESS) ||
      !DuplicateHandle(GetCurrentProcess(), space_ev, GetCurrentProcess(),
                       &dup_p, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    if (dup_s) {
      CloseHandle(dup_s);
    }
    if (dup_d) {
      CloseHandle(dup_d);
    }
    if (dup_p) {
      CloseHandle(dup_p);
    }
    return false;
  }
  out[0] = PlatformHandle::adopt(dup_s);
  out[1] = PlatformHandle::adopt(dup_d);
  out[2] = PlatformHandle::adopt(dup_p);
  return true;
}

bool adopt_three(PlatformHandle in[k_data_pipe_handles], HANDLE* section,
                 HANDLE* data_ev, HANDLE* space_ev, void** view) {
  if (!in || !in[0].is_valid() || !in[1].is_valid() || !in[2].is_valid()) {
    return false;
  }
  *section = in[0].release();
  *data_ev = in[1].release();
  *space_ev = in[2].release();
  return map_view(*section, view);
}

}  // namespace

DataPipeProducer::~DataPipeProducer() {
  close();
}

DataPipeProducer::DataPipeProducer(DataPipeProducer&& other) noexcept
    : section_(other.section_),
      data_ev_(other.data_ev_),
      space_ev_(other.space_ev_),
      view_(other.view_) {
  other.section_ = nullptr;
  other.data_ev_ = nullptr;
  other.space_ev_ = nullptr;
  other.view_ = nullptr;
}

DataPipeProducer& DataPipeProducer::operator=(DataPipeProducer&& other) noexcept {
  if (this != &other) {
    close();
    section_ = other.section_;
    data_ev_ = other.data_ev_;
    space_ev_ = other.space_ev_;
    view_ = other.view_;
    other.section_ = nullptr;
    other.data_ev_ = nullptr;
    other.space_ev_ = nullptr;
    other.view_ = nullptr;
  }
  return *this;
}

void DataPipeProducer::close() {
  if (view_) {
    auto* h = static_cast<RingHeader*>(view_);
    h->producer_closed = 1;
    if (data_ev_) {
      SetEvent(data_ev_);
    }
    UnmapViewOfFile(view_);
    view_ = nullptr;
  }
  if (section_) {
    CloseHandle(section_);
    section_ = nullptr;
  }
  if (data_ev_) {
    CloseHandle(data_ev_);
    data_ev_ = nullptr;
  }
  if (space_ev_) {
    CloseHandle(space_ev_);
    space_ev_ = nullptr;
  }
}

bool DataPipeProducer::is_valid() const {
  return view_ != nullptr;
}

bool DataPipeProducer::write(const void* data, uint32_t n, uint32_t* written) {
  if (written) {
    *written = 0;
  }
  if (!view_ || !data || n == 0) {
    return false;
  }
  auto* h = static_cast<RingHeader*>(view_);
  const DWORD start = GetTickCount();
  while (used(h) + n >= h->capacity) {
    if (h->consumer_closed) {
      return false;
    }
    if (GetTickCount() - start > 5000) {
      return false;
    }
    WaitForSingleObject(space_ev_, 50);
  }
  uint8_t* ring = bytes_of(h);
  const uint32_t cap = h->capacity;
  uint32_t pos = h->write_pos;
  const char* src = static_cast<const char*>(data);
  for (uint32_t i = 0; i < n; ++i) {
    ring[pos] = static_cast<uint8_t>(src[i]);
    pos = (pos + 1) % cap;
  }
  h->write_pos = pos;
  SetEvent(data_ev_);
  if (written) {
    *written = n;
  }
  return true;
}

bool DataPipeProducer::export_handles(PlatformHandle out[k_data_pipe_handles]) {
  return export_three(section_, data_ev_, space_ev_, out);
}

bool DataPipeProducer::adopt_handles(PlatformHandle in[k_data_pipe_handles]) {
  close();
  return adopt_three(in, &section_, &data_ev_, &space_ev_, &view_);
}

DataPipeConsumer::~DataPipeConsumer() {
  close();
}

DataPipeConsumer::DataPipeConsumer(DataPipeConsumer&& other) noexcept
    : section_(other.section_),
      data_ev_(other.data_ev_),
      space_ev_(other.space_ev_),
      view_(other.view_) {
  other.section_ = nullptr;
  other.data_ev_ = nullptr;
  other.space_ev_ = nullptr;
  other.view_ = nullptr;
}

DataPipeConsumer& DataPipeConsumer::operator=(DataPipeConsumer&& other) noexcept {
  if (this != &other) {
    close();
    section_ = other.section_;
    data_ev_ = other.data_ev_;
    space_ev_ = other.space_ev_;
    view_ = other.view_;
    other.section_ = nullptr;
    other.data_ev_ = nullptr;
    other.space_ev_ = nullptr;
    other.view_ = nullptr;
  }
  return *this;
}

void DataPipeConsumer::close() {
  if (view_) {
    auto* h = static_cast<RingHeader*>(view_);
    h->consumer_closed = 1;
    if (space_ev_) {
      SetEvent(space_ev_);
    }
    UnmapViewOfFile(view_);
    view_ = nullptr;
  }
  if (section_) {
    CloseHandle(section_);
    section_ = nullptr;
  }
  if (data_ev_) {
    CloseHandle(data_ev_);
    data_ev_ = nullptr;
  }
  if (space_ev_) {
    CloseHandle(space_ev_);
    space_ev_ = nullptr;
  }
}

bool DataPipeConsumer::is_valid() const {
  return view_ != nullptr;
}

bool DataPipeConsumer::read(void* data, uint32_t n, uint32_t* got) {
  if (got) {
    *got = 0;
  }
  if (!view_ || !data || n == 0) {
    return false;
  }
  auto* h = static_cast<RingHeader*>(view_);
  const DWORD start = GetTickCount();
  while (used(h) < n) {
    if (h->producer_closed && used(h) == 0) {
      return false;
    }
    if (GetTickCount() - start > 5000) {
      return false;
    }
    WaitForSingleObject(data_ev_, 50);
  }
  uint8_t* ring = bytes_of(h);
  const uint32_t cap = h->capacity;
  uint32_t pos = h->read_pos;
  char* dst = static_cast<char*>(data);
  for (uint32_t i = 0; i < n; ++i) {
    dst[i] = static_cast<char>(ring[pos]);
    pos = (pos + 1) % cap;
  }
  h->read_pos = pos;
  SetEvent(space_ev_);
  if (got) {
    *got = n;
  }
  return true;
}

bool DataPipeConsumer::export_handles(PlatformHandle out[k_data_pipe_handles]) {
  return export_three(section_, data_ev_, space_ev_, out);
}

bool DataPipeConsumer::adopt_handles(PlatformHandle in[k_data_pipe_handles]) {
  close();
  return adopt_three(in, &section_, &data_ev_, &space_ev_, &view_);
}

bool create_data_pipe(uint32_t capacity,
                      DataPipeProducer* producer,
                      DataPipeConsumer* consumer) {
  if (!producer || !consumer || capacity < 8) {
    return false;
  }
  producer->close();
  consumer->close();
  const uint32_t ring = capacity + 1;
  const DWORD bytes = static_cast<DWORD>(sizeof(RingHeader) + ring);
  HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
                                      PAGE_READWRITE, 0, bytes, nullptr);
  if (!section) {
    return false;
  }
  HANDLE data_ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  HANDLE space_ev = CreateEventW(nullptr, FALSE, TRUE, nullptr);
  if (!data_ev || !space_ev) {
    if (data_ev) {
      CloseHandle(data_ev);
    }
    if (space_ev) {
      CloseHandle(space_ev);
    }
    CloseHandle(section);
    return false;
  }
  HANDLE section2 = nullptr;
  HANDLE data2 = nullptr;
  HANDLE space2 = nullptr;
  if (!DuplicateHandle(GetCurrentProcess(), section, GetCurrentProcess(),
                       &section2, 0, FALSE, DUPLICATE_SAME_ACCESS) ||
      !DuplicateHandle(GetCurrentProcess(), data_ev, GetCurrentProcess(),
                       &data2, 0, FALSE, DUPLICATE_SAME_ACCESS) ||
      !DuplicateHandle(GetCurrentProcess(), space_ev, GetCurrentProcess(),
                       &space2, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    CloseHandle(section);
    CloseHandle(data_ev);
    CloseHandle(space_ev);
    return false;
  }
  void* view_p = nullptr;
  void* view_c = nullptr;
  if (!map_view(section, &view_p) || !map_view(section2, &view_c)) {
    CloseHandle(section);
    CloseHandle(section2);
    CloseHandle(data_ev);
    CloseHandle(data2);
    CloseHandle(space_ev);
    CloseHandle(space2);
    return false;
  }
  auto* hdr = static_cast<RingHeader*>(view_p);
  *hdr = {};
  hdr->capacity = ring;
  producer->section_ = section;
  producer->data_ev_ = data_ev;
  producer->space_ev_ = space_ev;
  producer->view_ = view_p;
  consumer->section_ = section2;
  consumer->data_ev_ = data2;
  consumer->space_ev_ = space2;
  consumer->view_ = view_c;
  return true;
}

}  // namespace ipc
}  // namespace base
