#pragma once
#include "Polygon3D.h"
#include "NavMesh.h"

class Bsp;
class PolygonOctree;

// generates a navigation mesh for a BSP
class NavMeshGenerator {
public:
	NavMeshGenerator() = default;

	// generate a nav mesh from the bsp
	// returns polygons used to construct the mesh
	NavMesh* generate(Bsp* map, int hull);

private:
	int octreeDepth = 6;

	// get faces of the hull that form the borders of the map
	std::vector<Polygon3D*> getHullFaces(Bsp* map, int hull);

	// get smallest octree box that can contain the entire map
	void getOctreeBox(Bsp* map, vec3& min, vec3& max);

	// group polys that are close together for fewer collision checks later
	PolygonOctree* createPolyOctree(Bsp* map, const std::vector<Polygon3D*>& faces, int treeDepth);

	// splits faces along their intersections with each other to clip polys that extend out
	// into the void, then tests each poly to see if it faces into the map or into the void.
	// Returns clipped faces that face the interior of the map
	std::vector<Polygon3D> getInteriorFaces(Bsp* map, int hull, std::vector<Polygon3D*>& faces);

	// merged polys adjacent to each other to reduce node count
	void mergeFaces(Bsp* map, std::vector<Polygon3D>& faces);

	// removes tiny faces
	void cullTinyFaces(std::vector<Polygon3D>& faces);

	// links nav polys that share an edge from a top-down view
	// climbability depends on game settings (gravity, stepsize, autoclimb, grapple/gauss weapon, etc.)
	void linkNavPolys(Bsp* map, NavMesh* mesh);

	// tests of polys can be linked by an overlapping edge from the top-down perspective
	// returns number of links created
	int tryEdgeLinkPolys(Bsp* map, NavMesh* mesh, int srcPolyIdx, int dstPolyIdx);
};