#pragma once

typedef union Float2 Float2;
typedef union Float3 Float3;
union Float2
{
	struct
	{
		float x;
		float y;
	};
	float values[2];
};
union Float3
{
	struct
	{
		float x;
		float y;
		float z;
	};
	float values[3];
};
SerializeSimpleType(Float3);

Float3 float3_from_float(float a)
{
	Float3 r;
	r.x = a;
	r.y = a;
	r.z = a;
	return r;
}

Float3 float3_add(Float3 a, Float3 b)
{
	Float3 r;
	r.x = a.x + b.x;
	r.y = a.y + b.y;
	r.z = a.z + b.z;
	return r;
}

Float3 float3_sub(Float3 a, Float3 b)
{
	Float3 r;
	r.x = a.x - b.x;
	r.y = a.y - b.y;
	r.z = a.z - b.z;
	return r;
}

Float3 float3_mul_scalar(Float3 a, float s)
{
	Float3 r;
	r.x = s*a.x;
	r.y = s*a.y;
	r.z = s*a.z;
	return r;
}

Float3 float3_mul(Float3 a, Float3 b)
{
	Float3 r;
	r.x = a.x*b.x;
	r.y = a.y*b.y;
	r.z = a.z*b.z;
	return r;
}

Float3 float3_normalize(Float3 v)
{
	float norm = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
	return (Float3){v.x / norm, v.y / norm, v.z / norm};
}

float float3_dot(Float3 a, Float3 b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

Float3 float3_cross(Float3 a, Float3 b)
{
	Float3 r;
	r.x = a.y * b.z - b.y * a.z;
	r.y = a.z * b.x - b.z * a.x;
	r.z = a.x * b.y - b.x * a.y;
	return r;
}

float float3_distance(Float3 a, Float3 b)
{
	float x = a.x - b.x;
	float y = a.y - b.y;
	float z = a.z - b.z;
	return sqrtf(x*x + y*y + z*z);
}

float float3_length(Float3 v)
{
	return sqrtf(float3_dot(v, v));
}

float float_lerp(float a, float b, float coef)
{
	return (1.0f - coef) * a + (coef) * b;
}

Float3 float3_lerp(Float3 a, Float3 b, float coef)
{
	Float3 r;
	r.x = float_lerp(a.x, b.x, coef);
	r.y = float_lerp(a.y, b.y, coef);
	r.z = float_lerp(a.z, b.z, coef);
	return r;
}

typedef union Quat Quat;
union Quat
{
	struct
	{
		float x;
		float y;
		float z;
		float w;
	};
	float values[4];
};
SerializeSimpleType(Quat);


Quat quat_normalize(Quat a)
{
	float norm = sqrtf(a.x*a.x+a.y*a.y+a.z*a.z+a.w*a.w);
	Quat r;
	r.x = a.x / norm;
	r.y = a.y / norm;
	r.z = a.z / norm;
	return r;
}

Quat quat_slerp(Quat a, Quat b, float coef) 
{
	float dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
	if (dot < 0.0) {
		dot = -dot;
		b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w;
	}
	float omega = acosf(fmin(fmax(dot, 0.0), 1.0));
	if (omega <= 1.175494351e-38f) return a;
	float rcp_so = 1.0 / sinf(omega);
	float af = sinf((1.0 - coef) * omega) * rcp_so;
	float bf = sinf(coef * omega) * rcp_so;

	float x = af*a.x + bf*b.x;
	float y = af*a.y + bf*b.y;
	float z = af*a.z + bf*b.z;
	float w = af*a.w + bf*b.w;
	float rcp_len = 1.0 / sqrtf(x*x + y*y + z*z + w*w);

	Quat ret;
	ret.x = x * rcp_len;
	ret.y = y * rcp_len;
	ret.z = z * rcp_len;
	ret.w = w * rcp_len;
	return ret;
}

struct FloatList
{
	float *data;
	uint32_t length;
};

void Serialize_FloatList(Serializer *serializer, struct FloatList *value)
{
	SV_ADD(SV_INITIAL, uint32_t, length);
	if (serializer->is_reading) {
		value->data = calloc(1, value->length * sizeof(float));
	}
	SerializeBytes(serializer, value->data, value->length * sizeof(float));
}

struct Float3List
{
	Float3 *data;
	uint32_t length;
};


void Serialize_Float3List(Serializer *serializer, struct Float3List *value)
{
	SV_ADD(SV_INITIAL, uint32_t, length);
	if (serializer->is_reading) {
		value->data = calloc(1, value->length * sizeof(Float3));
	}
	SerializeBytes(serializer, value->data, value->length * sizeof(Float3));
}

struct QuatList
{
	Quat *data;
	uint32_t length;
};

void Serialize_QuatList(Serializer *serializer, struct QuatList *value)
{
	SV_ADD(SV_INITIAL, uint32_t, length);
	if (serializer->is_reading) {
		value->data = calloc(1, value->length * sizeof(Quat));
	}
	SerializeBytes(serializer, value->data, value->length * sizeof(Quat));
}

typedef struct Float4x4 Float4x4;
struct Float4x4
{
	float values[16];
};
SerializeSimpleType(Float4x4);


typedef struct Float3x4 Float3x4;
struct Float3x4
{
	union
	{
		float values[12];
		Float3 cols[4];
	};
};
SerializeSimpleType(Float3x4);

// Column major
#define F44(m, i, j) m.values[(j * 4 + i)]
#define F34(m, i, j) m.values[(j * 3 + i)]

// 0 3 = 0*3+3 = 3  != 9
// 1 3 = 1*3+3 = 4  != 10
// 2 3 = 2*3+3 = 9  != 11

Float4x4 float4x4_mul(Float4x4 a, Float4x4 b)
{
	Float4x4 result = {0};
	for (uint32_t irow = 0; irow < 4; ++irow) {
		for (uint32_t icol = 0; icol < 4; ++icol) {

			for (uint32_t i = 0; i < 4; ++i) {
				F44(result, irow, icol) += F44(a, irow, i) * F44(b, i, icol);
			}
			
		}
	}
	return result;
}

Float4x4 perspective_projection(float vertical_fov, float aspect_ratio, float n, float f, Float4x4 *inverse)
{
    float fov_rad = vertical_fov * 2.0f * 3.14f / 360.0f;
    float focal_length = 1.0f / tanf(fov_rad / 2.0f);

    float x  =  focal_length / aspect_ratio;
    float y  = -focal_length;
    float A  = n / (n-f);
    float B  = -1.0f * f * A;

    Float4x4 p;
    F44(p,0,0) =    x;	F44(p,0,1) = 0.0f;	F44(p,0,2) = 0.0f;	F44(p,0,3) = 0.0f;
    F44(p,1,0) = 0.0f;	F44(p,1,1) = 0.0f;	F44(p,1,2) =    y;	F44(p,1,3) = 0.0f;
    F44(p,2,0) = 0.0f;	F44(p,2,1) =    A;	F44(p,2,2) = 0.0f;	F44(p,2,3) =    B;
    F44(p,3,0) = 0.0f;	F44(p,3,1) = 1.0f;	F44(p,3,2) = 0.0f;	F44(p,3,3) = 0.0f; 

    if (inverse) {
	    Float4x4 ip;
	    F44(ip,0,0) =  1/x;	F44(ip,0,1) = 0.0f;	F44(ip,0,2) =  0.0f;	F44(ip,0,3) = 0.0f;
	    F44(ip,1,0) = 0.0f;	F44(ip,1,1) = 0.0f;	F44(ip,1,2) =  0.0f;	F44(ip,1,3) = 1.0f;
	    F44(ip,2,0) = 0.0f;	F44(ip,2,1) =  1/y;	F44(ip,2,2) =  0.0f;	F44(ip,2,3) = 0.0f;
	    F44(ip,3,0) = 0.0f;	F44(ip,3,1) = 0.0f;	F44(ip,3,2) =   1/B;	F44(ip,3,3) = -A/B;
	    *inverse = ip;
    }

    return p;
}

Float3x4 lookat_view(Float3 from, Float3 to, Float3x4 *inverse)
{
	Float3 UP_AXIS = (Float3){0.0f, 0.0f, 1.0f};
	
	Float3 forward = float3_normalize(float3_sub(to, from));
	Float3 right = float3_normalize(float3_cross(forward, UP_AXIS));
	Float3 up = float3_normalize(float3_cross(right, forward));

	Float3x4 v;
	F34(v,0,0) =   right.x;	F34(v,0,1) =   right.y;	F34(v,0,2) =   right.z;	F34(v,0,3) = -float3_dot(from, right);
	F34(v,1,0) = forward.x;	F34(v,1,1) = forward.y;	F34(v,1,2) = forward.z;	F34(v,1,3) = -float3_dot(from, forward);
	F34(v,2,0) =      up.x;	F34(v,2,1) =      up.y;	F34(v,2,2) =      up.z;	F34(v,2,3) = -float3_dot(from, up);

	if (inverse) {
	    Float3x4 iv;
	    F34(iv,0,0) = right.x;	F34(iv,0,1) = forward.x;	F34(iv,0,2) = up.x;	F34(iv,0,3) = from.x;
	    F34(iv,1,0) = right.y;	F34(iv,1,1) = forward.y;	F34(iv,1,2) = up.y;	F34(iv,1,3) = from.y;
	    F34(iv,2,0) = right.z;	F34(iv,2,1) = forward.z;	F34(iv,2,2) = up.z;	F34(iv,2,3) = from.z;
	    *inverse = iv;
	}
	return v;
}

Float3x4 float3x4_mul(Float3x4 a, Float3x4 b)
{
	Float3x4 result = {0};

	F34(result,0,0) = F34(a,0,0)*F34(b,0,0) + F34(a,0,1)*F34(b,1,0) + F34(a,0,2)*F34(b,2,0);
	F34(result,1,0) = F34(a,1,0)*F34(b,0,0) + F34(a,1,1)*F34(b,1,0) + F34(a,1,2)*F34(b,2,0);
	F34(result,2,0) = F34(a,2,0)*F34(b,0,0) + F34(a,2,1)*F34(b,1,0) + F34(a,2,2)*F34(b,2,0);

	F34(result,0,1) = F34(a,0,0)*F34(b,0,1) + F34(a,0,1)*F34(b,1,1) + F34(a,0,2)*F34(b,2,1);
	F34(result,1,1) = F34(a,1,0)*F34(b,0,1) + F34(a,1,1)*F34(b,1,1) + F34(a,1,2)*F34(b,2,1);
	F34(result,2,1) = F34(a,2,0)*F34(b,0,1) + F34(a,2,1)*F34(b,1,1) + F34(a,2,2)*F34(b,2,1);

	F34(result,0,2) = F34(a,0,0)*F34(b,0,2) + F34(a,0,1)*F34(b,1,2) + F34(a,0,2)*F34(b,2,2);
	F34(result,1,2) = F34(a,1,0)*F34(b,0,2) + F34(a,1,1)*F34(b,1,2) + F34(a,1,2)*F34(b,2,2);
	F34(result,2,2) = F34(a,2,0)*F34(b,0,2) + F34(a,2,1)*F34(b,1,2) + F34(a,2,2)*F34(b,2,2);
	
	F34(result,0,3) = F34(a,0,0)*F34(b,0,3) + F34(a,0,1)*F34(b,1,3) + F34(a,0,2)*F34(b,2,3) + F34(a,0,3);
	F34(result,1,3) = F34(a,1,0)*F34(b,0,3) + F34(a,1,1)*F34(b,1,3) + F34(a,1,2)*F34(b,2,3) + F34(a,1,3);
	F34(result,2,3) = F34(a,2,0)*F34(b,0,3) + F34(a,2,1)*F34(b,1,3) + F34(a,2,2)*F34(b,2,3) + F34(a,2,3);
	
	return result;
}

Float3x4 float3x4_from_transform(Float3 translation, Quat rotation, Float3 scale)
{
	float sx = 2.0f * scale.x, sy = 2.0f * scale.y, sz = 2.0f * scale.z;
	float xx = rotation.x*rotation.x, xy = rotation.x*rotation.y, xz = rotation.x*rotation.z, xw = rotation.x*rotation.w;
	float yy = rotation.y*rotation.y, yz = rotation.y*rotation.z, yw = rotation.y*rotation.w;
	float zz = rotation.z*rotation.z, zw = rotation.z*rotation.w;
	Float3x4 m;
	F34(m,0,0) = sx * (- yy - zz + 0.5f);
	F34(m,1,0) = sx * (+ xy + zw);
	F34(m,2,0) = sx * (- yw + xz);
	F34(m,0,1) = sy * (- zw + xy);
	F34(m,1,1) = sy * (- xx - zz + 0.5f);
	F34(m,2,1) = sy * (+ xw + yz);
	F34(m,0,2) = sz * (+ xz + yw);
	F34(m,1,2) = sz * (- xw + yz);
	F34(m,2,2) = sz * (- xx - yy + 0.5f);
	F34(m,0,3) = translation.x;
	F34(m,1,3) = translation.y;
	F34(m,2,3) = translation.z;
	return m;
}

float float3x4_determinant(Float3x4 m)
{
	return
		- F34(m,0,2)*F34(m,1,1)*F34(m,2,0) + F34(m,0,1)*F34(m,1,2)*F34(m,2,0) + F34(m,0,2)*F34(m,1,0)*F34(m,2,1)
		- F34(m,0,0)*F34(m,1,2)*F34(m,2,1) - F34(m,0,1)*F34(m,1,0)*F34(m,2,2) + F34(m,0,0)*F34(m,1,1)*F34(m,2,2);
}

// float fmax(float a, float b) { return a > b ? a : b; }

void transform_from_float3x4(Float3x4 mat, Float3 *translation, Quat *rotation, Float3 *scale)
{
	float det = float3x4_determinant(mat);

	*translation = mat.cols[3];
	scale->x = float3_length(mat.cols[0]);
	scale->y = float3_length(mat.cols[1]);
	scale->z = float3_length(mat.cols[2]);

	// Flip a single non-zero axis if negative determinant
	float sign_x = 1.0f;
	float sign_y = 1.0f;
	float sign_z = 1.0f;
	if (det < 0.0f) {
		if (scale->x > 0.0f) sign_x = -1.0f;
		else if (scale->y > 0.0f) sign_y = -1.0f;
		else if (scale->z > 0.0f) sign_z = -1.0f;
	}

	Float3 x = float3_mul_scalar(mat.cols[0], scale->x > 0.0f ? sign_x / scale->x : 0.0f);
	Float3 y = float3_mul_scalar(mat.cols[1], scale->y > 0.0f ? sign_y / scale->y : 0.0f);
	Float3 z = float3_mul_scalar(mat.cols[2], scale->z > 0.0f ? sign_z / scale->z : 0.0f);
	float trace = x.x + y.y + z.z;
	if (trace > 0.0f) {
		float a = sqrtf(fmax(0.0, trace + 1.0)), b = (a != 0.0f) ? 0.5f / a : 0.0f;
		rotation->x = (y.z - z.y) * b;
		rotation->y = (z.x - x.z) * b;
		rotation->z = (x.y - y.x) * b;
		rotation->w = 0.5f * a;
	} else if (x.x > y.y && x.x > z.z) {
		float a = sqrtf(fmax(0.0, 1.0 + x.x - y.y - z.z)), b = (a != 0.0f) ? 0.5f / a : 0.0f;
		rotation->x = 0.5f * a;
		rotation->y = (y.x + x.y) * b;
		rotation->z = (z.x + x.z) * b;
		rotation->w = (y.z - z.y) * b;
	}
	else if (y.y > z.z) {
		float a = sqrtf(fmax(0.0, 1.0 - x.x + y.y - z.z)), b = (a != 0.0f) ? 0.5f / a : 0.0f;
		rotation->x = (y.x + x.y) * b;
		rotation->y = 0.5f * a;
		rotation->z = (z.y + y.z) * b;
		rotation->w = (z.x - x.z) * b;
	}
	else {
		float a = sqrtf(fmax(0.0, 1.0 - x.x - y.y + z.z)), b = (a != 0.0f) ? 0.5f / a : 0.0f;
		rotation->x = (z.x + x.z) * b;
		rotation->y = (z.y + y.z) * b;
		rotation->z = 0.5f * a;
		rotation->w = (x.y - y.x) * b;
	}

	float len = rotation->x*rotation->x + rotation->y*rotation->y + rotation->z*rotation->z + rotation->w*rotation->w;
	if (fabs(len - 1.0f) > 0.1f) {
		if (fabs(len) <= 0.1f) {
			rotation->x = 0.0f;
			rotation->y = 0.0f;
			rotation->z = 0.0f;
			rotation->w = 1.0f;
		} else {
			rotation->x /= len;
			rotation->y /= len;
			rotation->z /= len;
			rotation->w /= len;
		}
	}

	scale->x *= sign_x;
	scale->y *= sign_y;
	scale->z *= sign_z;
}

Float3 float3x4_transform_point(Float3x4 m, Float3 p)
{
	Float3 result;
	result.x = F34(m,0,0)*p.x + F34(m,0,1)*p.y + F34(m,0,2)*p.z + F34(m,0,3);
	result.y = F34(m,1,0)*p.x + F34(m,1,1)*p.y + F34(m,1,2)*p.z + F34(m,1,3);
	result.z = F34(m,2,0)*p.x + F34(m,2,1)*p.y + F34(m,2,2)*p.z + F34(m,2,3);
	return result;
}

Float3 float3x4_transform_direction(Float3x4 m, Float3 p)
{
	Float3 result;
	result.x = F34(m,0,0)*p.x + F34(m,0,1)*p.y + F34(m,0,2)*p.z;
	result.y = F34(m,1,0)*p.x + F34(m,1,1)*p.y + F34(m,1,2)*p.z;
	result.z = F34(m,2,0)*p.x + F34(m,2,1)*p.y + F34(m,2,2)*p.z;
	return result;
}
