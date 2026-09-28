// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/buffer/indexbuffer.h"

namespace render {

SmtD3DIndexBuffer::SmtD3DIndexBuffer(int count)
    : locked_(false),
      index_count_(static_cast<ulong>(count)),
      stride_(sizeof(uint)),
      cur_index_(nullptr),
      indices_(new uint[static_cast<ulong>(count)]) {}

SmtD3DIndexBuffer::~SmtD3DIndexBuffer() { delete[] indices_; }

long SmtD3DIndexBuffer::Lock() {
  locked_ = true;
  cur_index_ = indices_;
  return SMT_ERR_NONE;
}

long SmtD3DIndexBuffer::Unlock() {
  locked_ = false;
  cur_index_ = nullptr;
  return SMT_ERR_NONE;
}

void* SmtD3DIndexBuffer::GetIndexData() { return indices_; }

void SmtD3DIndexBuffer::Index(uint index) {
  *cur_index_ = index;
  ++cur_index_;
}

long SmtD3DIndexBuffer::PrepareForDrawing() { return SMT_ERR_NONE; }

long SmtD3DIndexBuffer::EndDrawing() { return SMT_ERR_NONE; }

}  // namespace render
