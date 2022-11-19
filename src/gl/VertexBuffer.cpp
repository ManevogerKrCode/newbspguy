#include "VertexBuffer.h"
#include "util.h"
#include <string.h>
#include "Renderer.h"

VertexAttr commonAttr[VBUF_FLAGBITS] =
{
	VertexAttr(2, GL_BYTE,          -1, GL_FALSE, ""), // TEX_2B
	VertexAttr(2, GL_SHORT,         -1, GL_FALSE, ""), // TEX_2S
	VertexAttr(2, GL_FLOAT,         -1, GL_FALSE, ""), // TEX_2F
	VertexAttr(3, GL_UNSIGNED_BYTE, -1, GL_TRUE, ""),  // COLOR_3B
	VertexAttr(3, GL_FLOAT,         -1, GL_TRUE, ""),  // COLOR_3F
	VertexAttr(4, GL_UNSIGNED_BYTE, -1, GL_TRUE, ""),  // COLOR_4B
	VertexAttr(4, GL_FLOAT,         -1, GL_TRUE, ""),  // COLOR_4F
	VertexAttr(3, GL_BYTE,          -1, GL_TRUE, ""),  // NORM_3B
	VertexAttr(3, GL_FLOAT,         -1, GL_TRUE, ""),  // NORM_3F
	VertexAttr(2, GL_BYTE,          -1, GL_FALSE, ""), // POS_2B
	VertexAttr(2, GL_SHORT,         -1, GL_FALSE, ""), // POS_2S
	VertexAttr(2, GL_INT,           -1, GL_FALSE, ""), // POS_2I
	VertexAttr(2, GL_FLOAT,         -1, GL_FALSE, ""), // POS_2F
	VertexAttr(3, GL_SHORT,         -1, GL_FALSE, ""), // POS_3S
	VertexAttr(3, GL_FLOAT,         -1, GL_FALSE, ""), // POS_3F
};

VertexAttr::VertexAttr(int numValues, int valueType, int vhandle, int normalized, const char* varName)
	: numValues(numValues), valueType(valueType), handle(vhandle), normalized(normalized), varName(varName)
{
	switch (valueType)
	{
	case(GL_BYTE):
	case(GL_UNSIGNED_BYTE):
		size = numValues;
		break;
	case(GL_SHORT):
	case(GL_UNSIGNED_SHORT):
		size = numValues * 2;
		break;
	case(GL_FLOAT):
	case(GL_INT):
	case(GL_UNSIGNED_INT):
		size = numValues * 4;
		break;
	default:
		logf("Unknown attribute value type: %d", valueType);
		handle = -1;
		size = 0;
	}
}

VertexBuffer::VertexBuffer(ShaderProgram* shaderProgram, int attFlags, const void* dat, GLsizei numVerts, int primitive)
{
	this->shaderProgram = shaderProgram;
	this->primitive = primitive;
	addAttributes(attFlags);
	setData(dat, numVerts, primitive);
	vboId = 0xFFFFFFFF;

	//glGenQueries(1, &drawQuery);
}

VertexBuffer::VertexBuffer(ShaderProgram* shaderProgram, int attFlags, int primitive)
{
	numVerts = 0;
	data = NULL;
	vboId = 0xFFFFFFFF;
	this->shaderProgram = shaderProgram;
	this->primitive = primitive;
	addAttributes(attFlags);
//	glGenQueries(1, &drawQuery);
}

VertexBuffer::~VertexBuffer() {
	deleteBuffer();
	if (ownData) {
		delete[] data;
	}
	//glDeleteQueries(1, &drawQuery);
	//drawQuery = -1;
}

void VertexBuffer::addAttributes(int attFlags)
{
	elementSize = 0;
	for (int i = 0; i < VBUF_FLAGBITS; i++)
	{
		if (attFlags & (1 << i))
		{
			if (i >= VBUF_POS_START)
				commonAttr[i].handle = shaderProgram->vposID;
			else if (i >= VBUF_COLOR_START)
				commonAttr[i].handle = shaderProgram->vcolorID;
			else
				commonAttr[i].handle = shaderProgram->vtexID;

			attribs.push_back(commonAttr[i]);
			elementSize += commonAttr[i].size;
		}
	}
}

void VertexBuffer::addAttribute(int numValues, int valueType, int normalized, const char* varName) {
	VertexAttr attribute(numValues, valueType, -1, normalized, varName);

	attribs.push_back(attribute);
	elementSize += attribute.size;
}

void VertexBuffer::addAttribute(int type, const char* varName) {

	int idx = 0;
	while (type >>= 1) // unroll for more speed...
	{
		idx++;
	}

	if (idx >= VBUF_FLAGBITS) {
		logf("Invalid attribute type\n");
		return;
	}

	VertexAttr attribute = commonAttr[idx];
	attribute.handle = -1;
	attribute.varName = varName;

	attribs.push_back(attribute);
	elementSize += attribute.size;
}

void VertexBuffer::setShader(ShaderProgram* program, bool hideErrors) {
	shaderProgram = program;
	attributesBound = false;
	for (int i = 0; i < attribs.size(); i++)
	{
		if (attribs[i].varName[0] != '\0') {
			attribs[i].handle = -1;
		}
	}

	bindAttributes(hideErrors);
	if (vboId != 0xFFFFFFFF)
	{
		deleteBuffer();
		upload();
	}
}

void VertexBuffer::bindAttributes(bool hideErrors) {
	if (attributesBound)
		return;

	for (int i = 0; i < attribs.size(); i++)
	{
		if (attribs[i].handle != -1)
			continue;

		attribs[i].handle = glGetAttribLocation(shaderProgram->ID, attribs[i].varName);

		if (!hideErrors && attribs[i].handle == -1)
			logf("Could not find vertex attribute: %s\n", attribs[i].varName);
	}

	attributesBound = true;
}

void VertexBuffer::setData(const void* _data, GLsizei _numVerts)
{
	this->data = (unsigned char*)_data;
	this->numVerts = _numVerts;
}

void VertexBuffer::setData(const void* _data, GLsizei _numVerts, int _primitive)
{
	this->data = (unsigned char*)_data;
	this->numVerts = _numVerts;
	this->primitive = _primitive;
}

void VertexBuffer::upload() {
	shaderProgram->bind();
	bindAttributes();

	glGenBuffers(1, &vboId);
	glBindBuffer(GL_ARRAY_BUFFER, vboId);
	glBufferData(GL_ARRAY_BUFFER, elementSize * numVerts, data, GL_STATIC_DRAW);

	size_t offset = 0;
	for (int i = 0; i < attribs.size(); i++)
	{
		VertexAttr& a = attribs[i];
		void* ptr = (void*)offset;
		offset += a.size;
		if (a.handle == -1) {
			continue;
		}
		glEnableVertexAttribArray(a.handle);
		glVertexAttribPointer(a.handle, a.numValues, a.valueType, a.normalized != 0, elementSize, ptr);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VertexBuffer::deleteBuffer() {
	if (vboId != 0xFFFFFFFF)
		glDeleteBuffers(1, &vboId);
	vboId = 0xFFFFFFFF;
}

void VertexBuffer::drawRange(GLint start, GLsizei end)
{
	if (start < 0 || start > numVerts)
		logf("Invalid start index: %d\n", start);
	else if (end > numVerts || end < 0)
		logf("Invalid end index: %d\n", end);
	else if (end - start <= 0)
		logf("Invalid draw range: %d -> %d\n", start, end);
	else
	{
		char* offsetPtr = (char*)data;

		shaderProgram->bind();
		bindAttributes();

		if (vboId != 0xFFFFFFFF) {
			glBindBuffer(GL_ARRAY_BUFFER, vboId);
			offsetPtr = NULL;
		}

		int offset = 0;

		// Drop FPS below: FIXME
		for (int i = 0; i < attribs.size(); i++)
		{
			VertexAttr& a = attribs[i];
			void* ptr = offsetPtr + offset;
			offset += a.size;
			if (a.handle == -1)
				continue;
			glEnableVertexAttribArray(a.handle);
			glVertexAttribPointer(a.handle, a.numValues, a.valueType, a.normalized != 0, elementSize, ptr);
		}
		// Drop FPS above 

		glDrawArrays(primitive, start, end - start);

		for (int i = 0; i < attribs.size(); i++)
		{
			VertexAttr& a = attribs[i];
			if (a.handle == -1)
				continue;
			glDisableVertexAttribArray(a.handle);
		}

		if (vboId != 0xFFFFFFFF) {
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

	}
}

void VertexBuffer::drawFull()
{
	return drawRange(0, numVerts);
}
