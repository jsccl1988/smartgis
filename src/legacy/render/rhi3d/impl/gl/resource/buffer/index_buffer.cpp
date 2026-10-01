// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/gl/resource/buffer/index_buffer.h"

#include "base/memory/arena.h"

namespace render {
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

SmtGLIndexBuffer::SmtGLIndexBuffer(int count) : SmtIndexBuffer() {
  m_dwIndexCount = count;
  m_bLocked = false;
  m_pIndex = nullptr;
  m_dwStrideIndex = sizeof(uint);
  m_pGLIndex = alloc_indices(m_dwIndexCount);
}

SmtGLIndexBuffer::~SmtGLIndexBuffer() {
  free_indices(m_pGLIndex, m_dwIndexCount);
  m_pGLIndex = nullptr;
}

long SmtGLIndexBuffer::Lock() {
  m_bLocked = true;
  m_pIndex = m_pGLIndex;
  return SMT_ERR_NONE;
}

long SmtGLIndexBuffer::Unlock() {
  m_bLocked = false;
  m_pIndex = nullptr;
  return SMT_ERR_NONE;
}

void* SmtGLIndexBuffer::GetIndexData() {
  return m_pGLIndex;
}

void SmtGLIndexBuffer::Index(uint index) {
  *m_pIndex = index;
  m_pIndex++;
}

long SmtGLIndexBuffer::PrepareForDrawing() {
  if (m_pGLIndex) {
    glEnableClientState(GL_INDEX_ARRAY);
    glIndexPointer(m_dwIndexCount, GL_UNSIGNED_INT, m_pGLIndex);
  } else {
    glDisableClientState(GL_INDEX_ARRAY);
  }
  return SMT_ERR_NONE;
}

long SmtGLIndexBuffer::EndDrawing() {
  if (m_pGLIndex) {
    glDisableClientState(GL_INDEX_ARRAY);
  }
  return SMT_ERR_NONE;
}
}  // namespace render
