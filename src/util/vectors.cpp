#include "vectors.h"
#include <cmath>
#include "mat4x4.h"
#include "util.h"

bool operator==(const vec3& v1, const vec3& v2)
{
	vec3 v = v1 - v2;
	if (abs(v.x) >= EPSILON)
		return false;
	if (abs(v.y) >= EPSILON)
		return false;
	if (abs(v.z) >= EPSILON)
		return false;
	return true;
}

bool operator!=(const vec3& v1, const vec3& v2)
{
	vec3 v = v1 - v2;
	if (abs(v.x) >= EPSILON)
		return true;
	if (abs(v.y) >= EPSILON)
		return true;
	if (abs(v.z) >= EPSILON)
		return true;
	return false;
}

vec3 operator-(vec3 v1, const vec3& v2)
{
	v1.x -= v2.x;
	v1.y -= v2.y;
	v1.z -= v2.z;
	return v1;
}

vec3 operator+(vec3 v1, const vec3& v2)
{
	v1.x += v2.x;
	v1.y += v2.y;
	v1.z += v2.z;
	return v1;
}

vec3 operator*(vec3 v1, const vec3& v2)
{
	v1.x *= v2.x;
	v1.y *= v2.y;
	v1.z *= v2.z;
	return v1;
}

vec3 operator/(vec3 v1, const vec3& v2)
{
	v1.x /= v2.x;
	v1.y /= v2.y;
	v1.z /= v2.z;
	return v1;
}

vec3 operator-(vec3 v, float f)
{
	v.x -= f;
	v.y -= f;
	v.z -= f;
	return v;
}

vec3 operator+(vec3 v, float f)
{
	v.x += f;
	v.y += f;
	v.z += f;
	return v;
}

vec3 operator*(vec3 v, float f)
{
	v.x *= f;
	v.y *= f;
	v.z *= f;
	return v;
}

vec3 operator/(vec3 v, float f)
{
	v.x /= f;
	v.y /= f;
	v.z /= f;
	return v;
}

void vec3::operator-=(const vec3& v)
{
	x -= v.x;
	y -= v.y;
	z -= v.z;
}

void vec3::operator+=(const vec3& v)
{
	x += v.x;
	y += v.y;
	z += v.z;
}

void vec3::operator*=(const vec3& v)
{
	x *= v.x;
	y *= v.y;
	z *= v.z;
}

void vec3::operator/=(const vec3& v)
{
	x /= v.x;
	y /= v.y;
	z /= v.z;
}

void vec3::operator-=(float f)
{
	x -= f;
	y -= f;
	z -= f;
}

void vec3::operator+=(float f)
{
	x += f;
	y += f;
	z += f;
}

void vec3::operator*=(float f)
{
	x *= f;
	y *= f;
	z *= f;
}

void vec3::operator/=(float f)
{
	x /= f;
	y /= f;
	z /= f;
}

vec3 crossProduct(const vec3 & v1,const vec3 & v2)
{
	float x = v1.y * v2.z - v2.y * v1.z;
	float y = v2.x * v1.z - v1.x * v2.z;
	float z = v1.x * v2.y - v1.y * v2.x;
	return vec3(x, y, z);
}

float dotProduct(const vec3& v1, const vec3& v2)
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

void makeVectors(const vec3& angles, vec3& forward, vec3& right, vec3& up)
{
	mat4x4 rotMat;
	rotMat.loadIdentity();
	rotMat.rotateX(PI * angles.x / 180.0f);
	rotMat.rotateY(PI * angles.y / 180.0f);
	rotMat.rotateZ(PI * angles.z / 180.0f);

	vec4 f = rotMat * vec4(0, 1, 0, 1);
	vec4 r = rotMat * vec4(1, 0, 0, 1);
	vec4 u = rotMat * vec4(0, 0, 1, 1);

	forward = vec3(f.x, f.y, f.z);
	right = vec3(r.x, r.y, r.z);
	up = vec3(u.x, u.y, u.z);
}

vec3 vec3::normalize(float length)
{
	if (abs(x) < EPSILON && abs(y) < EPSILON && abs(z) < EPSILON)
		return vec3();
	float d = length / sqrt((x * x) + (y * y) + (z * z));
	return vec3(x * d, y * d, z * d);
}

vec3 vec3::invert()
{
	return vec3(abs(x) >= EPSILON ? -x : x, abs(y) >= EPSILON ? -y : y, abs(z) >= EPSILON ? -z : z);
}

float vec3::length()
{
	return sqrt((x * x) + (y * y) + (z * z));
}

bool vec3::IsZero()
{
	return (abs(x) + abs(y) + abs(z)) < EPSILON;
}

std::string vec3::toString()
{
	return std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z);
}

std::string vec3::toKeyvalueString(bool truncate, const std::string& suffix_x, const std::string& suffix_y, const std::string& suffix_z)
{
	std::string parts[3] = {std::to_string(x) ,std::to_string(y), std::to_string(z)};

	// remove trailing zeros to save some space
	for (int i = 0; i < 3; i++)
	{
		if (truncate)
		{
			parts[i] = parts[i].substr(0, parts[i].find('.') + 3);
		}

		parts[i].erase(parts[i].find_last_not_of('0') + 1, std::string::npos);

		// strip dot if there's no fractional part
		if (parts[i][parts[i].size() - 1] == '.')
		{
			parts[i] = parts[i].substr(0, parts[i].size() - 1);
		}
	}

	return parts[0] + suffix_x + parts[1] + suffix_y + parts[2] + suffix_z;
}

vec3 vec3::flip()
{
	return vec3(x, z, -y);
}


bool operator==(vec2 v1, vec2 v2)
{
	vec2 v = v1 - v2;
	if (abs(v.x) >= EPSILON)
		return false;
	if (abs(v.y) >= EPSILON)
		return false;
	return true;
}

bool operator!=(vec2 v1, vec2 v2)
{
	return abs(v1.x - v2.x) >= EPSILON || abs(v1.y - v2.y) >= EPSILON;
}

vec2 operator-(vec2 v1, vec2 v2)
{
	v1.x -= v2.x;
	v1.y -= v2.y;
	return v1;
}

vec2 operator+(vec2 v1, vec2 v2)
{
	v1.x += v2.x;
	v1.y += v2.y;
	return v1;
}

vec2 operator*(vec2 v1, vec2 v2)
{
	v1.x *= v2.x;
	v1.y *= v2.y;
	return v1;
}

vec2 operator/(vec2 v1, vec2 v2)
{
	v1.x /= v2.x;
	v1.y /= v2.y;
	return v1;
}

vec2 operator-(vec2 v, float f)
{
	v.x -= f;
	v.y -= f;
	return v;
}

vec2 operator+(vec2 v, float f)
{
	v.x += f;
	v.y += f;
	return v;
}

vec2 operator*(vec2 v, float f)
{
	v.x *= f;
	v.y *= f;
	return v;
}

vec2 operator/(vec2 v, float f)
{
	v.x /= f;
	v.y /= f;
	return v;
}

void vec2::operator-=(vec2 v)
{
	x -= v.x;
	y -= v.y;
}

void vec2::operator+=(vec2 v)
{
	x += v.x;
	y += v.y;
}

void vec2::operator*=(vec2 v)
{
	x *= v.x;
	y *= v.y;
}

void vec2::operator/=(vec2 v)
{
	x /= v.x;
	y /= v.y;
}

void vec2::operator-=(float f)
{
	x -= f;
	y -= f;
}

void vec2::operator+=(float f)
{
	x += f;
	y += f;
}

void vec2::operator*=(float f)
{
	x *= f;
	y *= f;
}

void vec2::operator/=(float f)
{
	x /= f;
	y /= f;
}

float vec2::length()
{
	return sqrt((x * x) + (y * y));
}

vec2 vec2::normalize(float length)
{
	if (abs(x) < EPSILON && abs(y) < EPSILON)
		return vec2();
	float d = length / sqrt((x * x) + (y * y));
	return vec2(x * d, y * d);
}



bool operator==(const vec4& v1, const vec4& v2)
{
	vec4 v = v1 - v2;
	return abs(v.x) < EPSILON && abs(v.y) < EPSILON && abs(v.z) < EPSILON && abs(v.w) < EPSILON;
}


bool operator!=(const vec4& v1, const vec4& v2)
{
	return !(v1 == v2);
}


vec4 operator+(vec4 v1, const vec4& v2)
{
	v1.x += v2.x;
	v1.y += v2.y;
	v1.z += v2.z;
	v1.w += v2.w;
	return v1;
}

vec4 operator+(vec4 v, float f)
{
	v.x += f;
	v.y += f;
	v.z += f;
	v.w += f;
	return v;
}



vec4 operator*(vec4 v1, const vec4& v2)
{
	v1.x *= v2.x;
	v1.y *= v2.y;
	v1.z *= v2.z;
	v1.w *= v2.w;
	return v1;
}

vec4 operator*(vec4 v, float f)
{
	v.x *= f;
	v.y *= f;
	v.z *= f;
	v.w *= f;
	return v;
}



vec4 operator/(vec4 v1, const vec4& v2)
{
	v1.x /= v2.x;
	v1.y /= v2.y;
	v1.z /= v2.z;
	v1.w /= v2.w;
	return v1;
}

vec4 operator/(vec4 v, float f)
{
	v.x /= f;
	v.y /= f;
	v.z /= f;
	v.w /= f;
	return v;
}


vec4 operator-(vec4 v1, const vec4& v2)
{
	v1.x -= v2.x;
	v1.y -= v2.y;
	v1.z -= v2.z;
	v1.w -= v2.w;
	return v1;
}

vec4 operator-(vec4 v, float f)
{
	v.x -= f;
	v.y -= f;
	v.z -= f;
	v.w -= f;
	return v;
}

vec3 vec4::xyz()
{
	return vec3(x, y, z);
}

vec2 vec4::xy()
{
	return vec2(x, y);
}
