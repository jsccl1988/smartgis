#include "legacy/render/rhi2d/impl/gdi/buffer/gdi_bufpool.h"

namespace render {
SmtBufPool::SmtBufPool(u_short nBufCount, u_short nSizePerBuf)
    : m_nBufCount(nBufCount), m_nSizePerBuf(nSizePerBuf) {
  assert(m_nBufCount);
  assert(m_nSizePerBuf);
  m_pBuf = new char[m_nBufCount * m_nSizePerBuf];
  assert(m_pBuf);
  m_bBuf = new byte[m_nBufCount];
  assert(m_bBuf);
  memset(m_bBuf, BufFree, m_nBufCount);

  m_ulPoolSize = m_nBufCount * m_nSizePerBuf * sizeof(char);
}

SmtBufPool::~SmtBufPool(void) {
  SMT_SAFE_DELETE_A(m_bBuf);

  SMT_SAFE_DELETE_A(m_pBuf);
}

char* SmtBufPool::NewBuf() {
  char* pRet = NULL;
#ifdef SMT_THREAD_SAFE
  m_cslock.lock();
#endif
  for (int i = 0; i < m_nBufCount; ++i) {
    if (m_bBuf[i] == BufFree) {
      m_bBuf[i] = BufUsed;
      pRet = m_pBuf + m_nSizePerBuf * i;
      break;
    }
  }
#ifdef SMT_THREAD_SAFE
  m_cslock.unlock();
#endif
  return pRet;
}
void SmtBufPool::FreeBuf(char* pBuf) {
  if (!pBuf) return;

#ifdef SMT_THREAD_SAFE
  m_cslock.lock();
#endif
  char* pFree = m_pBuf;
  for (int i = 0; i < m_nBufCount; ++i) {
    if (pFree == pBuf) {
      m_bBuf[i] = BufFree;
      break;
    }
    pFree += m_nSizePerBuf;
  }
#ifdef SMT_THREAD_SAFE
  m_cslock.unlock();
#endif
}
void SmtBufPool::FreeAllBuf() {
#ifdef SMT_THREAD_SAFE
  m_cslock.lock();
#endif
  memset(m_bBuf, BufFree, m_nBufCount);
#ifdef SMT_THREAD_SAFE
  m_cslock.unlock();
#endif
}
}  // namespace render