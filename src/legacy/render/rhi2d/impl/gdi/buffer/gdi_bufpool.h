// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _GDI_BUFPOOL_H
#define _GDI_BUFPOOL_H

#define GDI_USE_BUFPOOL

#include <mutex>

#include "legacy/core/core.h"
using namespace base;
namespace render {
enum {
  BufFree = 0,
  BufUsed = 1
};

const unsigned short BUF_COUNT = 64;
const unsigned short BUF_SIZE = 1024 * 4;  // 4k
class SmtBufPool {
 public:
  SmtBufPool(unsigned short nBufCount = BUF_COUNT,
             unsigned short nSizePerBuf = BUF_SIZE);

  ~SmtBufPool(void);

  char *NewBuf();

  void FreeBuf(char *pBuf);

  void FreeAllBuf();

  u_short GetSizePerBuf() const { return m_nSizePerBuf; }

  u_short GetBufCount() const { return m_nBufCount; };

  u_long GetPoolSize() const { return m_ulPoolSize; }

 private:
  u_short m_nBufCount;
  u_short m_nSizePerBuf;
  u_long m_ulPoolSize;
  char *m_pBuf;
  PBYTE m_bBuf;
#ifdef SMT_THREAD_SAFE
  std::mutex m_cslock;
#endif
};
}  // namespace render

#endif  //_GDI_BUFPOOL_H
