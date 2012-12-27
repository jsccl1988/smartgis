#include "ml3d_api.h"

Vector4 GalcTriangleNormal( Vector4 &v1 ,Vector4 &v2, Vector4 &v3 )
{
	Vector4 normal,v12((v1 - v2)),v13((v1 - v3));
	normal = v12.CrossProduct(v13);
	normal.Normalize();
	return normal;
}