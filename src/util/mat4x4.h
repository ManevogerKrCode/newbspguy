#pragma once
#include "vectors.h"
#include <string.h>

// A row-major 4x4 matrix for use in OpenGL shader programs
struct mat4x4
{
	float m[16];

	void loadIdentity();

	void perspective(float fov, float aspect, float near, float far);

	// Set up an orthographic projection matrix
	void ortho(float left, float right, float bottom, float top, float near, float far);

	void translate(float x, float y, float z);

	void scale(float x, float y, float z);

	void rotateX(float r);

	void rotateY(float r);

	void rotateZ(float r);

	void rotate(float x, float y, float z);

	// converts row-major matrix to column-major (for OpenGL)
	mat4x4 transpose();

	mat4x4 invert();

	float& operator ()(size_t idx)
	{
		return m[idx];
	}

	float operator ()(size_t idx) const
	{
		return m[idx];
	}
	mat4x4() = default;

	mat4x4(const float newm[16])
	{
		memcpy(m, newm, 16 * sizeof(float));
	}
	mat4x4 operator*(float newm[16])
	{
		mult(newm);
		return *this;
	}

private:
	void mult(float mat[16]);
};

void loadEmptyMat4x4(float * m);

mat4x4 operator*(const mat4x4& m1, const mat4x4& m2);
vec4 operator*(const mat4x4& mat, const vec4& vec);
mat4x4 worldToLocalTransform(const vec3& local_x, const vec3& local_y, const vec3& local_z);
void mat4x4print(const mat4x4& mat);

extern float m_identity[16];
extern float m_zero[16];
void mat4x4_saveIdentity();