// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/impl/gl/resource/buffer/index_buffer.h"

#include "base/memory/arena.h"

namespace scenic {
namespace detail {
namespace {

uint* alloc_indices(ulong count) {
  if (count == 0) {
    return nullptr;
  }
  return static_cast<uint*>(base::allocate(sizeof(uint) * count));
}

void free_indices(uint* p, ulong count) {
  if (!p || count == 0) {
    return;
  }
  base::deallocate(p, sizeof(uint) * count);
}

}  // namespace

GlIndexBuffer::GlIndexBuffer(int count) : IndexBuffer() {
  m_dwIndexCount = count;
  m_bLocked = false;
  m_pIndex = nullptr;
  m_dwStrideIndex = sizeof(uint);
  m_pGLIndex = alloc_indices(m_dwIndexCount);
}

GlIndexBuffer::~GlIndexBuffer() {
  free_indices(m_pGLIndex, m_dwIndexCount);
  m_pGLIndex = nullptr;
}

long GlIndexBuffer::Lock() {
  m_bLocked = true;
  m_pIndex = m_pGLIndex;
  return kErrNone;
}

long GlIndexBuffer::Unlock() {
  m_bLocked = false;
  m_pIndex = nullptr;
  return kErrNone;
}

void* GlIndexBuffer::GetIndexData() {
  return m_pGLIndex;
}

void GlIndexBuffer::Index(uint index) {
  *m_pIndex = index;
  m_pIndex++;
}

long GlIndexBuffer::PrepareForDrawing() {
  if (m_pGLIndex) {
    glEnableClientState(GL_INDEX_ARRAY);
    glIndexPointer(m_dwIndexCount, GL_UNSIGNED_INT, m_pGLIndex);
  } else {
    glDisableClientState(GL_INDEX_ARRAY);
  }
  return kErrNone;
}

long GlIndexBuffer::EndDrawing() {
  if (m_pGLIndex) {
    glDisableClientState(GL_INDEX_ARRAY);
  }
  return kErrNone;
}
}  // namespace detail
}  // namespace scenic
