#pragma once

#include <filesystem>
namespace fs = std::filesystem;

#include <string>
#include <vector>
#include "mat4x4.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <thread>
#include <mutex>
#include "ProgressMeter.h"
#include "bsptypes.h"


extern std::string g_version_string;

#ifndef WIN32
#define fopen_s(pFile,filename,mode) ((*(pFile))=fopen((filename),  (mode)))==NULL
#endif

#define PRINT_BLUE		1
#define PRINT_GREEN		2
#define PRINT_RED		4
#define PRINT_BRIGHT	8

#define PI 3.141592f

#define EPSILON 0.0001f // NORMAL_EPSILON from rad.h / 10


#define mDotProduct(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])


static const vec3  s_baseaxis[18] = {
	{0, 0, 1}, {1, 0, 0}, {0, -1, 0},                      // floor
	{0, 0, -1}, {1, 0, 0}, {0, -1, 0},                     // ceiling
	{1, 0, 0}, {0, 1, 0}, {0, 0, -1},                      // west wall
	{-1, 0, 0}, {0, 1, 0}, {0, 0, -1},                     // east wall
	{0, 1, 0}, {1, 0, 0}, {0, 0, -1},                      // south wall
	{0, -1, 0}, {1, 0, 0}, {0, 0, -1},                     // north wall
};


extern unsigned int g_frame_counter;
extern double g_time;
extern bool DebugKeyPressed;
extern bool g_verbose;
extern ProgressMeter g_progress;
extern std::vector<std::string> g_log_buffer;
extern std::mutex g_log_mutex;

extern int g_render_flags;

void logf(const char* format, ...);

void debugf(const char* format, ...);

bool fileExists(const std::string& fileName);

void copyFile(const std::string& fileName, const std::string& fileName2);

char* loadFile(const std::string& fileName, int& length);

bool writeFile(const std::string& fileName, const char* data, int len);
bool writeFile(const std::string& fileName, const std::string& data);

bool removeFile(const std::string& fileName);

std::streampos fileSize(const std::string& filePath);

std::vector<std::string> splitStringIgnoringQuotes(std::string s, const std::string& delimitter);
std::vector<std::string> splitString(std::string s, const std::string& delimitter);

std::string basename(const std::string& path);

std::string stripExt(const std::string& filename);

bool isNumeric(const std::string& s);

void print_color(int colors);

std::string getConfigDir();

bool dirExists(const std::string& dirName);

bool createDir(const std::string& dirName);

void removeDir(const std::string& dirName);

std::string toLowerCase(std::string str);

std::string trimSpaces(std::string s);

int getBspTextureSize(BSPMIPTEX* bspTexture);

float clamp(float val, float min, float max);

vec3 parseVector(const std::string& s);

bool IsEntNotSupportAngles(std::string& entname);

bool pickAABB(vec3 start, vec3 rayDir, vec3 mins, vec3 maxs, float& bestDist);

bool rayPlaneIntersect(const vec3& start, const vec3& dir, const vec3& normal, float fdist, float& intersectDist);

float getDistAlongAxis(const vec3& axis, const vec3& p);

// returns false if verts are not planar
bool getPlaneFromVerts(const std::vector<vec3>& verts, vec3& outNormal, float& outDist);

void getBoundingBox(const std::vector<vec3>& verts, vec3& mins, vec3& maxs);

vec2 getCenter(std::vector<vec2>& verts);

vec3 getCenter(std::vector<vec3>& verts);

vec3 getCenter(const vec3& maxs, const vec3& mins);

void expandBoundingBox(const vec3& v, vec3& mins, vec3& maxs);

void expandBoundingBox(const vec2& v, vec2& mins, vec2& maxs);

std::vector<vec3> getPlaneIntersectVerts(std::vector<BSPPLANE>& planes);

bool vertsAllOnOneSide(std::vector<vec3>& verts, BSPPLANE& plane);

// get verts from the given set that form a triangle (no duplicates and not colinear)
std::vector<vec3> getTriangularVerts(std::vector<vec3>& verts);

vec3 getNormalFromVerts(std::vector<vec3>& verts);

// transforms verts onto a plane (which is defined by the verts themselves)
std::vector<vec2> localizeVerts(std::vector<vec3>& verts);

// Returns CCW sorted indexes into the verts, as viewed on the plane the verts define
std::vector<int> getSortedPlanarVertOrder(std::vector<vec3>& verts);

std::vector<vec3> getSortedPlanarVerts(std::vector<vec3>& verts);

bool pointInsidePolygon(std::vector<vec2>& poly, vec2 p);

enum class FIXUPPATH_SLASH
{
	FIXUPPATH_SLASH_CREATE,
	FIXUPPATH_SLASH_SKIP,
	FIXUPPATH_SLASH_REMOVE
};
void fixupPath(char* path, FIXUPPATH_SLASH startslash, FIXUPPATH_SLASH endslash);
void fixupPath(std::string& path, FIXUPPATH_SLASH startslash, FIXUPPATH_SLASH endslash);
void replaceAll(std::string& str, const std::string& from, const std::string& to);

void WriteBMP(const std::string& fileName, unsigned char* pixels, int width, int height, int bytesPerPixel);

std::string GetCurrentWorkingDir();

bool VectorCompare(vec3 v1, vec3 v2);

void QuaternionSlerp(const vec4& p, vec4 q, float t, vec4& qt);
void AngleQuaternion(const vec3& angles, vec4& quaternion);
void QuaternionMatrix(const vec4& quaternion, float(*matrix)[4]);
void R_ConcatTransforms(const float in1[3][4], const float in2[3][4], float out[3][4]);
void VectorScale(vec3 v, float scale, vec3& out);
float VectorNormalize(vec3 v);
void mCrossProduct(vec3 v1, vec3 v2, vec3& cross);
void VectorIRotate(const vec3& in1, const float in2[3][4], vec3& out);
void VectorTransform(const vec3& in1, const float in2[3][4], vec3& out);


int TextureAxisFromPlane(const BSPPLANE& pln, vec3& xv, vec3& yv);
float AngleFromTextureAxis(vec3 axis, bool x, int type);
vec3 AxisFromTextureAngle(float angle, bool x, int type);

size_t strlen(std::string str);

bool Is256Colors(COLOR3* image, int size);
int ColorDistance(COLOR3 color, COLOR3 other);
void SimpeColorReduce(COLOR3* image, int size);