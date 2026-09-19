#include "legacy/render/render3d/base.h"

namespace render {

SmtCombinedCamera::SmtCombinedCamera() {
  Vector3 zero = Vector3(0., 0., 0.);
  Vector3 view = Vector3(0.0, 1.0, 0.5);
  Vector3 up = Vector3(0., 0., 1.);

  m_vEye = zero;
  m_vTarget = view;
  m_vUp = up;
}

SmtCombinedCamera::~SmtCombinedCamera(void) {}

void SmtCombinedCamera::SetCamera(Vector3 &pos, Vector3 &view, Vector3 &up) {
  m_vEye = pos;
  m_vUp = up;
  m_vTarget = view;
}

// move camera
void SmtCombinedCamera::ShiftCamera(float step) {
  Vector3 vCross, vDir(m_vTarget - m_vEye);
  vCross = vDir.cross(m_vUp);
  vCross.normalize();

  m_vEye.x += vCross.x * step;
  m_vEye.z += vCross.z * step;

  m_vTarget.x += vCross.x * step;
  m_vTarget.z += vCross.z * step;
}

void SmtCombinedCamera::ForwardCamera(float step) {
  Vector3 vDir = m_vTarget - m_vEye;
  vDir.normalize();

  m_vEye.x += vDir.x * step;
  m_vEye.z += vDir.z * step;
  m_vTarget.x += vDir.x * step;
  m_vTarget.z += vDir.z * step;
}

void SmtCombinedCamera::RiseCamera(float step) {
  m_vEye.y += m_vUp.y * step;
  m_vTarget.y += m_vUp.y * step;
}

void SmtCombinedCamera::LeanCamera(float angle) {
  Vector3 vDir = m_vTarget - m_vEye;
  vDir.normalize();
  m_vUp.rotate(vDir, angle);
}

void SmtCombinedCamera::MoveCamera(Vector3 &vec) {
  m_vEye += vec;
  m_vTarget += vec;
}

void SmtCombinedCamera::MoveCameraToPos(Vector3 &pos) {
  Vector3 dV = pos - m_vEye;
  MoveCamera(dV);
}

void SmtCombinedCamera::RaiseViewDirection(float angle) {
  Vector3 vCross, vDir(m_vTarget - m_vEye);
  vCross = vDir.cross(m_vUp);
  vCross.normalize();
  vDir.rotate(vCross, angle);
  m_vTarget = m_vEye + vDir;
}

void SmtCombinedCamera::TurnViewDirection(float angle) {
  Vector3 vDir(m_vTarget - m_vEye);
  vDir.rotate(m_vUp, angle);
  m_vTarget = m_vEye + vDir;
}

//////////////////////////////////////////////////////////////////////////
void SmtCombinedCamera::SetSphereCameraMove(long deltX, long deltY) {
  // �Ӿ۽���ָ���ӵ������  OA����
  Vector3 vDir(m_vEye - m_vTarget);

  // ������������뾶
  float radius = vDir.length();
  // ���䵥λ��
  vDir.normalize();
  // ��ǰ������Ϸ�����a����ˣ�����ͶӰ��ˮƽ���ҷ�������u
  Vector3 u = m_vUp.cross(vDir);
  // ���䵥λ��19
  u.normalize();

  // ����������Ϸ�����ͶӰ���ϵ�ͶӰ���� ����ֱ���ϵķ�������v22
  Vector3 v = vDir.cross(u);
  // ���䵥λ��
  v.normalize();

  // ������ĻAB��ͶӰ���϶�Ӧ������ AB����27
  Vector3 m = u * deltX + v * deltY;
  // ����m�����ĳ���
  double len = m.length();
  // ����������
  len /= 15.0;
  if (len > 0.0) {
    // �Ƕ�AOB  ���ȱ�ʾ ����/�뾶
    double x = len / radius;
    // ��AB������λ��39
    m.normalize();
    // ���෴����ת���ӵ㵽C �Ӷ�ʹ�ð�������ƶ�һ�µķ���ת��ģ��42
    x = -1 * x;
    // �����µ����λ�� C
    m_vEye = m_vTarget + (vDir * cos(x) + m * sin(x)) * radius;
    // �����µ�������Ϸ���
    m_vUp = v;
  }
}
}  // namespace render