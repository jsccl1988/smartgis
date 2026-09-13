#include <cmath>
#include <vector>

#include "plugin/orthogrid/curve.h"
#include "plugin/orthogrid/detail/laplace_solver.h"
#include "plugin/orthogrid/detail/polyline.h"
#include "plugin/orthogrid/grid.h"

const double PI = 3.14159265;

using namespace base;

namespace orthogrid {
//////////////////////////////////////////////////////////////////////////
// ��������
long Orthogrid::CreateOrthGrid() {
  if (m_nX == 0 || m_nY == 0) return SMT_ERR_INVALID_PARAM;

  InitialGrid();    // ��ʼ������
  Orhogonal_SOR();  // ������
  SetDiggedRegionInvalid();  // ������ȥ���ڽڵ�����Ϊ��Чֵ
  CalGridOrthogonality();  // ���������������̶�
  SetGridCell();           // ��������Ԫ

  return SMT_ERR_NONE;
}

long Orthogrid::CvtToGrid(SmtGrid &oSmtGrid) {
  oSmtGrid.set_size(m_nY, m_nX);

  dbfPoint *pDataBuf = NULL;
  int nSize = 0;

  m_pNodes->GetElementBuf(pDataBuf, nSize);
  oSmtGrid.set_nodes(pDataBuf, nSize);

  return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
// ��ʼ������
void Orthogrid::InitialGrid(void) {
  float orth = 0.;
  for (int i = 0; i < m_nX; i++)
    for (int j = 0; j < m_nY; j++) m_pOrthogonality->SetElement(orth, j, i);

  // ��ʼ���߽��
  InitialBoudary();
  // ��ʼ���ڲ�����ڵ�
  InitialInternal();
  // ��ʼ���ھ����߽�����
  InitialRegionBoudary();
}

// ��ʼ���߽�
void Orthogrid::InitialBoudary() {
  int nBnds = m_rMainRegion.m_vBnds.size();
  for (int i = 0; i < nBnds; i++) {
    Boundary *pBnd = m_rMainRegion.m_vBnds.at(i);
    SampleBoudary(pBnd);  // �ȼ������߽��
  }
}

// ��ʼ�����߽�
void Orthogrid::InitialRegionBoudary() {
  for (int i = 0; i < m_rRegions.size(); i++) {
    Region *pRegion = m_rRegions.at(i);
    InitialRegionBoudary(pRegion);
  }
}

// ��ʼ�����߽�
void Orthogrid::InitialRegionBoudary(Region *pRegion) {
  int nBnds = pRegion->m_vBnds.size();
  for (int i = 0; i < nBnds; i++) {
    Boundary *pBnd = pRegion->m_vBnds.at(i);
    SampleBoudary(pBnd);  // �ȼ������߽��
  }
}

// �ȼ������߽�����
void Orthogrid::SampleBoudary(Boundary *pBnd) {
  int is = pBnd->start, ie = pBnd->end, ii = pBnd->index, flag = pBnd->flag;
  if (pBnd->can_sample) {
    switch (flag) {
      case 0:
      case 2:
        SampleBoudaryX(pBnd->curvePts, is, ie, ii);
        break;
      case 1:
      case 3:
        SampleBoudaryY(pBnd->curvePts, is, ie, ii);
        break;
      default:
        break;
    }
  } else {
    switch (flag) {
      case 0:
      case 2:
        SetBoudaryX(pBnd->curvePts, is, ie, ii);
        break;
      case 1:
      case 3:
        SetBoudaryY(pBnd->curvePts, is, ie, ii);
        break;
      default:
        break;
    }
  }
}

// �߽�����
void Orthogrid::SampleBoudaryX(vCurvePoints &bnd, int iStart, int iEnd, int J) {
  double dDis = 0 /*�ɵ���*/, AllDis = 0 /*�����ܳ�*/, CountDis = 0 /*�ۼӾ���*/,
         CurrentDis = 0 /*�����ľ���*/;
  int CountIndex = 0 /*���߶κ��ۼ�*/;
  int i = 0;
  int nSize = bnd.size();
  // ʼĩ�㸳ֵ
  m_pNodes->SetElement(bnd[0], J, iStart);
  m_pNodes->SetElement(bnd[nSize - 1], J, iEnd);

  double *disN = new double[nSize - 1];  // nb -1�����ߵľ���

  for (i = 0; i < nSize - 1; i++) {  // ÿ�����ߵľ���,disN[i]��ʾ���Ϊi-i+1�εľ���
    disN[i] = sqrt((bnd[i].x - bnd[i + 1].x) * (bnd[i].x - bnd[i + 1].x) +
                   (bnd[i].y - bnd[i + 1].y) * (bnd[i].y - bnd[i + 1].y));
    AllDis += disN[i];
  }

  int nSection = abs(iEnd - iStart);
  dDis = AllDis / nSection;  // �ɵ���

  dbfPoint P;
  // �ȼ���ɵ�
  if (iEnd > iStart) {
    for (i = iStart + 1; i < iEnd; i++) {
      // ������i�����ľ���
      CurrentDis = (i - iStart) * dDis;

      /*
      if((i-iStart)%2) CurrentDis = (i-iStart-0.5)*dDis;
      else CurrentDis = (i-iStart)*dDis;
      */

      while (CurrentDis > CountDis) {  // �ҵ�������λ�ڵ����߶�CountIndex
        CountDis += disN[CountIndex++];
      }

      // ��ֵ�õ�������
      double t = (CountDis - CurrentDis) / disN[CountIndex - 1];

      P.x = (1 - t) * bnd[CountIndex].x + t * bnd[CountIndex - 1].x;
      P.y = (1 - t) * bnd[CountIndex].y + t * bnd[CountIndex - 1].y;

      m_pNodes->SetElement(P, J, i);
    }
  } else {
    for (i = iStart - 1; i > iEnd; i--) {
      // ������i�����ľ���
      CurrentDis = (iStart - i) * dDis;

      /*
      if((iStart - i)%2) CurrentDis = (iStart - i-0.5)*dDis;
      else CurrentDis = (iStart - i)*dDis;
      */

      while (CurrentDis > CountDis) {  // �ҵ�������λ�ڵ����߶�CountIndex
        CountDis += disN[CountIndex++];
      }

      // ��ֵ�õ�������
      double t = (CountDis - CurrentDis) / disN[CountIndex - 1];

      P.x = (1 - t) * bnd[CountIndex].x + t * bnd[CountIndex - 1].x;
      P.y = (1 - t) * bnd[CountIndex].y + t * bnd[CountIndex - 1].y;

      m_pNodes->SetElement(P, J, i);
    }
  }

  delete[] disN;
}

// �߽�����
void Orthogrid::SampleBoudaryY(vCurvePoints &bnd, int jStart, int jEnd, int I) {
  double dDis = 0 /*�ɵ���*/, AllDis = 0 /*�����ܳ�*/, CountDis = 0 /*�ۼӾ���*/,
         CurrentDis = 0 /*�����ľ���*/;
  int CountIndex = 0 /*���߶κ��ۼ�*/;
  int i = 0;
  int nSize = bnd.size();
  // ʼĩ�㸳ֵ
  m_pNodes->SetElement(bnd[0], jStart, I);
  m_pNodes->SetElement(bnd[nSize - 1], jEnd, I);

  double *disN = new double[nSize - 1];  // nb -1�����ߵľ���

  for (i = 0; i < nSize - 1; i++) {  // ÿ�����ߵľ���,disN[i]��ʾ���Ϊi-i+1�εľ���
    disN[i] = sqrt((bnd[i].x - bnd[i + 1].x) * (bnd[i].x - bnd[i + 1].x) +
                   (bnd[i].y - bnd[i + 1].y) * (bnd[i].y - bnd[i + 1].y));
    AllDis += disN[i];
  }

  int nSection = abs(jEnd - jStart);
  dDis = AllDis / nSection;  // �ɵ���

  dbfPoint P;
  // �ȼ���ɵ�
  if (jEnd > jStart) {
    for (int j = jStart + 1; j < jEnd; j++) {
      // ������i�����ľ���
      CurrentDis = j * dDis;
      /*
      if((j-jStart)%2) CurrentDis = (j-jStart-0.5)*dDis;
      else CurrentDis = (j-jStart)*dDis;
      */

      while (CurrentDis > CountDis) {  // �ҵ�������λ�ڵ����߶�CountIndex
        CountDis += disN[CountIndex++];
      }

      // ��ֵ�õ�������
      double t = (CountDis - CurrentDis) / disN[CountIndex - 1];

      P.x = (1 - t) * bnd[CountIndex].x + t * bnd[CountIndex - 1].x;
      P.y = (1 - t) * bnd[CountIndex].y + t * bnd[CountIndex - 1].y;

      m_pNodes->SetElement(P, j, I);
    }
  } else {
    for (int j = jStart - 1; j > jEnd; j--) {
      // ������i�����ľ���
      CurrentDis = (jStart - j) * dDis;
      /*
      if((jStart - j)%2) CurrentDis = (jStart - j-0.5)*dDis;
      else CurrentDis = (jStart - j)*dDis;
      */
      while (CurrentDis > CountDis) {  // �ҵ�������λ�ڵ����߶�CountIndex
        CountDis += disN[CountIndex++];
      }

      // ��ֵ�õ�������
      double t = (CountDis - CurrentDis) / disN[CountIndex - 1];

      P.x = (1 - t) * bnd[CountIndex].x + t * bnd[CountIndex - 1].x;
      P.y = (1 - t) * bnd[CountIndex].y + t * bnd[CountIndex - 1].y;

      m_pNodes->SetElement(P, j, I);
    }
  }

  delete[] disN;
}

// ��ʼ������X�߽�����
void Orthogrid::SetBoudaryX(vCurvePoints &bnd, int iStart, int iEnd, int J) {
  int i = 0;
  if (iEnd > iStart) {
    for (i = iStart; i <= iEnd; i++) {
      m_pNodes->SetElement(bnd[i - iStart], J, i);
    }
  } else {
    for (i = iStart; i >= iEnd; i--) {
      m_pNodes->SetElement(bnd[iStart - i], J, i);
    }
  }
}

// ��ʼ������Y�߽�����
void Orthogrid::SetBoudaryY(vCurvePoints &bnd, int jStart, int jEnd, int I) {
  if (jEnd > jStart) {
    for (int j = jStart; j <= jEnd; j++) {
      m_pNodes->SetElement(bnd[j - jStart], j, I);
    }
  } else {
    for (int j = jStart; j >= jEnd; j--) {
      m_pNodes->SetElement(bnd[jStart - j], j, I);
    }
  }
}

//////////////////////////////////////////////////////////////////////////
// ��ʼ���ڲ�����ڵ㣬˫���Բ�ֵ
void Orthogrid::InitialInternal() {
  int i, j;
  dbfPoint P, P0, P1;
  // �Ӵ��ڲ��ڵ�߽�
  for (j = 1; j < m_nY - 1; j++) {
    for (i = 1; i < m_nX - 1; i++) {
      P0 = m_pNodes->GetElement(0, i);
      P1 = m_pNodes->GetElement(j, m_nX - 1);
      P.x = P0.x + (P1.x - P0.x) * i / (m_nX - 1);
      P.y = P0.y + (P1.y - P0.y) * i / (m_nX - 1);
      m_pNodes->SetElement(P, j, i);
    }
  }
  // �Ӵ��ڲ��ڵ�߽�
  for (i = 1; i < m_nX - 1; i++) {
    for (j = 1; j < m_nY - 1; j++) {
      P0 = m_pNodes->GetElement(0, i);
      P1 = m_pNodes->GetElement(m_nY - 1, i);
      P.x = P0.x + (P1.x - P0.x) * j / (m_nY - 1);
      P.y = P0.y + (P1.y - P0.y) * j / (m_nY - 1);
      m_pNodes->SetElement(P, j, i);
    }
  }
}

//////////////////////////////////////////////////////////////////////////
// Interior nodes: assemble Thompson elliptic (P=Q=0) and solve with Eigen.
void Orthogrid::Orhogonal_SOR(void) {
  if (m_nX < 3 || m_nY < 3 || m_pNodes == nullptr) {
    return;
  }

  const int n = m_nX * m_nY;
  std::vector<double> xs(static_cast<size_t>(n));
  std::vector<double> ys(static_cast<size_t>(n));
  std::vector<std::uint8_t> unknown(static_cast<size_t>(n), 0);

  auto pack_nodes = [&]() {
    for (int j = 0; j < m_nY; ++j) {
      for (int i = 0; i < m_nX; ++i) {
        const dbfPoint p = m_pNodes->GetElement(j, i);
        const int k = j * m_nX + i;
        xs[static_cast<size_t>(k)] = p.x;
        ys[static_cast<size_t>(k)] = p.y;
        const bool interior = (i > 0 && i < m_nX - 1 && j > 0 && j < m_nY - 1);
        unknown[static_cast<size_t>(k)] =
            (interior && !IsOnDiggedRegion(i, j)) ? 1 : 0;
      }
    }
  };

  auto unpack_nodes = [&]() {
    for (int j = 0; j < m_nY; ++j) {
      for (int i = 0; i < m_nX; ++i) {
        const int k = j * m_nX + i;
        dbfPoint p = m_pNodes->GetElement(j, i);
        p.x = xs[static_cast<size_t>(k)];
        p.y = ys[static_cast<size_t>(k)];
        m_pNodes->SetElement(p, j, i);
      }
    }
  };

  // Frozen-coefficient outer sweeps; slide boundary for orthogonality.
  constexpr int kOuterSweeps = 8;
  for (int sweep = 0; sweep < kOuterSweeps; ++sweep) {
    pack_nodes();
    orthogrid::GridField field{m_nX, m_nY, xs.data(), ys.data()};
    if (!orthogrid::solve_elliptic_step(field, unknown.data())) {
      break;
    }
    unpack_nodes();
    SlideBoudary();
  }
}

// �������Ƿ����ھ���
bool Orthogrid::IsOnDiggedRegion(int ii, int jj) {
  int i = 0;
  bool flag = false;
  while (!flag && i < m_rRegions.size()) {
    Region *pRegion = m_rRegions.at(i);
    flag = pRegion->HitTestOn(ii, jj);

    if (!flag) flag = pRegion->HitTestIn(ii, jj);

    i++;
  }
  return flag;
}

// �������Ƿ����ھ����ڲ�
bool Orthogrid::IsInDiggedRegion(int ii, int jj) {
  int i = 0;
  bool flag = false;
  while (!flag && i < m_rRegions.size()) {
    Region *pRegion = m_rRegions.at(i);
    flag = pRegion->HitTestIn(ii, jj);
    if (!flag) {
      if (pRegion->HitTestOn(ii, jj))
        if (ii == m_nX - 1 || ii == 0 || jj == m_nY - 1 || jj == 0)
          if (!pRegion->HitTestOnCorner(ii, jj)) flag = true;
    }
    i++;
  }
  return flag;
}

//////////////////////////////////////////////////////////////////////////
// �����߽磬ʹ�ñ߽籣������
void Orthogrid::SlideBoudary() {
  int nBnds = m_rMainRegion.m_vBnds.size();
  for (int i = 0; i < nBnds; i++) {
    Boundary *pBnd = m_rMainRegion.m_vBnds.at(i);
    if (pBnd->can_slide) SlideBoudary(pBnd);  // �����߽磬ʹ�߽籣������
  }

  SlideRegionBoudary();
}

// �������߽�
void Orthogrid::SlideRegionBoudary(void) {
  for (int i = 0; i < m_rRegions.size(); i++) {
    Region *pRegion = m_rRegions.at(i);
    SlideRegionBoudary(pRegion);
    AjustRegionCornerNode(pRegion);
  }
}

// �������߽�
void Orthogrid::SlideRegionBoudary(Region *pRegion) {
  int nBnds = pRegion->m_vBnds.size();
  for (int i = 0; i < nBnds; i++) {
    Boundary *pBnd = pRegion->m_vBnds.at(i);
    if (pBnd->can_slide) SlideBoudary(pBnd);  // �����߽磬ʹ�߽籣������
  }
}

// �������ǵ�
void Orthogrid::AjustRegionCornerNode(Region *pRegion) {
  int nBnds = pRegion->m_vBnds.size();
  int ii, jj;
  for (int i = 0; i < nBnds; i++) {
    Boundary *pBnd = pRegion->m_vBnds.at(i);
    switch (pBnd->flag) {
      case 0:
      case 2:
        ii = pBnd->start;
        jj = pBnd->index;
        break;
      case 1:
      case 3:
        ii = pBnd->index;
        jj = pBnd->start;
        break;
    }

    AjustRegionCornerNode(ii, jj);
  }
}

// �������ǵ�
void Orthogrid::AjustRegionCornerNode(int i, int j) {
  dbfPoint Pw, Ps, Pe, Pn, P;
  Pe = m_pNodes->GetElement(j, i + 1);
  Ps = m_pNodes->GetElement(j - 1, i);
  Pw = m_pNodes->GetElement(j, i - 1);
  Pn = m_pNodes->GetElement(j + 1, i);

  P.x = (Pe.x + Ps.x + Pw.x + Pn.x) / 4.;
  P.y = (Pe.y + Ps.y + Pw.y + Pn.y) / 4.;

  m_pNodes->SetElement(P, j, i);
}

// �����߽�
void Orthogrid::SlideBoudary(Boundary *pBnd) {
  int is = pBnd->start, ie = pBnd->end, ii = pBnd->index, flag = pBnd->flag;
  switch (flag) {
    case 0:
    case 2:
      SlideBoudaryX(pBnd->curvePts, is, ie, ii);
      break;
    case 1:
    case 3:
      SlideBoudaryY(pBnd->curvePts, is, ie, ii);
      break;
    default:
      break;
  }
}

// �����߽� X����
void Orthogrid::SlideBoudaryX(vCurvePoints &bnd, int iStart, int iEnd, int J) {
  int i;
  int pre = 0, next = pre + 1;

  if (iEnd > iStart) {
    if (J > m_nY - 2) return;

    for (i = iStart + 1; i < iEnd; i++) {
      pre = 0, next = pre + 1;
      dbfPoint A, B, P, H;
      int flag = 1;

      P = m_pNodes->GetElement(J, i);

      // �ҵ�Pλ�ڵı߽��߶�
      orthogrid::detail::locate_on_polyline(P, bnd, pre, next);
      if (next >= bnd.size()) continue;

      A = bnd[pre];
      B = bnd[next];
      P = m_pNodes->GetElement(J + 1, i);

      // ����P1����ֱ���ҵ��ı߽��߶εĴ��ߣ�����ΪH��flag���H�Ƿ�λ���߶���
      flag = orthogrid::detail::foot_on_segment(A, B, P, H);

      if (flag == 1) {  // AB�ӳ����ϣ�������ֱ����һ�����߶εĴ��ߣ���ô���H��Ϊ�߽�������ֵ
        pre++;
        next++;

        if (next >= bnd.size()) continue;

        A = bnd[pre];
        B = bnd[next];

        flag = orthogrid::detail::foot_on_segment(A, B, P, H);

        if (flag == -1) {  // ��thta�Ƿ�Χ��,������һ�����߶���Ҳ�Ҳ�������
          continue;
        }
      }

      m_pNodes->SetElement(H, J, i);
    }
  } else {
    if (J < 1) return;

    for (i = iStart - 1; i > iEnd; i--) {
      pre = 0, next = pre + 1;
      dbfPoint A, B, P, H;
      int flag = 1;

      P = m_pNodes->GetElement(J, i);

      // �ҵ�Pλ�ڵı߽��߶�
      orthogrid::detail::locate_on_polyline(P, bnd, pre, next);
      if (next >= bnd.size()) continue;

      A = bnd[pre];
      B = bnd[next];
      P = m_pNodes->GetElement(J - 1, i);

      // ����P1����ֱ���ҵ��ı߽��߶εĴ��ߣ�����ΪH��flag���H�Ƿ�λ���߶���
      flag = orthogrid::detail::foot_on_segment(A, B, P, H);

      if (flag == 1) {  // AB�ӳ����ϣ�������ֱ����һ�����߶εĴ��ߣ���ô���H��Ϊ�߽�������ֵ
        pre++;
        next++;

        if (next >= bnd.size()) continue;

        A = bnd[pre];
        B = bnd[next];

        flag = orthogrid::detail::foot_on_segment(A, B, P, H);

        if (flag == -1) {  // ��thta�Ƿ�Χ��,������һ�����߶���Ҳ�Ҳ�������
          continue;
        }
      }

      m_pNodes->SetElement(H, J, i);
    }
  }
}

// �����߽� Y����
void Orthogrid::SlideBoudaryY(vCurvePoints &bnd, int jStart, int jEnd, int I) {
  int j;
  int pre = 0, next = pre + 1;

  if (jEnd > jStart) {
    if (I < 1) return;

    for (j = jStart + 1; j < jEnd; j++) {
      pre = 0, next = pre + 1;
      dbfPoint A, B, P, H;
      int flag = 1;

      P = m_pNodes->GetElement(j, I);

      // �ҵ�Pλ�ڵı߽��߶�
      orthogrid::detail::locate_on_polyline(P, bnd, pre, next);
      if (next >= bnd.size()) continue;

      A = bnd[pre];
      B = bnd[next];
      P = m_pNodes->GetElement(j, I - 1);

      // ����P1����ֱ���ҵ��ı߽��߶εĴ��ߣ�����ΪH��flag���H�Ƿ�λ���߶���
      flag = orthogrid::detail::foot_on_segment(A, B, P, H);

      if (flag == 1) {  // AB�ӳ����ϣ�������ֱ����һ�����߶εĴ��ߣ���ô���H��Ϊ�߽�������ֵ
        pre++;
        next++;

        if (next >= bnd.size()) continue;

        A = bnd[pre];
        B = bnd[next];

        flag = orthogrid::detail::foot_on_segment(A, B, P, H);

        if (flag == -1) {  // ��thta�Ƿ�Χ��,������һ�����߶���Ҳ�Ҳ�������
          continue;
        }
      }

      m_pNodes->SetElement(H, j, I);
    }
  } else {
    if (I > m_nX - 2) return;

    for (j = jStart - 1; j > jEnd; j--) {
      pre = 0, next = pre + 1;
      dbfPoint A, B, P, H;
      int flag = 1;

      P = m_pNodes->GetElement(j, I);

      // �ҵ�Pλ�ڵı߽��߶�
      orthogrid::detail::locate_on_polyline(P, bnd, pre, next);
      if (next >= bnd.size()) continue;

      A = bnd[pre];
      B = bnd[next];
      P = m_pNodes->GetElement(j, I + 1);

      // ����P1����ֱ���ҵ��ı߽��߶εĴ��ߣ�����ΪH��flag���H�Ƿ�λ���߶���
      flag = orthogrid::detail::foot_on_segment(A, B, P, H);

      if (flag == 1) {  // AB�ӳ����ϣ�������ֱ����һ�����߶εĴ��ߣ���ô���H��Ϊ�߽�������ֵ
        pre++;
        next++;

        if (next >= bnd.size()) continue;

        A = bnd[pre];
        B = bnd[next];

        flag = orthogrid::detail::foot_on_segment(A, B, P, H);

        if (flag == -1) {  // ��thta�Ƿ�Χ��,������һ�����߶���Ҳ�Ҳ�������
          continue;
        }
      }

      m_pNodes->SetElement(H, j, I);
    }
  }
}

//////////////////////////////////////////////////////////////////////////
// �����������̶�
void Orthogrid::CalGridOrthogonality() {
  dbfPoint Pw, Ps, Pe, Pn;
  float xks, xat, yks, yat, arfa, beta, gama;
  float ftheta = 0., fdelta = 0.;

  int i, j;
  for (i = 1; i < m_nX - 1; i++) {
    for (j = 1; j < m_nY - 1; j++) {
      if (IsOnDiggedRegion(i, j)) continue;
      Pe = m_pNodes->GetElement(j, i + 1);
      Ps = m_pNodes->GetElement(j - 1, i);
      Pw = m_pNodes->GetElement(j, i - 1);
      Pn = m_pNodes->GetElement(j + 1, i);

      xks = (Pe.x - Pw.x) / 2.;
      xat = (Pn.x - Ps.x) / 2.;

      yks = (Pe.y - Pw.y) / 2.;
      yat = (Pn.y - Ps.y) / 2.;

      arfa = xat * xat + yat * yat;
      gama = xks * xks + yks * yks;
      beta = xks * xat + yks * yat;

      ftheta = acos(beta / sqrt(arfa * gama)) * 180. / PI;
      fdelta = fabs(90 - ftheta);

      m_pOrthogonality->SetElement(fdelta, j, i);
    }
  }
}

void Orthogrid::AddOrth(int i, int j, float add) {
  float orth = m_pOrthogonality->GetElement(j, i);
  orth += add;
  m_pOrthogonality->SetElement(orth, j, i);
}

//////////////////////////////////////////////////////////////////////////
// ��������Ԫ
void Orthogrid::SetGridCell() {
  int i, j;
  //////////////////////////////////////////////////////////////////////////
  float orth;
  dbfPoint A, B, C, D;
  GridCell cell;
  for (i = 0; i < m_nX - 1; i++) {
    for (j = 0; j < m_nY - 1; j++) {
      A = m_pNodes->GetElement(j, i);
      B = m_pNodes->GetElement(j, i + 1);
      C = m_pNodes->GetElement(j + 1, i + 1);
      D = m_pNodes->GetElement(j + 1, i);

      cell = m_pCells->GetElement(j, i);

      if (InValid(A) || InValid(B) || InValid(C) || InValid(D)) {
        cell.IsSelected = false;
        cell.P.x = fInvalidNum;
        cell.P.y = fInvalidNum;
        m_pCells->SetElement(cell, j, i);
        continue;
      }

      cell.IsSelected = true;
      cell.P.x = (A.x + B.x + C.x + D.x) / 4;
      cell.P.y = (A.y + B.y + C.y + D.y) / 4;

      orth = m_pOrthogonality->GetElement(j, i);
      orth += m_pOrthogonality->GetElement(j, i + 1);
      orth += m_pOrthogonality->GetElement(j + 1, i + 1);
      orth += m_pOrthogonality->GetElement(j + 1, i);

      m_pCells->SetElement(cell, j, i);
    }
  }
}

void Orthogrid::SetDiggedRegionInvalid(void) {
  dbfPoint P;
  P.x = fInvalidNum;
  P.y = fInvalidNum;
  for (int i = 0; i < m_nX; i++) {
    for (int j = 0; j < m_nY; j++) {
      if (IsInDiggedRegion(i, j)) m_pNodes->SetElement(P, j, i);
    }
  }

  dbfPoint A, B, C, D;
  GridCell cell;
  for (int i = 0; i < m_nX - 1; i++) {
    for (int j = 0; j < m_nY - 1; j++) {
      A = m_pNodes->GetElement(j, i);
      B = m_pNodes->GetElement(j, i + 1);
      C = m_pNodes->GetElement(j + 1, i + 1);
      D = m_pNodes->GetElement(j + 1, i);

      cell = m_pCells->GetElement(j, i);

      if (InValid(A) || InValid(B) || InValid(C) || InValid(D)) {
        cell.IsSelected = false;
        cell.P.x = fInvalidNum;
        cell.P.y = fInvalidNum;
        m_pCells->SetElement(cell, j, i);
        continue;
      }
    }
  }
}
}  // namespace orthogrid