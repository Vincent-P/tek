#pragma once

typedef union Float3 Float3;
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

void Serialize_Float3(Serializer *serializer, Float3 *value)
{
	SV_ADD(SV_INITIAL, float, x);
	SV_ADD(SV_INITIAL, float, y);
	SV_ADD(SV_INITIAL, float, z);
}

Float3 float3_sub(Float3 a, Float3 b)
{
	Float3 r;
	r.x = a.x - b.x;
	r.y = a.y - b.y;
	r.z = a.z - b.z;
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

void Serialize_Quat(Serializer *serializer, Quat *value)
{
	SV_ADD(SV_INITIAL, float, x);
	SV_ADD(SV_INITIAL, float, y);
	SV_ADD(SV_INITIAL, float, z);
	SV_ADD(SV_INITIAL, float, w);
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

typedef struct Float3x4 Float3x4;
struct Float3x4
{
	float values[12];
};

// Column major
#define CM(i, j) (i * 4 + j)
#define F44(m, i, j) m.values[CM(i, j)]

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

Float3 float4x4_transform_point(Float4x4 m, Float3 p)
{
	Float3 result;
	result.x = F44(m,0,0)*p.x + F44(m,0,1)*p.y + F44(m,0,2)*p.z + F44(m,0,3);
	result.y = F44(m,1,0)*p.x + F44(m,1,1)*p.y + F44(m,1,2)*p.z + F44(m,1,3);
	result.z = F44(m,2,0)*p.x + F44(m,2,1)*p.y + F44(m,2,2)*p.z + F44(m,2,3);
	float w  = F44(m,3,0)*p.x + F44(m,3,1)*p.y + F44(m,3,2)*p.z + F44(m,3,3);
	return result;
}

Float3 float4x4_project_point(Float4x4 m, Float3 p)
{
	float w  = F44(m,3,0)*p.x + F44(m,3,1)*p.y + F44(m,3,2)*p.z + F44(m,3,3);
	
	Float3 result;
	result.x = (F44(m,0,0)*p.x + F44(m,0,1)*p.y + F44(m,0,2)*p.z + F44(m,0,3)) / w;
	result.y = (F44(m,1,0)*p.x + F44(m,1,1)*p.y + F44(m,1,2)*p.z + F44(m,1,3)) / w;
	result.z = (F44(m,2,0)*p.x + F44(m,2,1)*p.y + F44(m,2,2)*p.z + F44(m,2,3)) / 2;
	return result;
}

Float3 float4x4_project_vector(Float4x4 m, Float3 p)
{
	Float3 result;
	result.x = F44(m,0,0)*p.x + F44(m,0,1)*p.y + F44(m,0,2)*p.z + F44(m,0,3);
	result.y = F44(m,1,0)*p.x + F44(m,1,1)*p.y + F44(m,1,2)*p.z + F44(m,1,3);
	result.z = F44(m,2,0)*p.x + F44(m,2,1)*p.y + F44(m,2,2)*p.z + F44(m,2,3);
	float w  = F44(m,3,0)*p.x + F44(m,3,1)*p.y + F44(m,3,2)*p.z + F44(m,3,3);
	return result;
}

Float4x4 perspective_projection(float vertical_fov, float aspect_ratio, float n, float f, Float4x4 *inverse)
{
    float fov_rad = vertical_fov * 2.0f * 3.14f / 360.0f;
    float focal_length = 1.0f / tanf(fov_rad / 2.0f);

    float x  =  focal_length / aspect_ratio;
    float y  = -focal_length;
    float A  = n / (f - n);
    float B  = f * A;

    Float4x4 p;
    F44(p,0,0) =    x;	F44(p,0,1) = 0.0f;	F44(p,0,2) =  0.0f;	F44(p,0,3) = 0.0f;
    F44(p,1,0) = 0.0f;	F44(p,1,1) =    y;	F44(p,1,2) =  0.0f;	F44(p,1,3) = 0.0f;
    F44(p,2,0) = 0.0f;	F44(p,2,1) = 0.0f;	F44(p,2,2) =     A;	F44(p,2,3) =    B;
    F44(p,3,0) = 0.0f;	F44(p,3,1) = 0.0f;	F44(p,3,2) = -1.0f;	F44(p,3,3) = 0.0f;

    if (inverse)
    {
	    Float4x4 ip;
	    F44(ip,0,0) =  1/x;	F44(ip,0,1) = 0.0f;	F44(ip,0,2) =  0.0f;	F44(ip,0,3) =  0.0f;
	    F44(ip,1,0) = 0.0f;	F44(ip,1,1) =  1/y;	F44(ip,1,2) =  0.0f;	F44(ip,1,3) =  0.0f;
	    F44(ip,2,0) = 0.0f;	F44(ip,2,1) = 0.0f;	F44(ip,2,2) =  0.0f;	F44(ip,2,3) = -1.0f;
	    F44(ip,3,0) = 0.0f;	F44(ip,3,1) = 0.0f;	F44(ip,3,2) =   1/B;	F44(ip,3,3) =   A/B;
	    *inverse = ip;
    }

    return p;
}

Float4x4 lookat_view(Float3 from, Float3 to)
{
	Float3 forward = float3_normalize(float3_sub(to, from)); // -Z
	Float3 right = float3_cross(forward, (Float3){0.0f, 1.0f, 0.0f}); // X
	Float3 up = float3_cross(right, forward); // Y

	Float4x4 v;
	F44(v,0,0) =    right.x;	F44(v,0,1) =    right.y;	F44(v,0,2) =    right.z;	F44(v,0,3) = -float3_dot(from, right);
	F44(v,1,0) =       up.x;	F44(v,1,1) =       up.y;	F44(v,1,2) =       up.z;	F44(v,1,3) = -float3_dot(from, up);
	F44(v,2,0) = -forward.x;	F44(v,2,1) = -forward.y;	F44(v,2,2) = -forward.z;	F44(v,2,3) =  float3_dot(from, forward);
	F44(v,3,0) =       0.0f;	F44(v,3,1) =       0.0f;	F44(v,3,2) =        0.0f;	F44(v,3,3) = 1.0f;
	return v;
}
