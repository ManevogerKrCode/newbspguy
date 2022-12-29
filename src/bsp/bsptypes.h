#pragma once
#include <stdint.h>
#include "vectors.h"
#include "bsplimits.h"
#include <vector>

#pragma pack(push, 1)

#define	SIDESPACE	24
#define	BOGUS_RANGE	18000

#define BSP_MODEL_BYTES 64 // size of a BSP model in bytes

#define LUMP_ENTITIES      0
#define LUMP_PLANES        1
#define LUMP_TEXTURES      2
#define LUMP_VERTICES      3
#define LUMP_VISIBILITY    4
#define LUMP_NODES         5
#define LUMP_TEXINFO       6
#define LUMP_FACES         7
#define LUMP_LIGHTING      8
#define LUMP_CLIPNODES     9
#define LUMP_LEAVES       10
#define LUMP_MARKSURFACES 11
#define LUMP_EDGES        12
#define LUMP_SURFEDGES    13
#define LUMP_MODELS       14
#define HEADER_LUMPS      15

// extra lump ordering
#define LUMP_LIGHTVECS		0	// deluxemap data
#define LUMP_FACEINFO		1	// landscape and lightmap resolution info
#define LUMP_CUBEMAPS		2	// cubemap description
#define LUMP_VERTNORMALS	3	// phong shaded vertex normals
#define LUMP_LEAF_LIGHTING	4	// store vertex lighting for statics
#define LUMP_WORLDLIGHTS	5	// list of all the virtual and real lights (used to relight models in-game)
#define LUMP_COLLISION		6	// physics engine collision hull dump (userdata)
#define LUMP_AINODEGRAPH	7	// node graph that stored into the bsp (userdata)
#define LUMP_SHADOWMAP		8	// contains shadow map for direct light
#define LUMP_VERTEX_LIGHT	9	// store vertex lighting for statics
#define LUMP_VISLIGHTDATA	10	// how many lights affects to faces
#define LUMP_SURFACE_LIGHT	11	// models lightmapping
#define EXTRA_LUMPS			12	// count of the extra lumps
#define EXTRA_LUMPS_OLD		8	// count of the extra lumps

enum lump_copy_targets
{
	ENTITIES = 1,
	PLANES = 2,
	TEXTURES = 4,
	VERTICES = 8,
	VISIBILITY = 16,
	NODES = 32,
	TEXINFO = 64,
	FACES = 128,
	LIGHTING = 256,
	CLIPNODES = 512,
	LEAVES = 1024,
	MARKSURFACES = 2048,
	EDGES = 4096,
	SURFEDGES = 8192,
	MODELS = 16384
};


enum clean_unused_lump
{
	CLEAN_LIGHTMAP = 1,
	CLEAN_PLANES = 2,
	CLEAN_NODES = 4,
	CLEAN_CLIPNODES = 8,
	CLEAN_LEAVES = 16,
	CLEAN_MARKSURFACES = 32,
	CLEAN_FACES = 64,
	CLEAN_SURFEDGES = 128,
	CLEAN_TEXINFOS = 256,
	CLEAN_EDGES = 512,
	CLEAN_VERTICES = 1024,
	CLEAN_TEXTURES = 2048,
	CLEAN_VISDATA = 4096,
	CLEAN_CLIPNODES_SOMETHING = 8192
};

#define MAX_AMBIENTS		  4

#define CONTENTS_EMPTY        -1
#define CONTENTS_SOLID        -2
#define CONTENTS_WATER        -3
#define CONTENTS_SLIME        -4
#define CONTENTS_LAVA         -5
#define CONTENTS_SKY          -6
#define CONTENTS_ORIGIN       -7
#define CONTENTS_CLIP         -8
#define CONTENTS_CURRENT_0    -9
#define CONTENTS_CURRENT_90   -10
#define CONTENTS_CURRENT_180  -11
#define CONTENTS_CURRENT_270  -12
#define CONTENTS_CURRENT_UP   -13
#define CONTENTS_CURRENT_DOWN -14
#define CONTENTS_TRANSLUCENT  -15

#define PLANE_X 0     // Plane is perpendicular to given axis
#define PLANE_Y 1
#define PLANE_Z 2
#define PLANE_ANYX 3  // Non-axial plane is snapped to the nearest
#define PLANE_ANYY 4
#define PLANE_ANYZ 5

// maximum x/y hull extent a monster can have before it starts using hull 2
#define MAX_HULL1_EXTENT_MONSTER 18

// maximum x/y hull dimension a pushable can have before it starts using hull 2
#define MAX_HULL1_SIZE_PUSHABLE 34.0f

static const char* g_lump_names[HEADER_LUMPS] = {
	"ENTITIES",
	"PLANES",
	"TEXTURES",
	"VERTICES",
	"VISIBILITY",
	"NODES",
	"TEXINFO",
	"FACES",
	"LIGHTING",
	"CLIPNODES",
	"LEAVES",
	"MARKSURFACES",
	"EDGES",
	"SURFEDGES",
	"MODELS"
};

enum MODEL_SORT_MODES
{
	SORT_VERTS,
	SORT_NODES,
	SORT_CLIPNODES,
	SORT_FACES,
	SORT_MODES
};

struct BSPLUMP
{
	int nOffset; // File offset to data
	int nLength; // Length of data
};


struct BSPHEADER
{
	int nVersion;           // Must be 30 for a valid HL BSP file
	BSPLUMP lump[HEADER_LUMPS]; // Stores the directory of lumps
};

struct BSPHEADER_EX
{
	int	id;			// must be little endian XASH
	int	version;
	BSPLUMP	lump[EXTRA_LUMPS];
};

struct LumpState
{
	unsigned char* lumps[HEADER_LUMPS];
	int lumpLen[HEADER_LUMPS];
};

struct BSPPLANE
{
	vec3 vNormal;
	float fDist;
	int nType;

	// returns true if the plane was flipped
	bool update(vec3 newNormal, float fdist);

	BSPPLANE()
	{
		vNormal = vec3();
		fDist = 0.0f;
		nType = 0;
	}

	BSPPLANE(vec3 normal, float dist, int type)
	{
		vNormal = normal;
		fDist = dist;
		nType = type;
	}
};

struct CSGPLANE
{
	double normal[3];
	double origin[3];
	double dist;
	int nType;
};

struct BSPTEXTUREINFO
{
	vec3 vS;
	float shiftS;
	vec3 vT;
	float shiftT;
	int iMiptex;
	int nFlags;
};

struct BSPMIPTEX
{
	char szName[MAXTEXTURENAME];  // Name of texture
	int nWidth, nHeight;		  // Extends of the texture
	int nOffsets[MIPLEVELS];	  // Offsets to texture mipmaps, relative to the start of this structure
};

struct BSPFACE16
{
	unsigned short iPlane;          // Plane the face is parallel to
	short nPlaneSide;      // Set if different normals orientation
	int iFirstEdge;      // Index of the first surfedge
	short nEdges;          // Number of consecutive surfedges
	short iTextureInfo;    // Index of the texture info structure
	unsigned char nStyles[4];       // Specify lighting styles
	int nLightmapOffset; // Offsets into the raw lightmap data
};

struct BSPFACE32
{
	int iPlane;          // Plane the face is parallel to
	int nPlaneSide;      // Set if different normals orientation
	int iFirstEdge;      // Index of the first surfedge
	int nEdges;          // Number of consecutive surfedges
	int iTextureInfo;    // Index of the texture info structure
	unsigned char nStyles[4];       // Specify lighting styles
	int nLightmapOffset; // Offsets into the raw lightmap data
};

struct BSPLEAF16
{
	int nContents;                         // Contents enumeration
	int nVisOffset;                        // Offset into the visibility lump
	short nMins[3], nMaxs[3];                // Defines bounding box
	unsigned short iFirstMarkSurface;
	unsigned short nMarkSurfaces;	// Index and count into marksurfaces array
	unsigned char nAmbientLevels[MAX_AMBIENTS];                 // Ambient sound levels

	bool isEmpty();
};

struct BSPLEAF32
{
	int	nContents;
	int	nVisOffset;
	float nMins[3];
	float nMaxs[3];
	int	iFirstMarkSurface;
	int nMarkSurfaces;
	unsigned char nAmbientLevels[MAX_AMBIENTS];

	bool isEmpty();
};

struct BSPLEAF32A
{
	int	nContents;
	int	nVisOffset;
	short nMins[3];
	short nMaxs[3];
	int	iFirstMarkSurface;
	int nMarkSurfaces;
	unsigned char nAmbientLevels[MAX_AMBIENTS];

	bool isEmpty();
};

struct BSPEDGE16
{
	unsigned short iVertex[2]; // Indices into vertex array

	BSPEDGE16();
	BSPEDGE16(unsigned int v1, unsigned int v2);
	BSPEDGE16(unsigned short v1, unsigned short v2);
};

struct BSPEDGE32
{
	int iVertex[2]; // Indices into vertex array

	BSPEDGE32();
	BSPEDGE32(unsigned int v1, unsigned int v2);
};

struct BSPMODEL
{
	vec3 nMins;
	vec3 nMaxs;
	vec3 vOrigin;                  // Coordinates to move the // coordinate system
	int iHeadnodes[MAX_MAP_HULLS]; // Index into nodes array
	int nVisLeafs;                 // ???
	int iFirstFace, nFaces;        // Index and count into faces
};

struct BSPNODE16
{
	int iPlane;            // Index into Planes lump
	short iChildren[2];       // If > 0, then indices into Nodes // otherwise bitwise inverse indices into Leafs
	short nMins[3], nMaxs[3]; // Defines bounding box
	unsigned short firstFace, nFaces; // Index and count into Faces
};

struct BSPNODE32
{
	int	iPlane;
	int	iChildren[2];		// negative numbers are -(leafs+1), not nodes
	float nMins[3];			// for sphere culling
	float nMaxs[3];
	int	firstFace;
	int	nFaces;			// counting both sides
};

struct BSPNODE32A
{
	int	iPlane;
	int	iChildren[2];		// negative numbers are -(leafs+1), not nodes
	short nMins[3];			// for sphere culling
	short nMaxs[3];
	int	firstFace;
	int	nFaces;			// counting both sides
};

struct BSPCLIPNODE16
{
	int iPlane;       // Index into planes
	short iChildren[2]; // negative numbers are contents
};

struct BSPCLIPNODE32
{
	int iPlane;       // Index into planes
	int iChildren[2]; // negative numbers are contents
};

/*
 * application types (not part of the BSP)
 */

struct ScalableTexinfo
{
	int texinfoIdx;
	vec3 oldS, oldT;
	float oldShiftS, oldShiftT;
	int planeIdx;
	int faceIdx;
};
#pragma pack(pop)
struct TransformVert
{
	vec3 pos;
	vec3* ptr; // face vertex to move with (null for invisible faces)
	std::vector<int> iPlanes;
	vec3 startPos; // for dragging
	vec3 undoPos; // for undoing invalid solid stuff
	bool selected;
};
#pragma pack(push, 1)
struct HullEdge
{
	int verts[2]; // index into modelVerts/hullVerts
	int planes[2]; // index into iPlanes
	bool selected;
};

struct Face
{
	std::vector<int> verts; // index into hullVerts
	BSPPLANE plane;
	int planeSide;
	int iTextureInfo;
};

struct Solid
{
	std::vector<Face> faces;
	std::vector<TransformVert> hullVerts; // control points for hull 0
	std::vector<HullEdge> hullEdges; // for vertex manipulation (holds indexes into hullVerts)
};
#pragma pack(pop)
// used to construct bounding volumes for solid leaves

struct BSPPLANEX
{
	vec3 vNormal;
	float fDist;
	int nType;
	int planeId;
	// returns true if the plane was flipped
	bool update(vec3 newNormal, float fdist);

	BSPPLANEX()
	{
		vNormal = vec3();
		fDist = 0.0f;
		nType = 0;
		planeId = -1;
	}

	BSPPLANEX(const BSPPLANE& other)
	{
		vNormal = other.vNormal;
		fDist = other.fDist;
		nType = other.nType;
		planeId = -1;
	}

	BSPPLANEX(vec3 normal, float dist, int type)
	{
		vNormal = normal;
		fDist = dist;
		nType = type;
		planeId = -1;
	}
};


struct NodeVolumeCuts
{
	int nodeIdx;
	std::vector<BSPPLANEX> cuts; // cuts which define the leaf boundaries when applied to a bounding box, in order.
};
#pragma pack(push, 1)
// Rendering constants
enum RenderMode : int
{
	kRenderNormal = 0,		// src
	kRenderTransColor,		// c*a+dest*(1-a)
	kRenderTransTexture,	// src*a+dest*(1-a)
	kRenderGlow,		// src*a+dest -- No Z buffer checks
	kRenderTransAlpha,		// src*srca+dest*(1-srca)
	kRenderTransAdd,		// src*a+dest
	kRenderWorldGlow		// Same as kRenderGlow but not fixed size in screen space
};

enum RenderFx : int
{
	kRenderFxNone = 0,
	kRenderFxPulseSlow,
	kRenderFxPulseFast,
	kRenderFxPulseSlowWide,
	kRenderFxPulseFastWide,
	kRenderFxFadeSlow,
	kRenderFxFadeFast,
	kRenderFxSolidSlow,
	kRenderFxSolidFast,
	kRenderFxStrobeSlow,
	kRenderFxStrobeFast,
	kRenderFxStrobeFaster,
	kRenderFxFlickerSlow,
	kRenderFxFlickerFast,
	kRenderFxNoDissipation,
	kRenderFxDistort,			// Distort/scale/translate flicker
	kRenderFxHologram,			// kRenderFxDistort + distance fade
	kRenderFxDeadPlayer,		// kRenderAmt is the player index
	kRenderFxExplode,			// Scale up really big!
	kRenderFxGlowShell,			// Glowing Shell
	kRenderFxClampMinScale,		// Keep this sprite from getting very small (SPRITES only!)
	kRenderFxLightMultiplier    //CTM !!!CZERO added to tell the studiorender that the value in iuser2 is a lightmultiplier
};

#define MAXTEXELS 262144

#define CLAMP(v, min, max) if (v < min) { v = min; } else if (v > max) { v = max; }

struct COLOR3
{
	unsigned char r, g, b;

	COLOR3() : r(0), g(0), b(0) {};
	COLOR3(unsigned char r, unsigned char g, unsigned char b) : r(r), g(g), b(b)
	{}
};

struct COLOR4
{
	unsigned char r, g, b, a;
	COLOR4() : r(0), g(0), b(0), a(0) {};
	COLOR4(unsigned char r, unsigned char g, unsigned char b, unsigned char a) : r(r), g(g), b(b), a(a)
	{}
	COLOR4(const COLOR3& c, unsigned char a) : r(c.r), g(c.g), b(c.b), a(a)
	{}
	COLOR4(const COLOR3& c) : r(c.r), g(c.g), b(c.b), a(255)
	{}
};

#pragma pack(pop)