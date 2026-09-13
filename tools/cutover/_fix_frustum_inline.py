# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Make SmtFrustum a header-only value type (fix LNK2005)."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
base = ROOT / "src/render/render3d/base.h"
text = base.read_text(encoding="utf-8", errors="replace")

start = text.find("\tclass RENDER3D_EXPORT_CLASS SmtFrustum")
if start < 0:
    start = text.find("\tclass SmtFrustum")
if start < 0:
    raise SystemExit("SmtFrustum class not found")

end = text.find("\t};", start)
if end < 0:
    raise SystemExit("class end not found")
end = end + len("\t};")

new = r"""	// Value-type frustum (not DLL-exported) to avoid LNK2005 on implicit
	// copy/assign across render3d / scene3d boundaries.
	class SmtFrustum
	{
	public:
		SmtFrustum(void) = default;
		SmtFrustum(const SmtFrustum& other) {
			memcpy(m_frustum, other.m_frustum, sizeof(m_frustum));
		}
		SmtFrustum& operator=(const SmtFrustum& other) {
			if (this != &other) {
				memcpy(m_frustum, other.m_frustum, sizeof(m_frustum));
			}
			return *this;
		}
		~SmtFrustum(void) = default;

		void GetFrustum(float frustum[6][4]) {
			memcpy(frustum, m_frustum, sizeof(m_frustum));
		}
		void SetFrustum(float frustum[6][4]) {
			memcpy(m_frustum, frustum, sizeof(m_frustum));
		}

		bool IsPointIn(float x, float y, float z) {
			for (int i = 0; i < 6; i++) {
				if (m_frustum[i][P_A] * x + m_frustum[i][P_B] * y +
				    m_frustum[i][P_C] * z + m_frustum[i][P_D] <= 0) {
					return false;
				}
			}
			return true;
		}

		bool IsSphereIn(float x, float y, float z, float radius) {
			for (int i = 0; i < 6; i++) {
				if (m_frustum[i][P_A] * x + m_frustum[i][P_B] * y +
				    m_frustum[i][P_C] * z + m_frustum[i][P_D] <= -radius) {
					return false;
				}
			}
			return true;
		}

		bool IsCubeIn(float x, float y, float z, float size) {
			for (int i = 0; i < 6; i++) {
				if (m_frustum[i][P_A] * (x - size) + m_frustum[i][P_B] * (y - size) + m_frustum[i][P_C] * (z - size) + m_frustum[i][P_D] >= 0) continue;
				if (m_frustum[i][P_A] * (x + size) + m_frustum[i][P_B] * (y - size) + m_frustum[i][P_C] * (z - size) + m_frustum[i][P_D] >= 0) continue;
				if (m_frustum[i][P_A] * (x - size) + m_frustum[i][P_B] * (y + size) + m_frustum[i][P_C] * (z - size) + m_frustum[i][P_D] >= 0) continue;
				if (m_frustum[i][P_A] * (x + size) + m_frustum[i][P_B] * (y + size) + m_frustum[i][P_C] * (z - size) + m_frustum[i][P_D] >= 0) continue;
				if (m_frustum[i][P_A] * (x - size) + m_frustum[i][P_B] * (y - size) + m_frustum[i][P_C] * (z + size) + m_frustum[i][P_D] >= 0) continue;
				if (m_frustum[i][P_A] * (x + size) + m_frustum[i][P_B] * (y - size) + m_frustum[i][P_C] * (z + size) + m_frustum[i][P_D] >= 0) continue;
				if (m_frustum[i][P_A] * (x - size) + m_frustum[i][P_B] * (y + size) + m_frustum[i][P_C] * (z + size) + m_frustum[i][P_D] >= 0) continue;
				if (m_frustum[i][P_A] * (x + size) + m_frustum[i][P_B] * (y + size) + m_frustum[i][P_C] * (z + size) + m_frustum[i][P_D] >= 0) continue;
				return false;
			}
			return true;
		}

		bool IsCuboidIn(float x, float y, float z, SIZE size) {
			for (int i = 0; i < 6; i++) {
				if (m_frustum[i][P_A] * (x - size.cx) + m_frustum[i][P_B] * (y - size.cy) + m_frustum[i][P_C] * (z - size.cx) + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * (x + size.cx) + m_frustum[i][P_B] * (y - size.cy) + m_frustum[i][P_C] * (z - size.cx) + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * (x - size.cx) + m_frustum[i][P_B] * (y + size.cy) + m_frustum[i][P_C] * (z - size.cx) + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * (x + size.cx) + m_frustum[i][P_B] * (y + size.cy) + m_frustum[i][P_C] * (z - size.cx) + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * (x - size.cx) + m_frustum[i][P_B] * (y - size.cy) + m_frustum[i][P_C] * (z + size.cx) + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * (x + size.cx) + m_frustum[i][P_B] * (y - size.cy) + m_frustum[i][P_C] * (z + size.cx) + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * (x - size.cx) + m_frustum[i][P_B] * (y + size.cy) + m_frustum[i][P_C] * (z + size.cx) + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * (x + size.cx) + m_frustum[i][P_B] * (y + size.cy) + m_frustum[i][P_C] * (z + size.cx) + m_frustum[i][P_D] > 0) continue;
				return false;
			}
			return true;
		}

		bool IsBoxIn(float max_x, float max_y, float max_z, float min_x, float min_y, float min_z) {
			for (int i = 0; i < 6; i++) {
				if (m_frustum[i][P_A] * min_x + m_frustum[i][P_B] * min_y + m_frustum[i][P_C] * min_z + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * max_x + m_frustum[i][P_B] * min_y + m_frustum[i][P_C] * min_z + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * min_x + m_frustum[i][P_B] * max_y + m_frustum[i][P_C] * min_z + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * max_x + m_frustum[i][P_B] * max_y + m_frustum[i][P_C] * min_z + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * min_x + m_frustum[i][P_B] * min_y + m_frustum[i][P_C] * max_z + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * max_x + m_frustum[i][P_B] * min_y + m_frustum[i][P_C] * max_z + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * min_x + m_frustum[i][P_B] * max_y + m_frustum[i][P_C] * max_z + m_frustum[i][P_D] > 0) continue;
				if (m_frustum[i][P_A] * max_x + m_frustum[i][P_B] * max_y + m_frustum[i][P_C] * max_z + m_frustum[i][P_D] > 0) continue;
				return false;
			}
			return true;
		}
		bool IsBoxIn(Vector3 &Max, Vector3 &Min) {
			return IsBoxIn(Max.x, Max.y, Max.z, Min.x, Min.y, Min.z);
		}

	private:
		float m_frustum[6][4]{};
	};"""

base.write_text(text[:start] + new + text[end:], encoding="utf-8", newline="\n")
print("updated", base.relative_to(ROOT))

gn = ROOT / "src/render/render3d/BUILD.gn"
gnt = gn.read_text(encoding="utf-8")
gnt2 = gnt.replace('    "frustum.cpp",\n', "")
if gnt2 != gnt:
    gn.write_text(gnt2, encoding="utf-8", newline="\n")
    print("removed frustum.cpp from BUILD.gn")

# Leave frustum.cpp as a thin stub so old references don't confuse readers.
stub = ROOT / "src/render/render3d/frustum.cpp"
stub.write_text(
    "// Copyright (c) 2026 The Mogu Authors.\n"
    "// All rights reserved.\n"
    "//\n"
    "// SmtFrustum is header-only in render/render3d/base.h (LNK2005 fix).\n",
    encoding="utf-8",
    newline="\n",
)
print("stubbed frustum.cpp")
