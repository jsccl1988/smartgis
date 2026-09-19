#include "legacy/render/gdi/gdi_bufpool.h"

namespace render {
SmtBufPool::SmtBufPool(u_short nBufCount, u_short nSizePerBuf)
    : m_nBufCount(nBufCount), m_nSizePerBuf(nSizePerBuf) {
  // �жϲ����Ƿ�Ϸ�
  assert(m_nBufCount);
  assert(m_nSizePerBuf);
  // �����ڴ�
  m_pBuf = new char[m_nBufCount * m_nSizePerBuf];
  assert(m_pBuf);
  // �����ڴ��ռ�ö���
  m_bBuf = new byte[m_nBufCount];
  assert(m_bBuf);
  // ��ʼ���ڴ��ռ�ö���
  memset(m_bBuf, BufFree, m_nBufCount);

  m_ulPoolSize = m_nBufCount * m_nSizePerBuf * sizeof(char);
}

SmtBufPool::~SmtBufPool(void) {
  // �ͷŻ���
  SMT_SAFE_DELETE_A(m_bBuf);

  // �ͷŻ���ռ�ö���
  SMT_SAFE_DELETE_A(m_pBuf);
}

//////////////////////////////////////////////////////////////////////////
// �������һ��buf
// ��������
// ����ֵ������ɹ�������ָ������bufָ�룬���򷵻�NULL
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
//////////////////////////////////////////////////////////////////////////
// �ͷ�һ��buf
// ������buf��ָ��
// ����ֵ����
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
//////////////////////////////////////////////////////////////////////////
// �ͷ�����buf
// ��������
// ����ֵ����
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