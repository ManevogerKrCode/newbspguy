#include "Bsp.h"
#include "util.h"
#include <algorithm>
#include <sstream>
#include "lodepng.h"
#include "rad.h"
#include "vis.h"
#include "remap.h"
#include "Renderer.h"
#include "BspRenderer.h"
#include <set>
#include <winding.h>
#include "Wad.h"
#include <vector>
#include "forcecrc32.h"

typedef std::map< std::string, vec3 > mapStringToVector;

vec3 default_hull_extents[MAX_MAP_HULLS] = {
	vec3(0,  0,  0),	// hull 0
	vec3(16, 16, 36),	// hull 1
	vec3(32, 32, 64),	// hull 2
	vec3(16, 16, 18)	// hull 3
};

int g_sort_mode = SORT_CLIPNODES;

void Bsp::init_empty_bsp()
{
	lumps = new unsigned char* [HEADER_LUMPS];

	bsp_header.nVersion = 30;

	for (int i = 0; i < HEADER_LUMPS; i++) {
		bsp_header.lump[i].nOffset = 0;
		if (i == LUMP_TEXTURES)
		{
			lumps[i] = new unsigned char[4];
			bsp_header.lump[i].nLength = 4;
			memset(lumps[i], 0, bsp_header.lump[i].nLength);
		}
		else if (i == LUMP_LIGHTING)
		{
			lumps[i] = new unsigned char[4096];
			bsp_header.lump[i].nLength = 4096;
			memset(lumps[i], 255, bsp_header.lump[i].nLength);
		}
		else
		{
			lumps[i] = new unsigned char[0]; // fix crash at replace_lump delete[]
			bsp_header.lump[i].nLength = 0;
		}
	}

	update_lump_pointers();
	bsp_name = "merged";
	bsp_valid = true;
	renderer = NULL;
}

void Bsp::selectModelEnt()
{
	if (!is_model || !ents.size())
		return;
	for (int i = 0; i < g_app->mapRenderers.size(); i++)
	{
		BspRenderer* mapRender = g_app->mapRenderers[i];
		if (!mapRender)
			continue;
		Bsp* map = mapRender->map;
		if (map && map != this)
		{
			vec3 worldOrigin = map->ents[0]->getOrigin();
			for (int n = 1; n < map->ents.size(); n++)
			{
				if (map->ents[n]->hasKey("model") && (map->ents[n]->getOrigin() + worldOrigin) == ents[0]->getOrigin())
				{
					g_app->clearSelection();
					g_app->selectMap(map);
					g_app->pickInfo.ent = map->ents[n];
					g_app->pickInfo.entIdx = n;
					return;
				}
			}
		}
	}
}

Bsp::Bsp() {
	this->init_empty_bsp();
}

Bsp::Bsp(std::string fpath)
{
	if (fpath.empty())
	{
		this->init_empty_bsp();
		return;
	}
	if (!fileExists(fpath)) {
		if (fpath.size() < 4 || fpath.rfind(".bsp") != fpath.size() - 4) {
			fpath = fpath + ".bsp";
		}
	}
	this->bsp_path = fpath;
	this->bsp_name = stripExt(basename(fpath));
	bsp_valid = false;

	if (!fileExists(fpath)) {
		logf("ERROR: %s not found\n", fpath.c_str());
		return;
	}

	if (!load_lumps(fpath)) {
		logf("%s is not a valid BSP file\n", fpath.c_str());
		return;
	}

	logf("(CRC \"%u\")\n", reverse_bits(originCrc32));

	load_ents();
	update_lump_pointers();

	if (modelCount > 0)
	{
		while (true)
		{
			BSPMODEL& lastModel = models[modelCount - 1];
			if (lastModel.nVisLeafs == 0 &&
				lastModel.iHeadnodes[0] == 0 &&
				lastModel.iHeadnodes[1] == 0 &&
				lastModel.iHeadnodes[2] == 0 &&
				lastModel.iHeadnodes[3] == 0 &&
				lastModel.iFirstFace == 0 &&
				abs(lastModel.vOrigin.z - 9999.0) < 0.01 &&
				lastModel.nFaces == 0)
			{
				logf("Removing CRC Hacking Model *%i\n", modelCount - 1);
				bsp_header.lump[LUMP_MODELS].nLength -= sizeof(BSPMODEL);
				update_lump_pointers();
			}
			else
				break;
		}
	}

	std::set<int> used_models; // Protected map
	used_models.insert(0);

	for (auto const& s : ents)
	{
		int ent_mdl_id = s->getBspModelIdx();
		if (ent_mdl_id >= 0)
		{
			if (!used_models.count(ent_mdl_id))
			{
				used_models.insert(ent_mdl_id);
			}
		}
	}

	for (unsigned int i = 0; i < modelCount; i++)
	{
		if (!used_models.count(i))
		{
			logf("Warning: in map %s found unused model: %d.\n", bsp_name.c_str(), i);
		}
	}


	renderer = NULL;
	bsp_valid = true;


	if (ents.size() && !ents[0]->hasKey("CRC"))
	{
		logf("Saving CRC key to Worldspawn\n");
		ents[0]->addKeyvalue("CRC", std::to_string(reverse_bits(originCrc32)));
		update_ent_lump();
	}

}

Bsp::~Bsp()
{
	for (int i = 0; i < HEADER_LUMPS; i++)
		if (lumps[i])
			delete[] lumps[i];
	delete[] lumps;

	for (int i = 0; i < ents.size(); i++)
		delete ents[i];
}



void Bsp::get_bounding_box(vec3& mins, vec3& maxs) {
	if (modelCount)
	{
		BSPMODEL& thisWorld = models[0];

		// the model bounds are little bigger than the actual vertices bounds in the map,
		// but if you go by the vertices then there will be collision problems.

		mins = thisWorld.nMins;
		maxs = thisWorld.nMaxs;
	}
	else
	{
		mins = maxs = vec3();
	}
}

void Bsp::get_model_vertex_bounds(int modelIdx, vec3& mins, vec3& maxs) {
	mins = vec3(FLT_MAX_COORD, FLT_MAX_COORD, FLT_MAX_COORD);
	maxs = vec3(FLT_MIN_COORD, FLT_MIN_COORD, FLT_MIN_COORD);

	BSPMODEL& model = models[modelIdx];
	/*auto verts = getModelVerts(modelIdx);
	for (auto const& s : verts)
	{
		if (s.pos.x < mins.x)
		{
			mins.x = s.pos.x;
		}
		if (s.pos.y < mins.y)
		{
			mins.y = s.pos.y;
		}
		if (s.pos.z < mins.z)
		{
			mins.z = s.pos.z;
		}

		if (s.pos.x > maxs.x)
		{
			maxs.x = s.pos.x;
		}
		if (s.pos.y > maxs.y)
		{
			maxs.y = s.pos.y;
		}
		if (s.pos.z > maxs.z)
		{
			maxs.z = s.pos.z;
		}
	}
	*/
	for (int i = 0; i < model.nFaces; i++) {
		BSPFACE& face = faces[model.iFirstFace + i];

		for (int e = 0; e < face.nEdges; e++) {
			int edgeIdx = surfedges[face.iFirstEdge + e];
			BSPEDGE& edge = edges[abs(edgeIdx)];
			int vertIdx = edgeIdx >= 0 ? edge.iVertex[1] : edge.iVertex[0];

			expandBoundingBox(verts[vertIdx], mins, maxs);
		}
	}
}

std::vector<TransformVert> Bsp::getModelVerts(int modelIdx) {
	std::vector<TransformVert> allVerts;
	std::set<int> visited;

	BSPMODEL& model = models[modelIdx];

	for (int i = 0; i < model.nFaces; i++) {
		BSPFACE& face = faces[model.iFirstFace + i];

		for (int e = 0; e < face.nEdges; e++) {
			int edgeIdx = surfedges[face.iFirstEdge + e];
			BSPEDGE& edge = edges[abs(edgeIdx)];
			int vertIdx = edgeIdx >= 0 ? edge.iVertex[1] : edge.iVertex[0];

			if (visited.find(vertIdx) == visited.end()) {
				TransformVert vert = TransformVert();
				vert.startPos = vert.undoPos = vert.pos = verts[vertIdx];
				vert.ptr = &verts[vertIdx];

				allVerts.push_back(vert);
				visited.insert(vertIdx);
			}
		}
	}

	return allVerts;
}

bool Bsp::getModelPlaneIntersectVerts(int modelIdx, std::vector<TransformVert>& outVerts) {
	std::vector<int> nodePlaneIndexes;
	BSPMODEL& model = models[modelIdx];
	getNodePlanes(model.iHeadnodes[0], nodePlaneIndexes);

	return getModelPlaneIntersectVerts(modelIdx, nodePlaneIndexes, outVerts);
}

bool Bsp::getModelPlaneIntersectVerts(int modelIdx, const std::vector<int>& nodePlaneIndexes, std::vector<TransformVert>& outVerts) {
	// TODO: this only works for convex objects. A concave solid will need
	// to get verts by creating convex hulls from each solid node in the tree.
	// That can be done by recursively cutting a huge cube but there's probably
	// a better way.
	std::vector<BSPPLANE> nodePlanes;

	BSPMODEL& model = models[modelIdx];

	// TODO: model center doesn't have to be inside all planes, even for convex objects(?)
	vec3 modelCenter = model.nMins + (model.nMaxs - model.nMins) * 0.5f;
	for (int i = 0; i < nodePlaneIndexes.size(); i++) {
		nodePlanes.push_back(planes[nodePlaneIndexes[i]]);
		BSPPLANE& plane = nodePlanes[i];
		vec3 planePoint = plane.vNormal * plane.fDist;
		vec3 planeDir = (planePoint - modelCenter).normalize(1.0f);
		if (dotProduct(planeDir, plane.vNormal) > 0) {
			plane.vNormal *= -1;
			plane.fDist *= -1;
		}
	}

	std::vector<vec3> nodeVerts = getPlaneIntersectVerts(nodePlanes);

	if (nodeVerts.size() < 4) {
		return false; // solid is either 2D or there were no intersections (not convex)
	}

	// coplanar test
	for (int i = 0; i < nodePlanes.size(); i++) {
		for (int k = 0; k < nodePlanes.size(); k++) {
			if (i == k)
				continue;

			if (nodePlanes[i].vNormal == nodePlanes[k].vNormal && nodePlanes[i].fDist - nodePlanes[k].fDist < EPSILON) {
				return false;
			}
		}
	}

	// convex test
	for (int k = 0; k < nodePlanes.size(); k++) {
		if (!vertsAllOnOneSide(nodeVerts, nodePlanes[k])) {
			return false;
		}
	}

	outVerts.clear();
	for (int k = 0; k < nodeVerts.size(); k++) {
		vec3 v = nodeVerts[k];

		TransformVert hullVert;
		hullVert.pos = hullVert.undoPos = hullVert.startPos = v;
		hullVert.ptr = NULL;
		hullVert.selected = false;

		for (int i = 0; i < nodePlanes.size(); i++) {
			BSPPLANE& p = nodePlanes[i];
			if (abs(dotProduct(v, p.vNormal) - p.fDist) < EPSILON) {
				hullVert.iPlanes.push_back(nodePlaneIndexes[i]);
			}
		}

		for (int i = 0; i < model.nFaces && !hullVert.ptr; i++) {
			BSPFACE& face = faces[model.iFirstFace + i];

			for (int e = 0; e < face.nEdges && !hullVert.ptr; e++) {
				int edgeIdx = surfedges[face.iFirstEdge + e];
				BSPEDGE& edge = edges[abs(edgeIdx)];
				int vertIdx = edgeIdx >= 0 ? edge.iVertex[1] : edge.iVertex[0];

				if (verts[vertIdx] != v) {
					continue;
				}

				hullVert.ptr = &verts[vertIdx];
			}
		}

		outVerts.push_back(hullVert);
	}

	return true;
}

void Bsp::getNodePlanes(int iNode, std::vector<int>& nodePlanes) {
	BSPNODE& node = nodes[iNode];
	nodePlanes.push_back(node.iPlane);

	for (int i = 0; i < 2; i++) {
		if (node.iChildren[i] >= 0) {
			getNodePlanes(node.iChildren[i], nodePlanes);
		}
	}
}

void Bsp::getClipNodePlanes(int iClipNode, std::vector<int>& nodePlanes) {
	BSPCLIPNODE& node = clipnodes[iClipNode];
	nodePlanes.push_back(node.iPlane);

	for (int i = 0; i < 2; i++) {
		if (node.iChildren[i] >= 0) {
			getClipNodePlanes(node.iChildren[i], nodePlanes);
		}
	}
}

std::vector<NodeVolumeCuts> Bsp::get_model_leaf_volume_cuts(int modelIdx, int hullIdx) {
	std::vector<NodeVolumeCuts> modelVolumeCuts;

	if (hullIdx >= 0 && hullIdx < MAX_MAP_HULLS)
	{
		int nodeIdx = models[modelIdx].iHeadnodes[hullIdx];
		bool is_valid_node = false;

		if (hullIdx == 0) {
			is_valid_node = nodeIdx >= 0 && nodeIdx < (int)nodeCount;
		}
		else {
			is_valid_node = nodeIdx >= 0 && nodeIdx < (int)clipnodeCount;
		}

		if (nodeIdx >= 0 && is_valid_node) {
			std::vector<BSPPLANE> clipOrder;
			if (hullIdx == 0) {
				get_node_leaf_cuts(nodeIdx, clipOrder, modelVolumeCuts);
			}
			else {
				get_clipnode_leaf_cuts(nodeIdx, clipOrder, modelVolumeCuts);
			}
		}
	}
	return modelVolumeCuts;
}

void Bsp::get_clipnode_leaf_cuts(int iNode, std::vector<BSPPLANE>& clipOrder, std::vector<NodeVolumeCuts>& output) {
	BSPCLIPNODE& node = clipnodes[iNode];

	if (node.iPlane < 0) {
		return;
	}

	for (int i = 0; i < 2; i++) {
		BSPPLANE plane = planes[node.iPlane];
		if (i != 0) {
			plane.vNormal = plane.vNormal.invert();
			plane.fDist = -plane.fDist;
		}
		clipOrder.push_back(plane);

		if (node.iChildren[i] >= 0) {
			get_clipnode_leaf_cuts(node.iChildren[i], clipOrder, output);
		}
		else if (node.iChildren[i] != CONTENTS_EMPTY) {
			NodeVolumeCuts nodeVolumeCuts;
			nodeVolumeCuts.nodeIdx = iNode;

			if (clipOrder.size())
			{
				// reverse order of branched planes = order of cuts to the world which define this node's volume
				// https://qph.fs.quoracdn.net/main-qimg-2a8faad60cc9d437b58a6e215e6e874d
				for (int k = (int)clipOrder.size() - 1; k >= 0; k--) {
					nodeVolumeCuts.cuts.push_back(clipOrder[k]);
				}
			}
			output.push_back(nodeVolumeCuts);
		}

		clipOrder.pop_back();
	}
}

void Bsp::get_node_leaf_cuts(int iNode, std::vector<BSPPLANE>& clipOrder, std::vector<NodeVolumeCuts>& output) {
	BSPNODE& node = nodes[iNode];

	for (int i = 0; i < 2; i++) {
		BSPPLANE plane = planes[node.iPlane];
		if (i != 0) {
			plane.vNormal = plane.vNormal.invert();
			plane.fDist = -plane.fDist;
		}
		clipOrder.push_back(plane);

		if (node.iChildren[i] >= 0) {
			get_node_leaf_cuts(node.iChildren[i], clipOrder, output);
		}
		else if (leaves[~node.iChildren[i]].nContents != CONTENTS_EMPTY) {
			NodeVolumeCuts nodeVolumeCuts;
			nodeVolumeCuts.nodeIdx = iNode;
			if (clipOrder.size())
			{
				// reverse order of branched planes = order of cuts to the world which define this node's volume
				// https://qph.fs.quoracdn.net/main-qimg-2a8faad60cc9d437b58a6e215e6e874d
				for (int k = (int)clipOrder.size() - 1; k >= 0; k--) {
					nodeVolumeCuts.cuts.push_back(clipOrder[k]);
				}
			}
			output.push_back(nodeVolumeCuts);
		}

		clipOrder.pop_back();
	}
}

bool Bsp::is_convex(int modelIdx) {
	return models[modelIdx].iHeadnodes[0] >= 0 && is_node_hull_convex(models[modelIdx].iHeadnodes[0]);
}

bool Bsp::is_node_hull_convex(int iNode) {
	BSPNODE& node = nodes[iNode];

	// convex models always have one node pointing to empty space
	if (node.iChildren[0] >= 0 && node.iChildren[1] >= 0) {
		return false;
	}

	for (int i = 0; i < 2; i++) {
		if (node.iChildren[i] >= 0) {
			if (!is_node_hull_convex(node.iChildren[i])) {
				return false;
			}
		}
	}

	return true;
}

int Bsp::addTextureInfo(BSPTEXTUREINFO& copy) {
	BSPTEXTUREINFO* newInfos = new BSPTEXTUREINFO[texinfoCount + 1];
	memcpy(newInfos, texinfos, texinfoCount * sizeof(BSPTEXTUREINFO));

	int newIdx = texinfoCount;
	newInfos[newIdx] = copy;

	replace_lump(LUMP_TEXINFO, newInfos, (texinfoCount + 1) * sizeof(BSPTEXTUREINFO));

	return newIdx;
}

std::vector<ScalableTexinfo> Bsp::getScalableTexinfos(int modelIdx) {
	BSPMODEL& model = models[modelIdx];
	std::vector<ScalableTexinfo> scalable;
	std::set<int> visitedTexinfos;

	for (int k = 0; k < model.nFaces; k++) {
		BSPFACE& face = faces[model.iFirstFace + k];
		int texinfoIdx = face.iTextureInfo;

		if (visitedTexinfos.find(texinfoIdx) != visitedTexinfos.end()) {
			continue;
			//texinfoIdx = face.iTextureInfo = addTextureInfo(texinfos[texinfoIdx]);
		}
		visitedTexinfos.insert(texinfoIdx);

		ScalableTexinfo st;
		st.oldS = texinfos[texinfoIdx].vS;
		st.oldT = texinfos[texinfoIdx].vT;
		st.oldShiftS = texinfos[texinfoIdx].shiftS;
		st.oldShiftT = texinfos[texinfoIdx].shiftT;
		st.texinfoIdx = texinfoIdx;
		st.planeIdx = face.iPlane;
		st.faceIdx = model.iFirstFace + k;
		scalable.push_back(st);
	}

	return scalable;
}

bool Bsp::vertex_manipulation_sync(int modelIdx, std::vector<TransformVert>& hullVerts, bool convexCheckOnly, bool regenClipnodes) {
	std::set<int> affectedPlanes;

	std::map<int, std::vector<vec3>> planeVerts;
	std::vector<vec3> allVertPos;

	for (int i = 0; i < hullVerts.size(); i++) {
		for (int k = 0; k < hullVerts[i].iPlanes.size(); k++) {
			int iPlane = hullVerts[i].iPlanes[k];
			affectedPlanes.insert(hullVerts[i].iPlanes[k]);
			planeVerts[iPlane].push_back(hullVerts[i].pos);
		}
		allVertPos.push_back(hullVerts[i].pos);
	}

	int planeUpdates = 0;
	std::map<int, BSPPLANE> newPlanes;
	std::map<int, bool> shouldFlipChildren;
	for (auto it = planeVerts.begin(); it != planeVerts.end(); ++it) {
		int iPlane = it->first;

		std::vector<vec3>& tverts = it->second;

		if (tverts.size() < 3) {
			logf("Face has less than 3 verts\n");
			return false; // invalid solid
		}

		BSPPLANE newPlane;
		if (!getPlaneFromVerts(tverts, newPlane.vNormal, newPlane.fDist)) {
			logf("Verts not planar\n");
			return false; // verts not planar
		}

		vec3 oldNormal = planes[iPlane].vNormal;
		if (dotProduct(oldNormal, newPlane.vNormal) < 0) {
			newPlane.vNormal = newPlane.vNormal.invert(); // TODO: won't work for big changes
			newPlane.fDist = -newPlane.fDist;
		}

		BSPPLANE testPlane;
		bool expectedFlip = testPlane.update(planes[iPlane].vNormal, planes[iPlane].fDist);
		bool flipped = newPlane.update(newPlane.vNormal, newPlane.fDist);

		testPlane = newPlane;

		// check that all verts are on one side of the plane.
		// plane inversions are ok according to hammer
		if (!vertsAllOnOneSide(allVertPos, testPlane)) {
			return false;
		}

		newPlanes[iPlane] = newPlane;
		shouldFlipChildren[iPlane] = flipped != expectedFlip;
	}

	if (convexCheckOnly)
		return true;

	for (auto it = newPlanes.begin(); it != newPlanes.end(); ++it) {
		auto iPlane = it->first;
		BSPPLANE& newPlane = it->second;

		planes[iPlane] = newPlane;
		planeUpdates++;

		if (shouldFlipChildren[iPlane]) {
			for (unsigned int i = 0; i < faceCount; i++) {
				BSPFACE& face = faces[i];
				if (face.iPlane == iPlane) {
					face.nPlaneSide = face.nPlaneSide ? 0 : 1;
				}
			}
			for (unsigned int i = 0; i < nodeCount; i++) {
				BSPNODE& node = nodes[i];
				if (node.iPlane == iPlane) {
					short temp = node.iChildren[0];
					node.iChildren[0] = node.iChildren[1];
					node.iChildren[1] = temp;
				}
			}
		}
	}

	//logf("UPDATED %d planes\n", planeUpdates);

	BSPMODEL& model = models[modelIdx];
	getBoundingBox(allVertPos, model.nMins, model.nMaxs);

	if (!regenClipnodes)
		return true;

	regenerate_clipnodes(modelIdx, -1);

	return true;
}

bool Bsp::move(vec3 offset, int modelIdx, bool onlyModel) {
	if (modelIdx < 0 || modelIdx >= (int)modelCount) {
		logf("Invalid modelIdx moved");
		return false;
	}

	BSPMODEL& target = models[modelIdx];

	// all ents should be moved if the world is being moved
	bool movingWorld = modelIdx == 0 && !onlyModel;

	// Submodels don't use leaves like the world model does. Only the contents of a leaf matters
	// for submodels. All other data is ignored. bspguy will reuse world leaves in submodels to 
	// save space, which means moving leaves for those models would likely break something else.
	// So, don't move leaves for submodels.
	bool dontMoveLeaves = !movingWorld;

	split_shared_model_structures(modelIdx);

	bool hasLighting = lightDataLength > 0;
	LIGHTMAP* oldLightmaps = NULL;
	LIGHTMAP* newLightmaps = NULL;

	if (hasLighting) {
		g_progress.update("Calculate lightmaps", faceCount);

		oldLightmaps = new LIGHTMAP[faceCount];
		newLightmaps = new LIGHTMAP[faceCount];
		memset(oldLightmaps, 0, sizeof(LIGHTMAP) * faceCount);
		memset(newLightmaps, 0, sizeof(LIGHTMAP) * faceCount);

		for (int i = 0; i < (int)faceCount; i++) {
			int size[2];
			GetFaceLightmapSize(this, i, size);

			int lightmapSz = size[0] * size[1];
			int lightmapCount = lightmap_count(i);
			oldLightmaps[i].layers = lightmapCount;
			lightmapSz *= lightmapCount;

			oldLightmaps[i].width = size[0];
			oldLightmaps[i].height = size[1];

			bool skipResize = i < target.iFirstFace || i >= target.iFirstFace + target.nFaces;

			if (!skipResize) {
				oldLightmaps[i].luxelFlags = new unsigned char[size[0] * size[1]];
				qrad_get_lightmap_flags(this, i, oldLightmaps[i].luxelFlags);
			}

			g_progress.tick();
		}
	}

	g_progress.update("Moving structures", (int)(ents.size() - 1));

	if (movingWorld) {
		for (int i = 1; i < ents.size(); i++) { // don't move the world entity
			g_progress.tick();

			vec3 ori;
			if (ents[i]->hasKey("origin")) {
				ori = parseVector(ents[i]->keyvalues["origin"]);
			}
			ori += offset;

			ents[i]->setOrAddKeyvalue("origin", ori.toKeyvalueString());

			if (ents[i]->hasKey("spawnorigin")) {
				vec3 spawnori = parseVector(ents[i]->keyvalues["spawnorigin"]);

				// entity not moved if destination is 0,0,0
				if (abs(spawnori.x) >= EPSILON || abs(spawnori.y) >= EPSILON || abs(spawnori.z) >= EPSILON) {
					ents[i]->setOrAddKeyvalue("spawnorigin", (spawnori + offset).toKeyvalueString());
				}
			}
		}

		update_ent_lump();
	}

	target.nMins += offset;
	target.nMaxs += offset;
	if (abs(target.nMins.x) > MAX_MAP_COORD ||
		abs(target.nMins.y) > MAX_MAP_COORD ||
		abs(target.nMins.z) > MAX_MAP_COORD ||
		abs(target.nMaxs.x) > MAX_MAP_COORD ||
		abs(target.nMaxs.y) > MAX_MAP_COORD ||
		abs(target.nMaxs.z) > MAX_MAP_COORD) {
		logf("\nWARNING: Model moved past safe world boundary!\n");
	}

	STRUCTUSAGE shouldBeMoved(this);
	mark_model_structures(modelIdx, &shouldBeMoved, dontMoveLeaves);


	for (unsigned int i = 0; i < nodeCount; i++) {
		if (!shouldBeMoved.nodes[i]) {
			continue;
		}

		BSPNODE& node = nodes[i];

		if (abs((float)node.nMins[0] + offset.x) > MAX_MAP_COORD ||
			abs((float)node.nMaxs[0] + offset.x) > MAX_MAP_COORD ||
			abs((float)node.nMins[1] + offset.y) > MAX_MAP_COORD ||
			abs((float)node.nMaxs[1] + offset.y) > MAX_MAP_COORD ||
			abs((float)node.nMins[2] + offset.z) > MAX_MAP_COORD ||
			abs((float)node.nMaxs[2] + offset.z) > MAX_MAP_COORD) {
			logf("\nWARNING: Bounding box for node moved past safe world boundary!\n");
		}
		node.nMins[0] += (short)offset.x;
		node.nMaxs[0] += (short)offset.x;
		node.nMins[1] += (short)offset.y;
		node.nMaxs[1] += (short)offset.y;
		node.nMins[2] += (short)offset.z;
		node.nMaxs[2] += (short)offset.z;
	}

	for (unsigned int i = 1; i < leafCount; i++) { // don't move the solid leaf (always has 0 size)
		if (!shouldBeMoved.leaves[i]) {
			continue;
		}

		BSPLEAF& leaf = leaves[i];

		if (abs((float)leaf.nMins[0] + offset.x) > MAX_MAP_COORD ||
			abs((float)leaf.nMaxs[0] + offset.x) > MAX_MAP_COORD ||
			abs((float)leaf.nMins[1] + offset.y) > MAX_MAP_COORD ||
			abs((float)leaf.nMaxs[1] + offset.y) > MAX_MAP_COORD ||
			abs((float)leaf.nMins[2] + offset.z) > MAX_MAP_COORD ||
			abs((float)leaf.nMaxs[2] + offset.z) > MAX_MAP_COORD) {
			logf("\nWARNING: Bounding box for leaf moved past safe world boundary!\n");
		}
		leaf.nMins[0] += (short)offset.x;
		leaf.nMaxs[0] += (short)offset.x;
		leaf.nMins[1] += (short)offset.y;
		leaf.nMaxs[1] += (short)offset.y;
		leaf.nMins[2] += (short)offset.z;
		leaf.nMaxs[2] += (short)offset.z;
	}

	for (unsigned int i = 0; i < vertCount; i++) {
		if (!shouldBeMoved.verts[i]) {
			continue;
		}

		vec3& vert = verts[i];

		vert += offset;

		if (abs(vert.x) > MAX_MAP_COORD ||
			abs(vert.y) > MAX_MAP_COORD ||
			abs(vert.z) > MAX_MAP_COORD) {
			logf("\nWARNING: Vertex moved past safe world boundary!\n");
		}
	}

	for (unsigned int i = 0; i < planeCount; i++) {
		if (!shouldBeMoved.planes[i]) {
			continue; // don't move submodels with origins
		}

		BSPPLANE& plane = planes[i];
		vec3 newPlaneOri = offset + (plane.vNormal * plane.fDist);

		if (abs(newPlaneOri.x) > MAX_MAP_COORD || abs(newPlaneOri.y) > MAX_MAP_COORD ||
			abs(newPlaneOri.z) > MAX_MAP_COORD) {
			logf("\nWARNING: Plane origin moved past safe world boundary!\n");
		}

		// get distance between new plane origin and the origin-aligned plane
		plane.fDist = dotProduct(plane.vNormal, newPlaneOri) / dotProduct(plane.vNormal, plane.vNormal);
	}

	for (unsigned int i = 0; i < texinfoCount; i++) {
		if (!shouldBeMoved.texInfo[i]) {
			continue; // don't move submodels with origins
		}

		move_texinfo(i, offset);
	}

	if (hasLighting && oldLightmaps && newLightmaps) {
		resize_lightmaps(oldLightmaps, newLightmaps);

		for (unsigned int i = 0; i < faceCount; i++) {
			if (oldLightmaps[i].luxelFlags) {
				delete[] oldLightmaps[i].luxelFlags;
			}
			if (newLightmaps[i].luxelFlags) {
				delete[] newLightmaps[i].luxelFlags;
			}
		}
		delete[] oldLightmaps;
		delete[] newLightmaps;
	}

	g_progress.clear();

	return true;
}

void Bsp::move_texinfo(int idx, vec3 offset) {
	BSPTEXTUREINFO& info = texinfos[idx];

	int texOffset = ((int*)textures)[info.iMiptex + 1];
	BSPMIPTEX& tex = *((BSPMIPTEX*)(textures + texOffset));

	vec3 offsetDir = offset.normalize();
	float offsetLen = offset.length();

	float scaleS = info.vS.length();
	float scaleT = info.vT.length();
	vec3 nS = info.vS.normalize();
	vec3 nT = info.vT.normalize();

	vec3 newOriS = offset + (nS * info.shiftS);
	vec3 newOriT = offset + (nT * info.shiftT);

	float shiftScaleS = dotProduct(offsetDir, nS);
	float shiftScaleT = dotProduct(offsetDir, nT);

	float shiftAmountS = shiftScaleS * offsetLen * scaleS;
	float shiftAmountT = shiftScaleT * offsetLen * scaleT;

	info.shiftS -= shiftAmountS;
	info.shiftT -= shiftAmountT;

	// minimize shift values (just to be safe. floats can be p wacky and zany)
	while (abs(info.shiftS) > tex.nWidth) {
		info.shiftS += (info.shiftS < 0) ? (int)tex.nWidth : -(int)(tex.nWidth);
	}
	while (abs(info.shiftT) > tex.nHeight) {
		info.shiftT += (info.shiftT < 0) ? (int)tex.nHeight : -(int)(tex.nHeight);
	}
}

void Bsp::resize_lightmaps(LIGHTMAP* oldLightmaps, LIGHTMAP* newLightmaps) {
	g_progress.update("Recalculate lightmaps", faceCount);

	// calculate new lightmap sizes
	int newLightDataSz = 0;
	int totalLightmaps = 0;
	int lightmapsResizeCount = 0;
	for (unsigned int i = 0; i < faceCount; i++) {
		BSPFACE& face = faces[i];

		g_progress.tick();

		if (lightmap_count(i) == 0)
			continue;

		BSPTEXTUREINFO& info = texinfos[face.iTextureInfo];
		int texOffset = ((int*)textures)[info.iMiptex + 1];

		int size[2];
		GetFaceLightmapSize(this, i, size);

		int lightmapSz = size[0] * size[1];

		newLightmaps[i].width = size[0];
		newLightmaps[i].height = size[1];
		newLightmaps[i].layers = oldLightmaps[i].layers;

		newLightDataSz += (lightmapSz * newLightmaps[i].layers) * sizeof(COLOR3);

		totalLightmaps += newLightmaps[i].layers;
		if (oldLightmaps[i].width != newLightmaps[i].width || oldLightmaps[i].height != newLightmaps[i].height) {
			lightmapsResizeCount += newLightmaps[i].layers;
		}
	}

	if (lightmapsResizeCount > 0) {
		//logf("%d lightmap(s) to resize\n", lightmapsResizeCount);

		g_progress.update("Resize lightmaps", faceCount);

		int newColorCount = newLightDataSz / sizeof(COLOR3);
		COLOR3* newLightData = new COLOR3[newColorCount];
		memset(newLightData, 255, newColorCount * sizeof(COLOR3));
		int lightmapOffset = 0;

		for (unsigned int i = 0; i < faceCount; i++) {
			BSPFACE& face = faces[i];

			g_progress.tick();

			if (lightmap_count(i) == 0) // no lighting
				continue;

			LIGHTMAP& oldLight = oldLightmaps[i];
			LIGHTMAP& newLight = newLightmaps[i];
			int oldLayerSz = (oldLight.width * oldLight.height) * sizeof(COLOR3);
			int newLayerSz = (newLight.width * newLight.height) * sizeof(COLOR3);
			int oldSz = oldLayerSz * oldLight.layers;
			int newSz = newLayerSz * newLight.layers;

			totalLightmaps++;

			bool faceMoved = oldLightmaps[i].luxelFlags;
			bool lightmapResized = oldLight.width != newLight.width || oldLight.height != newLight.height;

			if (!faceMoved || !lightmapResized) {
				memcpy((unsigned char*)newLightData + lightmapOffset, (unsigned char*)lightdata + face.nLightmapOffset, oldSz);
				newLight.luxelFlags = NULL;
			}
			else {
				newLight.luxelFlags = new unsigned char[newLight.width * newLight.height];
				qrad_get_lightmap_flags(this, i, newLight.luxelFlags);

				int srcOffsetX, srcOffsetY;
				get_lightmap_shift(oldLight, newLight, srcOffsetX, srcOffsetY);

				for (int layer = 0; layer < newLight.layers; layer++) {
					int srcOffset = (face.nLightmapOffset + oldLayerSz * layer) / sizeof(COLOR3);
					int dstOffset = (lightmapOffset + newLayerSz * layer) / sizeof(COLOR3);

					int startX = newLight.width > oldLight.width ? -1 : 0;
					int startY = newLight.height > oldLight.height ? -1 : 0;

					for (int y = startY; y < newLight.height; y++) {
						for (int x = startX; x < newLight.width; x++) {
							int offsetX = x + srcOffsetX;
							int offsetY = y + srcOffsetY;

							int srcX = oldLight.width > newLight.width ? offsetX : x;
							int srcY = oldLight.height > newLight.height ? offsetY : y;
							int dstX = newLight.width > oldLight.width ? offsetX : x;
							int dstY = newLight.height > oldLight.height ? offsetY : y;

							srcX = std::max(0, std::min(oldLight.width - 1, srcX));
							srcY = std::max(0, std::min(oldLight.height - 1, srcY));
							dstX = std::max(0, std::min(newLight.width - 1, dstX));
							dstY = std::max(0, std::min(newLight.height - 1, dstY));

							COLOR3& src = ((COLOR3*)lightdata)[srcOffset + srcY * oldLight.width + srcX];
							COLOR3& dst = newLightData[dstOffset + dstY * newLight.width + dstX];

							dst = src;
						}
					}
				}
			}

			face.nLightmapOffset = lightmapOffset;
			lightmapOffset += newSz;
		}

		replace_lump(LUMP_LIGHTING, newLightData, lightmapOffset);
	}
}

void Bsp::split_shared_model_structures(int modelIdx) {
	// marks which structures should not be moved
	STRUCTUSAGE shouldMove(this);
	STRUCTUSAGE shouldNotMove(this);

	g_progress.update("Split model structures", modelCount);

	mark_model_structures(modelIdx, &shouldMove, modelIdx == 0);
	for (unsigned int i = 0; i < modelCount; i++) {
		if (i != modelIdx)
			mark_model_structures(i, &shouldNotMove, false);

		g_progress.tick();
	}

	STRUCTREMAP remappedStuff(this);

	// TODO: handle all of these, assuming it's possible these are ever shared
	for (unsigned int i = 1; i < shouldNotMove.count.leaves; i++) { // skip solid leaf - it doesn't matter
		if (shouldMove.leaves[i] && shouldNotMove.leaves[i]) {
			logf("\nWarning: leaf shared with multiple models. Something might break.\n");
			break;
		}
	}
	for (unsigned int i = 0; i < shouldNotMove.count.nodes; i++) {
		if (shouldMove.nodes[i] && shouldNotMove.nodes[i]) {
			logf("\nError: node shared with multiple models. Something will break.\n");
			break;
		}
	}
	for (unsigned int i = 0; i < shouldNotMove.count.verts; i++) {
		if (shouldMove.verts[i] && shouldNotMove.verts[i]) {
			// this happens on activist series but doesn't break anything
			logf("\nError: vertex shared with multiple models. Something will break.\n");
			break;
		}
	}

	int duplicatePlanes = 0;
	int duplicateClipnodes = 0;
	int duplicateTexinfos = 0;

	for (unsigned int i = 0; i < shouldNotMove.count.planes; i++) {
		duplicatePlanes += shouldMove.planes[i] && shouldNotMove.planes[i];
	}
	for (unsigned int i = 0; i < shouldNotMove.count.clipnodes; i++) {
		duplicateClipnodes += shouldMove.clipnodes[i] && shouldNotMove.clipnodes[i];
	}
	for (unsigned int i = 0; i < shouldNotMove.count.texInfos; i++) {
		duplicateTexinfos += shouldMove.texInfo[i] && shouldNotMove.texInfo[i];
	}

	int newPlaneCount = planeCount + duplicatePlanes;
	int newClipnodeCount = clipnodeCount + duplicateClipnodes;
	int newTexinfoCount = texinfoCount + duplicateTexinfos;

	BSPPLANE* newPlanes = new BSPPLANE[newPlaneCount];
	memcpy(newPlanes, planes, planeCount * sizeof(BSPPLANE));

	BSPCLIPNODE* newClipnodes = new BSPCLIPNODE[newClipnodeCount];
	memcpy(newClipnodes, clipnodes, clipnodeCount * sizeof(BSPCLIPNODE));

	BSPTEXTUREINFO* newTexinfos = new BSPTEXTUREINFO[newTexinfoCount];
	memcpy(newTexinfos, texinfos, newTexinfoCount * sizeof(BSPTEXTUREINFO));

	int addIdx = planeCount;
	for (unsigned int i = 0; i < shouldNotMove.count.planes; i++) {
		if (shouldMove.planes[i] && shouldNotMove.planes[i]) {
			newPlanes[addIdx] = planes[i];
			remappedStuff.planes[i] = addIdx;
			addIdx++;
		}
	}

	addIdx = clipnodeCount;
	for (unsigned int i = 0; i < shouldNotMove.count.clipnodes; i++) {
		if (shouldMove.clipnodes[i] && shouldNotMove.clipnodes[i]) {
			newClipnodes[addIdx] = clipnodes[i];
			remappedStuff.clipnodes[i] = addIdx;
			addIdx++;
		}
	}

	addIdx = texinfoCount;
	for (unsigned int i = 0; i < shouldNotMove.count.texInfos; i++) {
		if (shouldMove.texInfo[i] && shouldNotMove.texInfo[i]) {
			newTexinfos[addIdx] = texinfos[i];
			remappedStuff.texInfo[i] = addIdx;
			addIdx++;
		}
	}

	replace_lump(LUMP_PLANES, newPlanes, newPlaneCount * sizeof(BSPPLANE));
	replace_lump(LUMP_CLIPNODES, newClipnodes, newClipnodeCount * sizeof(BSPCLIPNODE));
	replace_lump(LUMP_TEXINFO, newTexinfos, newTexinfoCount * sizeof(BSPTEXTUREINFO));

	bool* newVisitedClipnodes = new bool[newClipnodeCount];
	memset(newVisitedClipnodes, 0, newClipnodeCount);
	delete[] remappedStuff.visitedClipnodes;
	remappedStuff.visitedClipnodes = newVisitedClipnodes;

	remap_model_structures(modelIdx, &remappedStuff);

	if (duplicatePlanes || duplicateClipnodes || duplicateTexinfos) {
		debugf("\nShared model structures were duplicated to allow independent movement:\n");
		if (duplicatePlanes)
			debugf("    Added %d planes\n", duplicatePlanes);
		if (duplicateClipnodes)
			debugf("    Added %d clipnodes\n", duplicateClipnodes);
		if (duplicateTexinfos)
			debugf("    Added %d texinfos\n", duplicateTexinfos);
	}
}

bool Bsp::does_model_use_shared_structures(int modelIdx) {
	STRUCTUSAGE shouldMove(this);
	STRUCTUSAGE shouldNotMove(this);

	for (unsigned int i = 0; i < modelCount; i++) {
		if (i == modelIdx)
			mark_model_structures(i, &shouldMove, true);
		else
			mark_model_structures(i, &shouldNotMove, false);
	}

	for (unsigned int i = 0; i < planeCount; i++) {
		if (shouldMove.planes[i] && shouldNotMove.planes[i]) {
			return true;
		}
	}
	for (unsigned int i = 0; i < clipnodeCount; i++) {
		if (shouldMove.clipnodes[i] && shouldNotMove.clipnodes[i]) {
			return true;
		}
	}
	return false;
}

LumpState Bsp::duplicate_lumps(int targets) {
	LumpState state;

	for (int i = 0; i < HEADER_LUMPS; i++) {
		if ((targets & (1 << i)) == 0) {
			state.lumps[i] = NULL;
			state.lumpLen[i] = 0;
			continue;
		}
		state.lumps[i] = new unsigned char[bsp_header.lump[i].nLength];
		state.lumpLen[i] = bsp_header.lump[i].nLength;
		memcpy(state.lumps[i], lumps[i], bsp_header.lump[i].nLength);
	}

	return state;
}

int Bsp::delete_embedded_textures() {
	unsigned int headerSz = (textureCount + 1) * sizeof(int);
	unsigned int newTexDataSize = headerSz + (textureCount * sizeof(BSPMIPTEX));
	unsigned char* newTextureData = new unsigned char[newTexDataSize];

	BSPMIPTEX* mips = (BSPMIPTEX*)(newTextureData + headerSz);

	int* header = (int*)newTextureData;
	*header = textureCount;
	header++;

	int numRemoved = 0;

	for (unsigned int i = 0; i < textureCount; i++) {
		int oldOffset = ((int*)textures)[i + 1];
		BSPMIPTEX* oldTex = (BSPMIPTEX*)(textures + oldOffset);

		if (oldTex->nOffsets[0] != -1) {
			numRemoved++;
		}

		header[i] = headerSz + i * sizeof(BSPMIPTEX);
		mips[i].nWidth = oldTex->nWidth;
		mips[i].nHeight = oldTex->nHeight;
		memcpy(mips[i].szName, oldTex->szName, MAXTEXTURENAME);
		memset(mips[i].nOffsets, 0, MIPLEVELS * sizeof(int));
	}

	replace_lump(LUMP_TEXTURES, newTextureData, newTexDataSize);

	return numRemoved;
}

void Bsp::replace_lumps(LumpState& state) {
	for (unsigned int i = 0; i < HEADER_LUMPS; i++) {
		if (!state.lumps[i]) {
			continue;
		}

		delete[] lumps[i];
		lumps[i] = new unsigned char[state.lumpLen[i]];
		memcpy(lumps[i], state.lumps[i], state.lumpLen[i]);
		bsp_header.lump[i].nLength = state.lumpLen[i];

		if (i == LUMP_ENTITIES) {
			load_ents();
		}
	}

	update_lump_pointers();
}

unsigned int Bsp::remove_unused_structs(int lumpIdx, bool* usedStructs, int* remappedIndexes) {
	int structSize = 0;

	switch (lumpIdx) {
	case LUMP_PLANES: structSize = sizeof(BSPPLANE); break;
	case LUMP_VERTICES: structSize = sizeof(vec3); break;
	case LUMP_NODES: structSize = sizeof(BSPNODE); break;
	case LUMP_TEXINFO: structSize = sizeof(BSPTEXTUREINFO); break;
	case LUMP_FACES: structSize = sizeof(BSPFACE); break;
	case LUMP_CLIPNODES: structSize = sizeof(BSPCLIPNODE); break;
	case LUMP_LEAVES: structSize = sizeof(BSPLEAF); break;
	case LUMP_MARKSURFACES: structSize = sizeof(unsigned short); break;
	case LUMP_EDGES: structSize = sizeof(BSPEDGE); break;
	case LUMP_SURFEDGES: structSize = sizeof(int); break;
	default:
		logf("\nERROR: Invalid lump %d passed to remove_unused_structs\n", lumpIdx);
		return 0;
	}

	int oldStructCount = bsp_header.lump[lumpIdx].nLength / structSize;

	int removeCount = 0;
	for (int i = 0; i < oldStructCount; i++) {
		removeCount += !usedStructs[i];
	}

	int newStructCount = oldStructCount - removeCount;

	unsigned char* oldStructs = lumps[lumpIdx];
	unsigned char* newStructs = new unsigned char[newStructCount * structSize];

	for (int i = 0, k = 0; i < oldStructCount; i++) {
		if (!usedStructs[i]) {
			remappedIndexes[i] = 0; // prevent out-of-bounds remaps later
			continue;
		}
		memcpy(newStructs + k * structSize, oldStructs + i * structSize, structSize);
		remappedIndexes[i] = k++;
	}

	replace_lump(lumpIdx, newStructs, newStructCount * structSize);

	return removeCount;
}

unsigned int Bsp::remove_unused_textures(bool* usedTextures, int* remappedIndexes) {
	int oldTexCount = textureCount;

	int removeCount = 0;
	int removeSize = 0;
	for (int i = 0; i < oldTexCount; i++) {
		if (!usedTextures[i]) {
			int offset = ((int*)textures)[i + 1];
			BSPMIPTEX* tex = (BSPMIPTEX*)(textures + offset);

			// don't delete single frames from animated textures or else game crashes
			if (tex->szName[0] == '-' || tex->szName[0] == '+') {
				usedTextures[i] = true;
				// TODO: delete all frames if none are used
				continue;
			}

			if (offset == -1) {
				removeSize += sizeof(int);
			}
			else {
				removeSize += getBspTextureSize(tex) + sizeof(int);
			}
			removeCount++;
		}
	}

	int newTexCount = oldTexCount - removeCount;
	unsigned char* newTexData = new unsigned char[bsp_header.lump[LUMP_TEXTURES].nLength - removeSize];

	unsigned int* texHeader = (unsigned int*)newTexData;
	texHeader[0] = newTexCount;

	int newOffset = (newTexCount + 1) * sizeof(int);
	for (int i = 0, k = 0; i < oldTexCount; i++) {
		if (!usedTextures[i]) {
			continue;
		}
		int oldOffset = ((int*)textures)[i + 1];

		if (oldOffset == -1) {
			texHeader[k + 1] = -1;
		}
		else {
			BSPMIPTEX* tex = (BSPMIPTEX*)(textures + oldOffset);
			int sz = getBspTextureSize(tex);

			memcpy(newTexData + newOffset, textures + oldOffset, sz);
			texHeader[k + 1] = newOffset;
			newOffset += sz;
		}

		remappedIndexes[i] = k;
		k++;
	}

	replace_lump(LUMP_TEXTURES, newTexData, bsp_header.lump[LUMP_TEXTURES].nLength - removeSize);

	return removeCount;
}

unsigned int Bsp::remove_unused_lightmaps(bool* usedFaces) {
	int oldLightdataSize = lightDataLength;

	int* lightmapSizes = new int[faceCount];

	int newLightDataSize = 0;
	for (unsigned int i = 0; i < faceCount; i++) {
		if (usedFaces[i]) {
			lightmapSizes[i] = GetFaceLightmapSizeBytes(this, i);
			newLightDataSize += lightmapSizes[i];
		}
	}

	unsigned char* newColorData = new unsigned char[newLightDataSize];

	int offset = 0;
	for (unsigned int i = 0; i < faceCount; i++) {
		BSPFACE& face = faces[i];

		if (usedFaces[i] && (face.nLightmapOffset + lightmapSizes[i]) <= (unsigned int)lightDataLength) {
			memcpy(newColorData + offset, lightdata + face.nLightmapOffset, lightmapSizes[i]);
			face.nLightmapOffset = offset;
			offset += lightmapSizes[i];
		}
	}

	delete[] lightmapSizes;

	replace_lump(LUMP_LIGHTING, newColorData, newLightDataSize);

	return (unsigned int)(oldLightdataSize - newLightDataSize);
}

unsigned int Bsp::remove_unused_visdata(bool* usedLeaves, BSPLEAF* oldLeaves, int oldLeafCount) {
	int oldVisLength = visDataLength;

	// exclude solid leaf
	int oldVisLeafCount = oldLeafCount - 1;
	int newVisLeafCount = (bsp_header.lump[LUMP_LEAVES].nLength / sizeof(BSPLEAF)) - 1;

	int oldWorldLeaves = ((BSPMODEL*)lumps[LUMP_MODELS])->nVisLeafs; // TODO: allow deleting world leaves
	int newWorldLeaves = ((BSPMODEL*)lumps[LUMP_MODELS])->nVisLeafs;

	unsigned int oldVisRowSize = ((oldVisLeafCount + 63) & ~63) >> 3;
	unsigned int newVisRowSize = ((newVisLeafCount + 63) & ~63) >> 3;

	int decompressedVisSize = oldLeafCount * oldVisRowSize;
	unsigned char* decompressedVis = new unsigned char[decompressedVisSize];
	memset(decompressedVis, 0, decompressedVisSize);
	decompress_vis_lump(oldLeaves, lumps[LUMP_VISIBILITY], decompressedVis,
		oldWorldLeaves, oldVisLeafCount, oldVisLeafCount);

	if (oldVisRowSize != newVisRowSize) {
		int newDecompressedVisSize = oldLeafCount * newVisRowSize;
		unsigned char* newDecompressedVis = new unsigned char[decompressedVisSize];
		memset(newDecompressedVis, 0, newDecompressedVisSize);

		int minRowSize = std::min(oldVisRowSize, newVisRowSize);
		for (int i = 0; i < oldWorldLeaves; i++) {
			memcpy(newDecompressedVis + i * newVisRowSize, decompressedVis + i * oldVisRowSize, minRowSize);
		}

		delete[] decompressedVis;
		decompressedVis = newDecompressedVis;
	}

	unsigned char* compressedVis = new unsigned char[decompressedVisSize];
	memset(compressedVis, 0, decompressedVisSize);
	int64_t newVisLen = CompressAll(leaves, decompressedVis, compressedVis, newVisLeafCount, newWorldLeaves, decompressedVisSize);

	unsigned char* compressedVisResized = new unsigned char[newVisLen];
	memcpy(compressedVisResized, compressedVis, newVisLen);

	replace_lump(LUMP_VISIBILITY, compressedVisResized, newVisLen);

	delete[] decompressedVis;
	delete[] compressedVis;

	return (unsigned int)(oldVisLength - newVisLen);
}

STRUCTCOUNT Bsp::remove_unused_model_structures(bool export_bsp_with_clipnodes) {
	// marks which structures should not be moved
	STRUCTUSAGE usedStructures(this);

	bool* usedModels = new bool[modelCount];
	memset(usedModels, 0, sizeof(bool) * modelCount);
	usedModels[0] = true; // never delete worldspawn
	for (unsigned int i = 0; i < ents.size(); i++) {
		int modelIdx = ents[i]->getBspModelIdx();
		if (modelIdx >= 0 && modelIdx < (int)modelCount) {
			usedModels[modelIdx] = true;
		}
	}

	// reversed so models can be deleted without shifting the next delete index
	if (modelCount > 0)
	{
		for (int i = (int)modelCount - 1; i >= 0; i--) {
			if (!usedModels[i]) {
				delete_model(i);
			}
			else {
				mark_model_structures(i, &usedStructures, false);
			}
		}
	}
	delete[] usedModels;

	STRUCTREMAP remap(this);
	STRUCTCOUNT removeCount;
	memset(&removeCount, 0, sizeof(STRUCTCOUNT));

	usedStructures.edges[0] = true; // first edge is never used but maps break without it?

	unsigned char* oldLeaves = new unsigned char[bsp_header.lump[LUMP_LEAVES].nLength];
	memcpy(oldLeaves, lumps[LUMP_LEAVES], bsp_header.lump[LUMP_LEAVES].nLength);

	if (lightDataLength)
		removeCount.lightdata = remove_unused_lightmaps(usedStructures.faces);

	removeCount.planes = remove_unused_structs(LUMP_PLANES, usedStructures.planes, remap.planes);
	removeCount.nodes = remove_unused_structs(LUMP_NODES, usedStructures.nodes, remap.nodes);
	if (!export_bsp_with_clipnodes)
	{
		removeCount.clipnodes = remove_unused_structs(LUMP_CLIPNODES, usedStructures.clipnodes, remap.clipnodes);
	}
	removeCount.leaves = remove_unused_structs(LUMP_LEAVES, usedStructures.leaves, remap.leaves);
	removeCount.markSurfs = remove_unused_structs(LUMP_MARKSURFACES, usedStructures.markSurfs, remap.markSurfs);
	removeCount.faces = remove_unused_structs(LUMP_FACES, usedStructures.faces, remap.faces);
	removeCount.surfEdges = remove_unused_structs(LUMP_SURFEDGES, usedStructures.surfEdges, remap.surfEdges);
	removeCount.texInfos = remove_unused_structs(LUMP_TEXINFO, usedStructures.texInfo, remap.texInfo);
	removeCount.edges = remove_unused_structs(LUMP_EDGES, usedStructures.edges, remap.edges);
	removeCount.verts = remove_unused_structs(LUMP_VERTICES, usedStructures.verts, remap.verts);
	removeCount.textures = remove_unused_textures(usedStructures.textures, remap.textures);

	if (visDataLength)
		removeCount.visdata = remove_unused_visdata(usedStructures.leaves, (BSPLEAF*)oldLeaves, usedStructures.count.leaves);

	STRUCTCOUNT newCounts(this);

	for (unsigned int i = 0; i < newCounts.markSurfs; i++) {
		marksurfs[i] = remap.faces[marksurfs[i]];
	}
	for (unsigned int i = 0; i < newCounts.surfEdges; i++) {
		surfedges[i] = surfedges[i] >= 0 ? remap.edges[surfedges[i]] : -remap.edges[-surfedges[i]];
	}
	for (unsigned int i = 0; i < newCounts.edges; i++) {
		for (int k = 0; k < 2; k++) {
			edges[i].iVertex[k] = remap.verts[edges[i].iVertex[k]];
		}
	}
	for (unsigned int i = 0; i < newCounts.texInfos; i++) {
		texinfos[i].iMiptex = remap.textures[texinfos[i].iMiptex];
	}
	for (unsigned int i = 0; i < newCounts.clipnodes; i++) {
		clipnodes[i].iPlane = remap.planes[clipnodes[i].iPlane];
		for (int k = 0; k < 2; k++) {
			if (clipnodes[i].iChildren[k] >= 0) {
				clipnodes[i].iChildren[k] = remap.clipnodes[clipnodes[i].iChildren[k]];
			}
		}
	}
	for (unsigned int i = 0; i < newCounts.nodes; i++) {
		nodes[i].iPlane = remap.planes[nodes[i].iPlane];
		if (nodes[i].nFaces > 0)
			nodes[i].firstFace = remap.faces[nodes[i].firstFace];
		for (int k = 0; k < 2; k++) {
			if (nodes[i].iChildren[k] >= 0) {
				nodes[i].iChildren[k] = remap.nodes[nodes[i].iChildren[k]];
			}
			else {
				short leafIdx = ~nodes[i].iChildren[k];
				nodes[i].iChildren[k] = ~((short)remap.leaves[leafIdx]);
			}
		}
	}
	for (unsigned int i = 1; i < newCounts.leaves; i++) {
		if (leaves[i].nMarkSurfaces > 0)
			leaves[i].iFirstMarkSurface = remap.markSurfs[leaves[i].iFirstMarkSurface];
	}
	for (unsigned int i = 0; i < newCounts.faces; i++) {
		faces[i].iPlane = remap.planes[faces[i].iPlane];
		if (faces[i].nEdges > 0)
			faces[i].iFirstEdge = remap.surfEdges[faces[i].iFirstEdge];
		faces[i].iTextureInfo = remap.texInfo[faces[i].iTextureInfo];
	}

	for (unsigned int i = 0; i < modelCount; i++) {
		if (models[i].nFaces > 0)
			models[i].iFirstFace = remap.faces[models[i].iFirstFace];
		if (models[i].iHeadnodes[0] >= 0)
			models[i].iHeadnodes[0] = remap.nodes[models[i].iHeadnodes[0]];
		for (int k = 1; k < MAX_MAP_HULLS; k++) {
			if (models[i].iHeadnodes[k] >= 0)
				models[i].iHeadnodes[k] = remap.clipnodes[models[i].iHeadnodes[k]];
		}
	}

	return removeCount;
}

bool Bsp::has_hull2_ents() {
	// monsters that use hull 2 by default
	static std::set<std::string> largeMonsters{
		"monster_alien_grunt",
		"monster_alien_tor",
		"monster_alien_voltigore",
		"monster_babygarg",
		"monster_bigmomma",
		"monster_bullchicken",
		"monster_gargantua",
		"monster_ichthyosaur",
		"monster_kingpin",
		"monster_apache",
		"monster_blkop_apache"
		// osprey, nihilanth, and tentacle are huge but are basically nonsolid (no brush collision or triggers)
	};

	for (int i = 0; i < ents.size(); i++) {
		std::string cname = ents[i]->keyvalues["classname"];
		//std::string tname = ents[i]->keyvalues["targetname"];

		if (cname.find("monster_") == 0) {
			vec3 minhull;
			vec3 maxhull;

			if (!ents[i]->keyvalues["minhullsize"].empty())
				minhull = parseVector(ents[i]->keyvalues["minhullsize"]);
			if (!ents[i]->keyvalues["maxhullsize"].empty())
				maxhull = parseVector(ents[i]->keyvalues["maxhullsize"]);

			if (minhull == vec3(0, 0, 0) && maxhull == vec3(0, 0, 0)) {
				// monster is using its default hull size
				if (largeMonsters.find(cname) != largeMonsters.end()) {
					return true;
				}
			}
			else if (abs(minhull.x) > MAX_HULL1_EXTENT_MONSTER || abs(maxhull.x) > MAX_HULL1_EXTENT_MONSTER
				|| abs(minhull.y) > MAX_HULL1_EXTENT_MONSTER || abs(maxhull.y) > MAX_HULL1_EXTENT_MONSTER) {
				return true;
			}
		}
		else if (cname == "func_pushable") {
			int modelIdx = ents[i]->getBspModelIdx();
			if (modelIdx >= 0 && modelIdx < (int)modelCount) {
				BSPMODEL& model = models[modelIdx];
				vec3 size = model.nMaxs - model.nMins;

				if (size.x > MAX_HULL1_SIZE_PUSHABLE || size.y > MAX_HULL1_SIZE_PUSHABLE) {
					return true;
				}
			}
		}
	}

	return false;
}

STRUCTCOUNT Bsp::delete_unused_hulls(bool noProgress) {
	if (!noProgress) {
		if (g_verbose)
			g_progress.update("", 0);
		else
			g_progress.update("Deleting unused hulls", modelCount - 1);
	}

	int deletedHulls = 0;

	for (unsigned int i = 1; i < modelCount; i++) {
		if (!g_verbose && !noProgress)
			g_progress.tick();

		std::vector<Entity*> usageEnts = get_model_ents(i);

		if (usageEnts.empty()) {
			debugf("Deleting unused model %d\n", i);

			for (int k = 0; k < MAX_MAP_HULLS; k++)
				deletedHulls += models[i].iHeadnodes[k] >= 0;

			delete_model(i);
			//modelCount--; automatically updated when lump is replaced
			i--;
			continue;
		}

		std::set<std::string> conditionalPointEntTriggers;
		conditionalPointEntTriggers.insert("trigger_once");
		conditionalPointEntTriggers.insert("trigger_multiple");
		conditionalPointEntTriggers.insert("trigger_counter");
		conditionalPointEntTriggers.insert("trigger_gravity");
		conditionalPointEntTriggers.insert("trigger_teleport");

		std::set<std::string> entsThatNeverNeedAnyHulls;
		entsThatNeverNeedAnyHulls.insert("env_bubbles");
		entsThatNeverNeedAnyHulls.insert("func_tankcontrols");
		entsThatNeverNeedAnyHulls.insert("func_traincontrols");
		entsThatNeverNeedAnyHulls.insert("func_vehiclecontrols");
		entsThatNeverNeedAnyHulls.insert("trigger_autosave"); // obsolete in sven
		entsThatNeverNeedAnyHulls.insert("trigger_endsection"); // obsolete in sven

		std::set<std::string> entsThatNeverNeedCollision;
		entsThatNeverNeedCollision.insert("func_illusionary");
		entsThatNeverNeedCollision.insert("func_mortar_field");

		std::set<std::string> passableEnts;
		passableEnts.insert("func_door");
		passableEnts.insert("func_door_rotating");
		passableEnts.insert("func_pendulum");
		passableEnts.insert("func_tracktrain");
		passableEnts.insert("func_train");
		passableEnts.insert("func_water");
		passableEnts.insert("momentary_door");

		std::set<std::string> playerOnlyTriggers;
		playerOnlyTriggers.insert("func_ladder");
		playerOnlyTriggers.insert("game_zone_player");
		playerOnlyTriggers.insert("player_respawn_zone");
		playerOnlyTriggers.insert("trigger_cdaudio");
		playerOnlyTriggers.insert("trigger_changelevel");
		playerOnlyTriggers.insert("trigger_transition");

		std::set<std::string> monsterOnlyTriggers;
		monsterOnlyTriggers.insert("func_monsterclip");
		monsterOnlyTriggers.insert("trigger_monsterjump");

		std::string uses;
		bool needsPlayerHulls = false; // HULL 1 + HULL 3
		bool needsMonsterHulls = false; // All HULLs
		bool needsVisibleHull = false; // HULL 0
		for (int k = 0; k < usageEnts.size(); k++) {
			std::string cname = usageEnts[k]->keyvalues["classname"];
			std::string tname = usageEnts[k]->keyvalues["targetname"];
			int spawnflags = atoi(usageEnts[k]->keyvalues["spawnflags"].c_str());

			if (k != 0) {
				uses += ", ";
			}
			uses += "\"" + tname + "\" (" + cname + ")";

			if (entsThatNeverNeedAnyHulls.find(cname) != entsThatNeverNeedAnyHulls.end()) {
				continue; // no collision or faces needed at all
			}
			else if (entsThatNeverNeedCollision.find(cname) != entsThatNeverNeedCollision.end()) {
				needsVisibleHull = !is_invisible_solid(usageEnts[k]);
			}
			else if (passableEnts.find(cname) != passableEnts.end()) {
				needsPlayerHulls = needsMonsterHulls = !(spawnflags & 8); // "Passable" or "Not solid" unchecked
				needsVisibleHull = !(spawnflags & 8) || !is_invisible_solid(usageEnts[k]);
			}
			else if (cname.find("trigger_") == 0) {
				if (conditionalPointEntTriggers.find(cname) != conditionalPointEntTriggers.end()) {
					needsVisibleHull = spawnflags & 8; // "Everything else" flag checked
					needsPlayerHulls = !(spawnflags & 2); // "No clients" unchecked
					needsMonsterHulls = (spawnflags & 1) || (spawnflags & 4); // "monsters" or "pushables" checked
				}
				else if (cname == "trigger_push") {
					needsPlayerHulls = !(spawnflags & 8); // "No clients" unchecked
					needsMonsterHulls = (spawnflags & 4) || !(spawnflags & 16); // "Pushables" checked or "No monsters" unchecked
					needsVisibleHull = true; // needed for point-ent pushing
				}
				else if (cname == "trigger_hurt") {
					needsPlayerHulls = !(spawnflags & 8); // "No clients" unchecked
					needsMonsterHulls = !(spawnflags & 16) || !(spawnflags & 32); // "Fire/Touch client only" unchecked
				}
				else {
					needsPlayerHulls = true;
					needsMonsterHulls = true;
				}
			}
			else if (cname == "func_clip") {
				needsPlayerHulls = !(spawnflags & 8); // "No clients" not checked
				needsMonsterHulls = (spawnflags & 8) || !(spawnflags & 16); // "Pushables" checked or "No monsters" unchecked
				needsVisibleHull = (spawnflags & 32) || (spawnflags & 64); // "Everything else" or "item_inv" checked
			}
			else if (cname == "func_conveyor") {
				needsPlayerHulls = needsMonsterHulls = !(spawnflags & 2); // "Not Solid" unchecked
				needsVisibleHull = !(spawnflags & 2) || !is_invisible_solid(usageEnts[k]);
			}
			else if (cname == "func_friction") {
				needsPlayerHulls = true;
				needsMonsterHulls = true;
			}
			else if (cname == "func_rot_button") {
				needsPlayerHulls = needsMonsterHulls = !(spawnflags & 1); // "Not solid" unchecked
				needsVisibleHull = true;
			}
			else if (cname == "func_rotating") {
				needsPlayerHulls = needsMonsterHulls = !(spawnflags & 64); // "Not solid" unchecked
				needsVisibleHull = true;
			}
			else if (cname == "func_ladder") {
				needsPlayerHulls = true;
				needsVisibleHull = true;
			}
			else if (playerOnlyTriggers.find(cname) != playerOnlyTriggers.end()) {
				needsPlayerHulls = true;
			}
			else if (monsterOnlyTriggers.find(cname) != monsterOnlyTriggers.end()) {
				needsMonsterHulls = true;
			}
			else {
				// assume all hulls are needed
				needsPlayerHulls = true;
				needsMonsterHulls = true;
				needsVisibleHull = true;
				break;
			}
		}

		BSPMODEL& model = ((BSPMODEL*)lumps[LUMP_MODELS])[i];

		if (!needsVisibleHull && !needsMonsterHulls) {
			if (models[i].iHeadnodes[0] >= 0)
				debugf("Deleting HULL 0 from model %d, used in %s\n", i, uses.c_str());

			deletedHulls += models[i].iHeadnodes[0] >= 0;

			model.iHeadnodes[0] = -1;
			model.nVisLeafs = 0;
			model.nFaces = 0;
			model.iFirstFace = 0;
		}
		if (!needsPlayerHulls && !needsMonsterHulls) {
			bool deletedAnyHulls = false;
			for (int k = 1; k < MAX_MAP_HULLS; k++) {
				deletedHulls += models[i].iHeadnodes[k] >= 0;
				if (models[i].iHeadnodes[k] >= 0) {
					deletedHulls++;
					deletedAnyHulls = true;
				}
			}

			if (deletedAnyHulls)
				debugf("Deleting HULL 1-3 from model %d, used in %s\n", i, uses.c_str());

			model.iHeadnodes[1] = -1;
			model.iHeadnodes[2] = -1;
			model.iHeadnodes[3] = -1;
		}
		else if (!needsMonsterHulls) {
			if (models[i].iHeadnodes[2] >= 0)
				debugf("Deleting HULL 2 from model %d, used in %s\n", i, uses.c_str());

			deletedHulls += models[i].iHeadnodes[2] >= 0;

			model.iHeadnodes[2] = -1;
		}
		else if (!needsPlayerHulls) {
			// monsters use all hulls so can't do anything about this
		}
	}

	STRUCTCOUNT removed = remove_unused_model_structures();

	update_ent_lump();

	if (!g_verbose && !noProgress) {
		g_progress.clear();
	}

	return removed;
}

bool Bsp::is_invisible_solid(Entity* ent) {
	if (!ent->isBspModel())
		return false;

	std::string tname = ent->keyvalues["targetname"];
	int rendermode = atoi(ent->keyvalues["rendermode"].c_str());
	int renderamt = atoi(ent->keyvalues["renderamt"].c_str());
	int renderfx = atoi(ent->keyvalues["renderfx"].c_str());

	if (rendermode == 0 || renderamt != 0) {
		return false;
	}
	switch (renderfx) {
	case 1: case 2: case 3: case 4: case 7:
	case 8: case 15: case 16: case 17:
		return false;
	default:
		break;
	}

	static std::set<std::string> renderKeys{
		"rendermode",
		"renderamt",
		"renderfx"
	};

	for (int i = 0; i < ents.size(); i++) {
		std::string cname = ents[i]->keyvalues["classname"];

		if (cname == "env_render") {
			return false; // assume it will affect the brush since it can be moved anywhere
		}
		else if (cname == "env_render_individual") {
			if (ents[i]->keyvalues["target"] == tname) {
				return false; // assume it's making the ent visible
			}
		}
		else if (cname == "trigger_changevalue") {
			if (ents[i]->keyvalues["target"] == tname) {
				if (renderKeys.find(ents[i]->keyvalues["m_iszValueName"]) != renderKeys.end()) {
					return false; // assume it's making the ent visible
				}
			}
		}
		else if (cname == "trigger_copyvalue") {
			if (ents[i]->keyvalues["target"] == tname) {
				if (renderKeys.find(ents[i]->keyvalues["m_iszDstValueName"]) != renderKeys.end()) {
					return false; // assume it's making the ent visible
				}
			}
		}
		else if (cname == "trigger_createentity") {
			if (ents[i]->keyvalues["+model"] == tname || ents[i]->keyvalues["-model"] == ent->keyvalues["model"]) {
				return false; // assume this new ent will be visible at some point
			}
		}
		else if (cname == "trigger_changemodel") {
			if (ents[i]->keyvalues["model"] == ent->keyvalues["model"]) {
				return false; // assume the target is visible
			}
		}
	}

	return true;
}

void Bsp::get_lightmap_shift(const LIGHTMAP& oldLightmap, const LIGHTMAP& newLightmap, int& srcOffsetX, int& srcOffsetY) {
	int minWidth = std::min(newLightmap.width, oldLightmap.width);
	int minHeight = std::min(newLightmap.height, oldLightmap.height);

	int bestMatch = 0;
	int bestShiftCombo = 0;

	// Try different combinations of shifts to find the best alignment of the lightmaps.
	// Example (2 = unlit, 3 = lit)
	//  old         new
	// 3 3 3      2 3 3 3
	// 3 3 3  ->  2 3 3 3  =  old lightmap matches more luxels when it's shifted right 1 pixel in the new lightmap
	// 3 3 3      2 3 3 3
	// Only works for lightmap resizes caused by precision errors. Faces that are actually different sizes will
	// likely have more than 1 pixel of difference in either dimension.
	for (int t = 0; t < 4; t++) {
		int numMatch = 0;
		for (int y = 0; y < minHeight; y++) {
			for (int x = 0; x < minWidth; x++) {
				int offsetX = x;
				int offsetY = y;

				if (t == 1) {
					offsetX = x + 1;
				}
				if (t == 2) {
					offsetY = y + 1;
				}
				if (t == 3) {
					offsetX = x + 1;
					offsetY = y + 1;
				}

				int srcX = oldLightmap.width > newLightmap.width ? offsetX : x;
				int srcY = oldLightmap.height > newLightmap.height ? offsetY : y;
				int dstX = newLightmap.width > oldLightmap.width ? offsetX : x;
				int dstY = newLightmap.height > oldLightmap.height ? offsetY : y;

				srcX = std::max(0, std::min(oldLightmap.width - 1, srcX));
				srcY = std::max(0, std::min(oldLightmap.height - 1, srcY));
				dstX = std::max(0, std::min(newLightmap.width - 1, dstX));
				dstY = std::max(0, std::min(newLightmap.height - 1, dstY));

				int oldLuxelFlag = oldLightmap.luxelFlags[srcY * oldLightmap.width + srcX];
				int newLuxelFlag = newLightmap.luxelFlags[dstY * newLightmap.width + dstX];

				if (oldLuxelFlag == newLuxelFlag) {
					numMatch += 1;
				}
			}
		}

		if (numMatch > bestMatch) {
			bestMatch = numMatch;
			bestShiftCombo = t;
		}
	}

	int shouldShiftLeft = bestShiftCombo == 1 || bestShiftCombo == 3;
	int shouldShiftTop = bestShiftCombo == 2 || bestShiftCombo == 3;

	srcOffsetX = newLightmap.width != oldLightmap.width ? shouldShiftLeft : 0;
	srcOffsetY = newLightmap.height != oldLightmap.height ? shouldShiftTop : 0;
}

void Bsp::update_ent_lump(bool stripNodes) {
	std::stringstream ent_data;

	for (int i = 0; i < ents.size(); i++) {
		if (stripNodes) {
			std::string cname = ents[i]->keyvalues["classname"];
			if (cname == "info_node" || cname == "info_node_air") {
				continue;
			}
		}

		ent_data << "{\n";

		for (int k = 0; k < ents[i]->keyOrder.size(); k++) {
			std::string key = ents[i]->keyOrder[k];
			ent_data << "\"" << key << "\" \"" << ents[i]->keyvalues[key] << "\"\n";
		}

		ent_data << "}";
		if (i < ents.size() - 1) {
			ent_data << "\n"; // trailing newline crashes sven, and only sven, and only sometimes
		}
	}

	std::string str_data = ent_data.str();

	unsigned char* newEntData = new unsigned char[str_data.size() + 1];
	memcpy(newEntData, str_data.c_str(), str_data.size());
	newEntData[str_data.size()] = 0; // null terminator required too(?)

	replace_lump(LUMP_ENTITIES, newEntData, str_data.size() + 1);
}

vec3 Bsp::get_model_center(int modelIdx) {
	if (modelIdx < 0 || modelIdx > bsp_header.lump[LUMP_MODELS].nLength / sizeof(BSPMODEL)) {
		logf("Invalid model index %d. Must be 0 - %d\n", modelIdx);
		return vec3();
	}

	BSPMODEL& model = models[modelIdx];

	return model.nMins + (model.nMaxs - model.nMins) * 0.5f;
}

int Bsp::lightmap_count(int faceIdx) {
	BSPFACE& face = faces[faceIdx];

	if (texinfos[face.iTextureInfo].nFlags & TEX_SPECIAL || face.nLightmapOffset >= lightDataLength)
		return 0;

	int lightmapCount = 0;
	for (int k = 0; k < 4; k++) {
		lightmapCount += face.nStyles[k] != 255;
	}

	return lightmapCount;
}

void Bsp::write(std::string path) {

	if (path.rfind(".bsp") != path.size() - 4) {
		path = path + ".bsp";
	}

	// calculate lump offsets
	int offset = sizeof(BSPHEADER);
	for (int i = 0; i < HEADER_LUMPS; i++) {
		bsp_header.lump[i].nOffset = offset;
		offset += bsp_header.lump[i].nLength;
	}

	// Make single backup
	if (g_settings.backUpMap && fileExists(path) && !fileExists(path + ".bak"))
	{
		int len;
		char* oldfile = loadFile(path, len);
		std::ofstream file(path + ".bak", std::ios::trunc | std::ios::binary);
		if (!file.is_open()) {
			logf("Failed to open backup file for writing:\n%s\n", path.c_str());
			return;
		}
		logf("Writing backup to %s\n", (path + ".bak").c_str());

		file.write(oldfile, len);
		delete[] oldfile;
	}

	std::ofstream file(path, std::ios::trunc | std::ios::binary);
	if (!file.is_open()) {
		logf("Failed to open BSP file for writing:\n%s\n", path.c_str());
		return;
	}


	if (g_settings.preserveCrc32)
	{
		if (ents.size() && ents[0]->hasKey("CRC"))
		{
			originCrc32 = reverse_bits(std::stoul(ents[0]->keyvalues["CRC"]));
			logf("HACKING CRC value. Loading original CRC key from WORLDSPAWN: %u. ",
				reverse_bits(originCrc32));
		}
		else
			logf("HACKING CRC value. Original crc: %u. ", reverse_bits(originCrc32));

		unsigned int crc32 = UINT32_C(0xFFFFFFFF);


		for (int i = 0; i < HEADER_LUMPS; i++)
		{
			if (i != LUMP_ENTITIES)
				crc32 = GetCrc32InMemory(&lumps[i][0], bsp_header.lump[i].nLength, crc32);
		}


		logf("Current value: %u. ", reverse_bits(crc32));

		if (originCrc32 == crc32)
		{
			logf("Same values. Skip hacking.\n");
		}
		else
		{
			int originsize = bsp_header.lump[LUMP_MODELS].nLength;

			unsigned char* tmpNewModelds = new unsigned char[originsize + sizeof(BSPMODEL)];
			memset(tmpNewModelds, 0, originsize + sizeof(BSPMODEL));
			memcpy(tmpNewModelds, &lumps[LUMP_MODELS][0], bsp_header.lump[LUMP_MODELS].nLength);
			BSPMODEL* lastmodel = (BSPMODEL*)&tmpNewModelds[originsize];
			lastmodel->vOrigin.z = 9999.0f;
			lumps[LUMP_MODELS] = tmpNewModelds;
			bsp_header.lump[LUMP_MODELS].nLength += sizeof(BSPMODEL);

			update_lump_pointers();

			crc32 = UINT32_C(0xFFFFFFFF);


			for (int i = 0; i < HEADER_LUMPS; i++)
			{
				if (i != LUMP_ENTITIES)
					crc32 = GetCrc32InMemory(&lumps[i][0], bsp_header.lump[i].nLength, crc32);
			}

			PathCrc32InMemory(&lumps[LUMP_MODELS][0], bsp_header.lump[LUMP_MODELS].nLength, bsp_header.lump[LUMP_MODELS].nLength - sizeof(BSPMODEL), crc32, originCrc32);

			crc32 = UINT32_C(0xFFFFFFFF);
			for (int i = 0; i < HEADER_LUMPS; i++)
			{
				if (i != LUMP_ENTITIES)
					crc32 = GetCrc32InMemory(&lumps[i][0], bsp_header.lump[i].nLength, crc32);
			}

			logf("Hacked value: %u. \n", reverse_bits(crc32));
		}
	}

	logf("Writing %s\n", bsp_path.c_str());
	file.write((char*)&bsp_header, sizeof(BSPHEADER));

	// write the lumps
	for (int i = 0; i < HEADER_LUMPS; i++) {

		file.write((char*)lumps[i], bsp_header.lump[i].nLength);
	}
}

bool Bsp::load_lumps(std::string fpath)
{
	bool valid = true;

	// Read all BSP Data
	std::ifstream fin(fpath, std::ios::binary | std::ios::ate);
	auto size = fin.tellg();
	fin.seekg(0, std::ios::beg);

	if (size < sizeof(BSPHEADER) + sizeof(BSPLUMP) * HEADER_LUMPS)
		return false;

	fin.read((char*)&bsp_header.nVersion, sizeof(int));
#ifndef NDEBUG
	logf("Bsp version: %d\n", bsp_header.nVersion);
#endif

	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		fin.read((char*)&bsp_header.lump[i], sizeof(BSPLUMP));
#ifndef NDEBUG
		logf("Read lump id: %d. Len: %d. Offset %d.\n", i, bsp_header.lump[i].nLength, bsp_header.lump[i].nOffset);
#endif
	}

	lumps = new unsigned char* [HEADER_LUMPS];
	memset(lumps, 0, sizeof(unsigned char*) * HEADER_LUMPS);

	unsigned int crc32 = UINT32_C(0xFFFFFFFF);

	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		if (bsp_header.lump[i].nLength == 0) {
			lumps[i] = NULL;
			continue;
		}

		fin.seekg(bsp_header.lump[i].nOffset);
		if (fin.eof()) {
			logf("FAILED TO READ BSP LUMP %d\n", i);
			valid = false;
		}
		else
		{
			lumps[i] = new unsigned char[bsp_header.lump[i].nLength];
			fin.read((char*)lumps[i], bsp_header.lump[i].nLength);
			if (i != LUMP_ENTITIES)
				crc32 = GetCrc32InMemory(&lumps[i][0], bsp_header.lump[i].nLength, crc32);
		}
	}

	originCrc32 = crc32;

	fin.close();

	return valid;
}

void Bsp::load_ents()
{
	for (int i = 0; i < ents.size(); i++)
		delete ents[i];
	ents.clear();

	bool verbose = true;
	membuf sbuf((char*)lumps[LUMP_ENTITIES], bsp_header.lump[LUMP_ENTITIES].nLength);
	std::istream in(&sbuf);

	int lineNum = 0;
	int lastBracket = -1;
	Entity* ent = NULL;

	std::string line;
	while (std::getline(in, line))
	{
		lineNum++;

		while (line[0] == ' ' || line[0] == '\t' || line[0] == '\r')
		{
			line.erase(line.begin());
		}

		if (line.length() < 1 || line[0] == '\n')
			continue;

		if (line[0] == '{')
		{
			if (lastBracket == 0)
			{
				logf("%s.bsp ent data (line %d): Unexpected '{'\n", bsp_path.c_str(), lineNum);
				continue;
			}
			lastBracket = 0;
			if (ent)
				delete ent;
			ent = new Entity();

			if (line.find('}') == std::string::npos &&
				line.find('\"') == std::string::npos)
			{
				continue;
			}
		}
		if (line[0] == '}')
		{
			if (lastBracket == 1)
				logf("%s.bsp ent data (line %d): Unexpected '}'\n", bsp_path.c_str(), lineNum);
			lastBracket = 1;
			if (!ent)
				continue;

			if (ent->keyvalues.count("classname"))
				ents.push_back(ent);
			else
				logf("Found unknown classname entity. Skip it.\n");

			ent = NULL;

			// you can end/start an ent on the same line, you know
			if (line.find('{') != std::string::npos)
			{
				ent = new Entity();
				lastBracket = 0;

				if (line.find('\"') == std::string::npos)
				{
					continue;
				}
				line.erase(line.begin());
			}
		}
		if (lastBracket == 0 && ent) // currently defining an entity
		{
			Keyvalues k(line);
			for (int i = 0; i < k.keys.size(); i++)
			{
				ent->addKeyvalue(k.keys[i], k.values[i], true);
			}

			if (line.find('}') != std::string::npos)
			{
				lastBracket = 1;

				if (ent->keyvalues.count("classname"))
					ents.push_back(ent);
				else
					logf("Found unknown classname entity. Skip it.\n");
				ent = NULL;
			}
			if (line.find('{') != std::string::npos)
			{
				ent = new Entity();
				lastBracket = 0;
			}
		}
	}

	if (ents.size() > 1)
	{
		if (ents[0]->keyvalues["classname"] != "worldspawn")
		{
			logf("First entity has classname different from 'woldspawn', we do fixup it\n");
			for (int i = 1; i < ents.size(); i++)
			{
				if (ents[i]->keyvalues["classname"] == "worldspawn")
				{
					std::swap(ents[0], ents[i]);
					break;
				}
			}
		}
	}

	if (ent)
		delete ent;
}

void Bsp::print_stat(const std::string& name, unsigned int val, unsigned int max, bool isMem) {
	const float meg = 1024 * 1024;
	float percent = (val / (float)max) * 100;

	if (val > max) {
		print_color(PRINT_RED | PRINT_BRIGHT);
	}
	else if (percent >= 90) {
		print_color(PRINT_RED | PRINT_GREEN | PRINT_BRIGHT);
	}
	else if (percent >= 75) {
		print_color(PRINT_RED | PRINT_GREEN | PRINT_BLUE | PRINT_BRIGHT);
	}
	else {
		print_color(PRINT_RED | PRINT_GREEN | PRINT_BLUE);
	}

	logf("%-12s  ", name.c_str());
	if (isMem) {
		logf("%8.2f / %-5.2f MB", val / meg, max / meg);
	}
	else {
		logf("%8u / %-8u", val, max);
	}
	logf("  %6.1f%%", percent);

	if (val > max) {
		logf("  (OVERFLOW!!!)");
	}

	logf("\n");

	print_color(PRINT_RED | PRINT_GREEN | PRINT_BLUE);
}

void Bsp::print_model_stat(STRUCTUSAGE* modelInfo, unsigned int val, unsigned int max, bool isMem)
{
	std::string classname = modelInfo->modelIdx == 0 ? "worldspawn" : "???";
	std::string targetname = modelInfo->modelIdx == 0 ? "" : "???";
	for (int k = 0; k < ents.size(); k++) {
		if (ents[k]->getBspModelIdx() == modelInfo->modelIdx) {
			targetname = ents[k]->keyvalues["targetname"];
			classname = ents[k]->keyvalues["classname"];
		}
	}

	const float meg = 1024 * 1024;
	float percent = (val / (float)max) * 100;

	if (isMem) {
		logf("%8.1f / %-5.1f MB", val / meg, max / meg);
	}
	else {
		logf("%-26s %-26s *%-6d %9d", classname.c_str(), targetname.c_str(), modelInfo->modelIdx, val);
	}
	if (percent >= 0.1f)
		logf("  %6.1f%%", percent);

	logf("\n");
}

bool sortModelInfos(const STRUCTUSAGE* a, const STRUCTUSAGE* b) {
	switch (g_sort_mode) {
	case SORT_VERTS:
		return a->sum.verts > b->sum.verts;
	case SORT_NODES:
		return a->sum.nodes > b->sum.nodes;
	case SORT_CLIPNODES:
		return a->sum.clipnodes > b->sum.clipnodes;
	case SORT_FACES:
		return a->sum.faces > b->sum.faces;
	}
	return false;
}

bool Bsp::isValid() {
	return modelCount < MAX_MAP_MODELS
		&& planeCount < MAX_MAP_PLANES
		&& vertCount < MAX_MAP_VERTS
		&& nodeCount < MAX_MAP_NODES
		&& texinfoCount < MAX_MAP_TEXINFOS
		&& faceCount < MAX_MAP_FACES
		&& clipnodeCount < MAX_MAP_CLIPNODES
		&& leafCount < MAX_MAP_LEAVES
		&& marksurfCount < MAX_MAP_MARKSURFS
		&& surfedgeCount < MAX_MAP_SURFEDGES
		&& edgeCount < MAX_MAP_SURFEDGES
		&& textureCount < MAX_MAP_TEXTURES
		&& lightDataLength < MAX_MAP_LIGHTDATA
		&& visDataLength < MAX_MAP_VISDATA
		&& ents.size() < MAX_MAP_ENTS;
}

bool Bsp::validate() {
	bool isValid = true;

	for (unsigned int i = 0; i < marksurfCount; i++) {
		if (marksurfs[i] >= faceCount) {
			logf("Bad face reference in marksurf %d: %d / %d\n", i, marksurfs[i], faceCount);
			isValid = false;
		}
	}
	for (unsigned int i = 0; i < surfedgeCount; i++) {
		if ((unsigned int)abs(surfedges[i]) >= edgeCount) {
			logf("Bad edge reference in surfedge %d: %d / %d\n", i, surfedges[i], edgeCount);
			isValid = false;
		}
	}
	for (unsigned int i = 0; i < texinfoCount; i++) {
		if (texinfos[i].iMiptex >= textureCount) {
			logf("Bad texture reference in textureinfo %d: %d / %d\n", i, texinfos[i].iMiptex, textureCount);
			isValid = false;
		}
	}
	for (unsigned int i = 0; i < faceCount; i++) {
		if (faces[i].iPlane >= planeCount) {
			logf("Bad plane reference in face %d: %d / %d\n", i, faces[i].iPlane, planeCount);
			isValid = false;
		}
		if (faces[i].nEdges > 0 && faces[i].iFirstEdge >= surfedgeCount) {
			logf("Bad surfedge reference in face %d: %d / %d\n", i, faces[i].iFirstEdge, surfedgeCount);
			isValid = false;
		}
		if (faces[i].iTextureInfo >= texinfoCount) {
			logf("Bad textureinfo reference in face %d: %d / %d\n", i, faces[i].iTextureInfo, texinfoCount);
			isValid = false;
		}
		if (lightDataLength > 0 && faces[i].nStyles[0] != 255 &&
			faces[i].nLightmapOffset != (unsigned int)-1 && faces[i].nLightmapOffset >= lightDataLength)
		{
			logf("Bad lightmap offset in face %d: %d / %d\n", i, faces[i].nLightmapOffset, lightDataLength);
			isValid = false;
		}
	}
	for (unsigned int i = 0; i < leafCount; i++) {
		if (leaves[i].nMarkSurfaces > 0 && leaves[i].iFirstMarkSurface >= marksurfCount) {
			logf("Bad marksurf reference in leaf %d: %d / %d\n", i, leaves[i].iFirstMarkSurface, marksurfCount);
			isValid = false;
		}
		if (visDataLength > 0 &&
			leaves[i].nVisOffset != (unsigned int)-1 && (leaves[i].nVisOffset < 0 || (unsigned int)leaves[i].nVisOffset >= visDataLength)) {
			logf("Bad vis offset in leaf %d: %d / %d\n", i, leaves[i].nVisOffset, visDataLength);
			isValid = false;
		}
	}
	for (unsigned int i = 0; i < edgeCount; i++) {
		for (unsigned int k = 0; k < 2; k++) {
			if (edges[i].iVertex[k] >= vertCount) {
				logf("Bad vertex reference in edge %d: %d / %d\n", i, edges[i].iVertex[k], vertCount);
				isValid = false;
			}
		}
	}
	for (unsigned int i = 0; i < nodeCount; i++) {
		if (nodes[i].nFaces > 0 && nodes[i].firstFace >= faceCount) {
			logf("Bad face reference in node %d: %d / %d\n", i, nodes[i].firstFace, faceCount);
			isValid = false;
		}
		if (nodes[i].iPlane >= planeCount) {
			logf("Bad plane reference in node %d: %d / %d\n", i, nodes[i].iPlane, planeCount);
			isValid = false;
		}
		for (unsigned int k = 0; k < 2; k++) {
			if (nodes[i].iChildren[k] != (unsigned int)-1 && nodes[i].iChildren[k] > 0 && (unsigned int)nodes[i].iChildren[k] >= nodeCount) {
				logf("Bad node reference in node %d child %d: %d / %d\n", i, k, nodes[i].iChildren[k], nodeCount);
				isValid = false;
			}
			else if (~nodes[i].iChildren[k] != (unsigned int)-1 && nodes[i].iChildren[k] < 0 && (unsigned int)~nodes[i].iChildren[k] >= leafCount) {
				logf("Bad leaf reference in ~node %d child %d: %d / %d\n", i, k, ~nodes[i].iChildren[k], leafCount);
				isValid = false;
			}
		}
	}
	for (unsigned int i = 0; i < clipnodeCount; i++) {
		if (clipnodes[i].iPlane < 0 || (unsigned int)clipnodes[i].iPlane >= planeCount) {
			logf("Bad plane reference in clipnode %d: %d / %d\n", i, clipnodes[i].iPlane, planeCount);
			isValid = false;
		}
		for (unsigned int k = 0; k < 2; k++) {
			if (clipnodes[i].iChildren[k] > 0 && (unsigned int)clipnodes[i].iChildren[k] >= clipnodeCount) {
				logf("Bad clipnode reference in clipnode %d child %d: %d / %d\n", i, k, clipnodes[i].iChildren[k], clipnodeCount);
				isValid = false;
			}
		}
	}
	for (unsigned int i = 0; i < ents.size(); i++) {
		if (ents[i]->getBspModelIdxForce() > 0 && (unsigned int)ents[i]->getBspModelIdxForce() >= modelCount) {
			logf("Bad model reference in entity %d: %d / %d\n", i, ents[i]->getBspModelIdxForce(), modelCount);
			isValid = false;
		}
	}


	int totalVisLeaves = 1; // solid leaf not included in model leaf counts
	int totalFaces = 0;
	for (unsigned int i = 0; i < modelCount; i++) {
		totalVisLeaves += models[i].nVisLeafs;
		totalFaces += models[i].nFaces;
		if (models[i].nFaces > 0 && (models[i].iFirstFace < 0 || (unsigned int)models[i].iFirstFace >= faceCount)) {
			logf("Bad face reference in model %d: %d / %d\n", i, models[i].iFirstFace, faceCount);
			isValid = false;
		}
		if (models[i].iHeadnodes[0] >= (int)nodeCount) {
			logf("Bad node reference in model %d hull 0: %d / %d\n", i, models[i].iHeadnodes[0], nodeCount);
			isValid = false;
		}
		for (int k = 1; k < MAX_MAP_HULLS; k++) {
			if (models[i].iHeadnodes[k] >= (int)clipnodeCount) {
				logf("Bad clipnode reference in model %d hull %d: %d / %d\n", i, k, models[i].iHeadnodes[k], clipnodeCount);
				isValid = false;
			}
		}
		if (models[i].nMins.x > models[i].nMaxs.x ||
			models[i].nMins.y > models[i].nMaxs.y ||
			models[i].nMins.z > models[i].nMaxs.z) {
			std::swap(models[i].nMins, models[i].nMaxs);
			if (models[i].nMins.x > models[i].nMaxs.x ||
				models[i].nMins.y > models[i].nMaxs.y ||
				models[i].nMins.z > models[i].nMaxs.z) {
				logf("Backwards mins/maxs in model %d. Mins: (%f, %f, %f) Maxs: (%f %f %f)\n", i,
					models[i].nMins.x, models[i].nMins.y, models[i].nMins.z,
					models[i].nMaxs.x, models[i].nMaxs.y, models[i].nMaxs.z);
				isValid = false;
			}
			else
			{
				logf("SWAPPED mins/maxs in model %d. Mins: (%f, %f, %f) Maxs: (%f %f %f)\n", i,
					models[i].nMins.x, models[i].nMins.y, models[i].nMins.z,
					models[i].nMaxs.x, models[i].nMaxs.y, models[i].nMaxs.z);
			}
		}
	}
	if (totalVisLeaves != leafCount) {
		logf("Bad model vis leaf sum: %d / %d\n", totalVisLeaves, leafCount);
		isValid = false;
	}
	if (totalFaces != faceCount) {
		logf("Bad model face sum: %d / %d\n", totalFaces, faceCount);
		isValid = false;
	}

	unsigned int worldspawn_count = 0;
	for (unsigned int i = 0; i < ents.size(); i++) {
		if (ents[i]->keyvalues["classname"] == "worldspawn") {
			worldspawn_count++;
		}
	}
	if (worldspawn_count != 1) {
		logf("Found %d worldspawn entities (expected 1). This can cause crashes and svc_bad errors.\n", worldspawn_count);
		isValid = false;
	}

	std::set<int> used_models; // Protected map
	used_models.insert(0);

	for (auto const& s : ents)
	{
		int ent_mdl_id = s->getBspModelIdx();
		if (ent_mdl_id >= 0)
		{
			if (!used_models.count(ent_mdl_id))
			{
				used_models.insert(ent_mdl_id);
			}
		}
	}

	for (unsigned int i = 0; i < modelCount; i++)
	{
		if (!used_models.count(i))
		{
			logf("Warning: in map %s found unused model: %d.\n", bsp_name.c_str(), i);
		}
	}


	return isValid;
}

std::vector<STRUCTUSAGE*> Bsp::get_sorted_model_infos(int sortMode) {
	std::vector<STRUCTUSAGE*> modelStructs;
	modelStructs.resize(modelCount);

	for (unsigned int i = 0; i < modelCount; i++) {
		modelStructs[i] = new STRUCTUSAGE(this);
		modelStructs[i]->modelIdx = i;
		mark_model_structures(i, modelStructs[i], false);
		modelStructs[i]->compute_sum();
	}

	g_sort_mode = sortMode;
	sort(modelStructs.begin(), modelStructs.end(), sortModelInfos);

	return modelStructs;
}

void Bsp::print_info(bool perModelStats, int perModelLimit, int sortMode) {
	size_t entCount = ents.size();

	if (perModelStats) {
		g_sort_mode = sortMode;

		if (planeCount >= MAX_MAP_PLANES || texinfoCount >= MAX_MAP_TEXINFOS || leafCount >= MAX_MAP_LEAVES ||
			modelCount >= MAX_MAP_MODELS || nodeCount >= MAX_MAP_NODES || vertCount >= MAX_MAP_VERTS ||
			faceCount >= MAX_MAP_FACES || clipnodeCount >= MAX_MAP_CLIPNODES || marksurfCount >= MAX_MAP_MARKSURFS ||
			surfedgeCount >= MAX_MAP_SURFEDGES || edgeCount >= MAX_MAP_EDGES || textureCount >= MAX_MAP_TEXTURES ||
			lightDataLength >= MAX_MAP_LIGHTDATA || visDataLength >= MAX_MAP_VISDATA)
		{
			logf("Unable to show model stats while BSP limits are exceeded.\n");
			return;
		}

		std::vector<STRUCTUSAGE*> modelStructs = get_sorted_model_infos(sortMode);

		int maxCount = 0;
		const char* countName = "None";

		switch (g_sort_mode) {
		case SORT_VERTS:		maxCount = vertCount; countName = "  Verts";  break;
		case SORT_NODES:		maxCount = nodeCount; countName = "  Nodes";  break;
		case SORT_CLIPNODES:	maxCount = clipnodeCount; countName = "Clipnodes";  break;
		case SORT_FACES:		maxCount = faceCount; countName = "  Faces";  break;
		}

		logf("       Classname                  Targetname          Model  %-10s  Usage\n", countName);
		logf("-------------------------  -------------------------  -----  ----------  --------\n");

		for (unsigned int i = 0; i < modelCount && i < (unsigned int)perModelLimit; i++) {

			int val = 0;
			switch (g_sort_mode) {
			case SORT_VERTS:		val = modelStructs[i]->sum.verts; break;
			case SORT_NODES:		val = modelStructs[i]->sum.nodes; break;
			case SORT_CLIPNODES:	val = modelStructs[i]->sum.clipnodes; break;
			case SORT_FACES:		val = modelStructs[i]->sum.faces; break;
			}

			if (val == 0)
				break;

			print_model_stat(modelStructs[i], val, maxCount, false);
		}
	}
	else {
		logf(" Data Type     Current / Max       Fullness\n");
		logf("------------  -------------------  --------\n");
		print_stat("models", modelCount, MAX_MAP_MODELS, false);
		print_stat("planes", planeCount, MAX_MAP_PLANES, false);
		print_stat("vertexes", vertCount, MAX_MAP_VERTS, false);
		print_stat("nodes", nodeCount, MAX_MAP_NODES, false);
		print_stat("texinfos", texinfoCount, MAX_MAP_TEXINFOS, false);
		print_stat("faces", faceCount, MAX_MAP_FACES, false);
		print_stat("clipnodes", clipnodeCount, MAX_MAP_CLIPNODES, false);
		print_stat("leaves", leafCount, MAX_MAP_LEAVES, false);
		print_stat("marksurfaces", marksurfCount, MAX_MAP_MARKSURFS, false);
		print_stat("surfedges", surfedgeCount, MAX_MAP_SURFEDGES, false);
		print_stat("edges", edgeCount, MAX_MAP_SURFEDGES, false);
		print_stat("textures", textureCount, MAX_MAP_TEXTURES, false);
		print_stat("lightdata", lightDataLength, MAX_MAP_LIGHTDATA, true);
		print_stat("visdata", visDataLength, MAX_MAP_VISDATA, true);
		print_stat("entities", (unsigned int)entCount, MAX_MAP_ENTS, false);
	}
}

void Bsp::print_model_bsp(int modelIdx) {
	int node = models[modelIdx].iHeadnodes[0];
	recurse_node(node, 0);
}

void Bsp::print_clipnode_tree(int iNode, int depth) {
	for (int i = 0; i < depth; i++) {
		logf("    ");
	}

	if (iNode < 0) {
		logf(getLeafContentsName(iNode));
		logf("\n");
		return;
	}
	else {
		BSPPLANE& plane = planes[clipnodes[iNode].iPlane];
		logf("NODE (%.2f, %.2f, %.2f) @ %.2f\n", plane.vNormal.x, plane.vNormal.y, plane.vNormal.z, plane.fDist);
	}


	for (int i = 0; i < 2; i++) {
		print_clipnode_tree(clipnodes[iNode].iChildren[i], depth + 1);
	}
}

void Bsp::print_model_hull(int modelIdx, int hull_number) {
	if (modelIdx < 0 || modelIdx > bsp_header.lump[LUMP_MODELS].nLength / sizeof(BSPMODEL)) {
		logf("Invalid model index %d. Must be 0 - %d\n", modelIdx);
		return;
	}

	// the first hull is used for point-sized clipping, but uses nodes and not clipnodes.
	if (hull_number < 0 || hull_number >= MAX_MAP_HULLS) {
		logf("Invalid hull number. Clipnode hull numbers are 0 - %d\n", MAX_MAP_HULLS);
		return;
	}

	BSPMODEL& model = models[modelIdx];

	logf("Model %d Hull %d - %s\n", modelIdx, hull_number, get_model_usage(modelIdx).c_str());

	if (hull_number == 0)
		print_model_bsp(modelIdx);
	else
		print_clipnode_tree(model.iHeadnodes[hull_number], 0);
}

std::string Bsp::get_model_usage(int modelIdx) {
	for (int i = 0; i < ents.size(); i++) {
		if (ents[i]->getBspModelIdx() == modelIdx) {
			return "\"" + ents[i]->keyvalues["targetname"] + "\" (" + ents[i]->keyvalues["classname"] + ")";
		}
	}
	return "(unused)";
}

std::vector<Entity*> Bsp::get_model_ents(int modelIdx) {
	std::vector<Entity*> uses;
	for (int i = 0; i < ents.size(); i++) {
		if (ents[i]->getBspModelIdx() == modelIdx) {
			uses.push_back(ents[i]);
		}
	}
	return uses;
}

std::vector<int> Bsp::get_model_ents_ids(int modelIdx) {
	std::vector<int> uses;
	for (int i = 0; i < ents.size(); i++) {
		if (ents[i]->getBspModelIdxForce() == modelIdx) {
			uses.push_back(i);
		}
	}
	return uses;
}

void Bsp::recurse_node(short nodeIdx, int depth) {
	for (int i = 0; i < depth; i++) {
		logf("    ");
	}

	if (nodeIdx < 0) {
		BSPLEAF& leaf = leaves[~nodeIdx];
		print_leaf(leaf);
		logf(" (LEAF %d)\n", ~nodeIdx);
		return;
	}
	else {
		print_node(nodes[nodeIdx]);
		logf("\n");
	}

	recurse_node(nodes[nodeIdx].iChildren[0], depth + 1);
	recurse_node(nodes[nodeIdx].iChildren[1], depth + 1);
}

void Bsp::print_node(const BSPNODE& node) {
	BSPPLANE& plane = planes[node.iPlane];

	logf("Plane (%f %f %f) d: %f, Faces: %d, Min(%d, %d, %d), Max(%d, %d, %d)",
		plane.vNormal.x, plane.vNormal.y, plane.vNormal.z,
		plane.fDist, node.nFaces,
		node.nMins[0], node.nMins[1], node.nMins[2],
		node.nMaxs[0], node.nMaxs[1], node.nMaxs[2]);
}

int Bsp::pointContents(int iNode, const vec3& p, int hull, std::vector<int>& nodeBranch, int& leafIdx, int& childIdx) {
	if (iNode < 0) {
		leafIdx = -1;
		childIdx = -1;
		return CONTENTS_EMPTY;
	}

	if (hull == 0) {
		while (iNode >= 0)
		{
			nodeBranch.push_back(iNode);
			BSPNODE& node = nodes[iNode];
			BSPPLANE& plane = planes[node.iPlane];

			float d = dotProduct(plane.vNormal, p) - plane.fDist;
			if (d < 0) {
				iNode = node.iChildren[1];
				childIdx = 1;
			}
			else {
				iNode = node.iChildren[0];
				childIdx = 0;
			}
		}

		leafIdx = ~iNode;
		return leaves[~iNode].nContents;
	}
	else {
		while (iNode >= 0)
		{
			nodeBranch.push_back(iNode);
			BSPCLIPNODE& node = clipnodes[iNode];
			BSPPLANE& plane = planes[node.iPlane];

			float d = dotProduct(plane.vNormal, p) - plane.fDist;
			if (d < 0) {
				iNode = node.iChildren[1];
				childIdx = 1;
			}
			else {
				iNode = node.iChildren[0];
				childIdx = 0;
			}
		}

		return iNode;
	}
}

int Bsp::pointContents(int iNode, const vec3& p, int hull) {
	std::vector<int> nodeBranch;
	int leafIdx;
	int childIdx;
	return pointContents(iNode, p, hull, nodeBranch, leafIdx, childIdx);
}

const char* Bsp::getLeafContentsName(int contents) {
	switch (contents) {
	case CONTENTS_EMPTY:
		return "EMPTY";
	case CONTENTS_SOLID:
		return "SOLID";
	case CONTENTS_WATER:
		return "WATER";
	case CONTENTS_SLIME:
		return "SLIME";
	case CONTENTS_LAVA:
		return "LAVA";
	case CONTENTS_SKY:
		return "SKY";
	case CONTENTS_ORIGIN:
		return "ORIGIN";
	case CONTENTS_CURRENT_0:
		return "CURRENT_0";
	case CONTENTS_CURRENT_90:
		return "CURRENT_90";
	case CONTENTS_CURRENT_180:
		return "CURRENT_180";
	case CONTENTS_CURRENT_270:
		return "CURRENT_270";
	case CONTENTS_CURRENT_UP:
		return "CURRENT_UP";
	case CONTENTS_CURRENT_DOWN:
		return "CURRENT_DOWN";
	case CONTENTS_TRANSLUCENT:
		return "TRANSLUCENT";
	default:
		return "UNKNOWN";
	}
}

void Bsp::mark_face_structures(int iFace, STRUCTUSAGE* usage) {
	BSPFACE& face = faces[iFace];
	usage->faces[iFace] = true;

	for (int e = 0; e < face.nEdges; e++) {
		int edgeIdx = surfedges[face.iFirstEdge + e];
		BSPEDGE& edge = edges[abs(edgeIdx)];
		int vertIdx = edgeIdx >= 0 ? edge.iVertex[1] : edge.iVertex[0];

		usage->surfEdges[face.iFirstEdge + e] = true;
		usage->edges[abs(edgeIdx)] = true;
		usage->verts[vertIdx] = true;
	}

	usage->texInfo[face.iTextureInfo] = true;
	usage->planes[face.iPlane] = true;
	usage->textures[texinfos[face.iTextureInfo].iMiptex] = true;
}

void Bsp::mark_node_structures(int iNode, STRUCTUSAGE* usage, bool skipLeaves) {
	BSPNODE& node = nodes[iNode];

	usage->nodes[iNode] = true;
	usage->planes[node.iPlane] = true;

	for (int i = 0; i < node.nFaces; i++) {
		mark_face_structures(node.firstFace + i, usage);
	}

	for (int i = 0; i < 2; i++) {
		if (node.iChildren[i] >= 0) {
			mark_node_structures(node.iChildren[i], usage, skipLeaves);
		}
		else if (!skipLeaves) {
			BSPLEAF& leaf = leaves[~node.iChildren[i]];
			for (int n = 0; n < leaf.nMarkSurfaces; n++) {
				usage->markSurfs[leaf.iFirstMarkSurface + n] = true;
				mark_face_structures(marksurfs[leaf.iFirstMarkSurface + n], usage);
			}

			usage->leaves[~node.iChildren[i]] = true;
		}
	}
}

void Bsp::mark_clipnode_structures(int iNode, STRUCTUSAGE* usage) {
	BSPCLIPNODE& node = clipnodes[iNode];

	usage->clipnodes[iNode] = true;
	usage->planes[node.iPlane] = true;

	for (int i = 0; i < 2; i++) {
		if (node.iChildren[i] >= 0) {
			mark_clipnode_structures(node.iChildren[i], usage);
		}
	}
}

void Bsp::mark_model_structures(int modelIdx, STRUCTUSAGE* usage, bool skipLeaves) {
	BSPMODEL& model = models[modelIdx];

	for (int i = 0; i < model.nFaces; i++) {
		mark_face_structures(model.iFirstFace + i, usage);
	}
	if (model.iHeadnodes[0] >= 0)
		mark_node_structures(model.iHeadnodes[0], usage, skipLeaves);
	for (int k = 1; k < MAX_MAP_HULLS; k++) {
		if (model.iHeadnodes[k] >= 0 && model.iHeadnodes[k] < (int)clipnodeCount)
			mark_clipnode_structures(model.iHeadnodes[k], usage);
	}
}

void Bsp::remap_face_structures(int faceIdx, STRUCTREMAP* remap) {
	if (remap->visitedFaces[faceIdx]) {
		return;
	}
	remap->visitedFaces[faceIdx] = true;

	BSPFACE& face = faces[faceIdx];

	face.iPlane = remap->planes[face.iPlane];
	face.iTextureInfo = remap->texInfo[face.iTextureInfo];
	//logf("REMAP FACE %d: %d -> %d\n", faceIdx, face.iFirstEdge, remap->surfEdges[face.iFirstEdge]);
	//logf("REMAP FACE %d: %d -> %d\n", faceIdx, face.iTextureInfo, remap->texInfo[face.iTextureInfo]);
	//face.iFirstEdge = remap->surfEdges[face.iFirstEdge];
}

void Bsp::remap_node_structures(int iNode, STRUCTREMAP* remap) {
	BSPNODE& node = nodes[iNode];

	remap->visitedNodes[iNode] = true;

	node.iPlane = remap->planes[node.iPlane];

	for (int i = 0; i < node.nFaces; i++) {
		remap_face_structures(node.firstFace + i, remap);
	}

	for (int i = 0; i < 2; i++) {
		if (node.iChildren[i] >= 0) {
			node.iChildren[i] = remap->nodes[node.iChildren[i]];
			if (!remap->visitedNodes[node.iChildren[i]]) {
				remap_node_structures(node.iChildren[i], remap);
			}
		}
	}
}

void Bsp::remap_clipnode_structures(int iNode, STRUCTREMAP* remap) {
	BSPCLIPNODE& node = clipnodes[iNode];

	remap->visitedClipnodes[iNode] = true;
	node.iPlane = remap->planes[node.iPlane];

	for (int i = 0; i < 2; i++) {
		if (node.iChildren[i] >= 0) {
			if (node.iChildren[i] < (int)remap->count.clipnodes) {
				node.iChildren[i] = remap->clipnodes[node.iChildren[i]];
			}

			if (!remap->visitedClipnodes[node.iChildren[i]])
				remap_clipnode_structures(node.iChildren[i], remap);
		}
	}
}

void Bsp::remap_model_structures(int modelIdx, STRUCTREMAP* remap) {
	BSPMODEL& model = ((BSPMODEL*)lumps[LUMP_MODELS])[modelIdx];

	// sometimes the face index is invalid when the model has no faces
	if (model.nFaces > 0)
		model.iFirstFace = remap->faces[model.iFirstFace];

	if (model.iHeadnodes[0] >= 0) {
		model.iHeadnodes[0] = remap->nodes[model.iHeadnodes[0]];
		if (model.iHeadnodes[0] < (int)clipnodeCount && !remap->visitedNodes[model.iHeadnodes[0]]) {
			remap_node_structures(model.iHeadnodes[0], remap);
		}
	}
	for (int k = 1; k < MAX_MAP_HULLS; k++) {
		if (model.iHeadnodes[k] >= 0) {
			model.iHeadnodes[k] = remap->clipnodes[model.iHeadnodes[k]];
			if (model.iHeadnodes[k] < (int)clipnodeCount && !remap->visitedClipnodes[model.iHeadnodes[k]]) {
				remap_clipnode_structures(model.iHeadnodes[k], remap);
			}
		}
	}
}

void Bsp::delete_hull(int hull_number, int redirect) {
	if (hull_number < 0 || hull_number >= MAX_MAP_HULLS) {
		logf("Invalid hull number. Valid hull numbers are 1-%d\n", MAX_MAP_HULLS);
		return;
	}

	for (unsigned int i = 0; i < modelCount; i++) {
		delete_hull(hull_number, i, redirect);
	}
}

void Bsp::delete_hull(int hull_number, int modelIdx, int redirect) {
	if (modelIdx < 0 || (unsigned int)modelIdx >= modelCount) {
		logf("Invalid model index %d. Must be 0-%d\n", modelIdx);
		return;
	}

	// the first hull is used for point-sized clipping, but uses nodes and not clipnodes.
	if (hull_number < 0 || hull_number >= MAX_MAP_HULLS) {
		logf("Invalid hull number. Valid hull numbers are 1-%d\n", MAX_MAP_HULLS);
		return;
	}

	if (redirect >= MAX_MAP_HULLS) {
		logf("Invalid redirect hull number. Valid redirect hulls are 1-%d\n", MAX_MAP_HULLS);
		return;
	}

	if (hull_number == 0 && redirect > 0) {
		logf("Hull 0 can't be redirected. Hull 0 is the only hull that doesn't use clipnodes.\n", MAX_MAP_HULLS);
		return;
	}

	BSPMODEL& model = models[modelIdx];

	if (hull_number == 0) {
		model.iHeadnodes[0] = -1; // redirect to solid leaf
		model.nVisLeafs = 0;
		model.nFaces = 0;
		model.iFirstFace = 0;
	}
	else if (redirect > 0) {
		if (model.iHeadnodes[hull_number] > 0 && model.iHeadnodes[redirect] < 0) {
			//logf("WARNING: HULL %d is empty\n", redirect);
		}
		else if (model.iHeadnodes[hull_number] == model.iHeadnodes[redirect]) {
			//logf("WARNING: HULL %d and %d are already sharing clipnodes\n", hull_number, redirect);
		}
		model.iHeadnodes[hull_number] = model.iHeadnodes[redirect];
	}
	else {
		model.iHeadnodes[hull_number] = CONTENTS_EMPTY;
	}
}

void Bsp::delete_model(int modelIdx) {
	unsigned char* oldModels = (unsigned char*)models;

	int newSize = (modelCount - 1) * sizeof(BSPMODEL);
	unsigned char* newModels = new unsigned char[newSize];

	memcpy(newModels, oldModels, modelIdx * sizeof(BSPMODEL));
	memcpy(newModels + modelIdx * sizeof(BSPMODEL),
		oldModels + (modelIdx + 1) * sizeof(BSPMODEL),
		(modelCount - (modelIdx + 1)) * sizeof(BSPMODEL));

	replace_lump(LUMP_MODELS, newModels, newSize);

	// update model index references
	for (int i = 0; i < ents.size(); i++) {
		int entModel = ents[i]->getBspModelIdx();
		if (entModel == modelIdx) {
			ents[i]->setOrAddKeyvalue("model", "error.mdl");
		}
		else if (entModel > modelIdx) {
			ents[i]->setOrAddKeyvalue("model", "*" + std::to_string(entModel - 1));
		}
	}
}

int Bsp::create_solid(const vec3& mins, const vec3& maxs, int textureIdx) {
	int newModelIdx = create_model();
	BSPMODEL& newModel = models[newModelIdx];

	create_node_box(mins, maxs, &newModel, textureIdx);
	create_clipnode_box(mins, maxs, &newModel);

	//remove_unused_model_structures(); // will also resize VIS data for new leaf count

	return newModelIdx;
}

int Bsp::create_solid(Solid& solid, int targetModelIdx) {
	int modelIdx = targetModelIdx >= 0 ? targetModelIdx : create_model();
	BSPMODEL& newModel = models[modelIdx];

	create_nodes(solid, &newModel);
	regenerate_clipnodes(modelIdx, -1);

	return modelIdx;
}

void Bsp::add_model(Bsp* sourceMap, int modelIdx) {
	STRUCTUSAGE usage(sourceMap);
	sourceMap->mark_model_structures(modelIdx, &usage, false);

	// TODO: add the model lel

	usage.compute_sum();

	logf("");
}

BSPMIPTEX* Bsp::find_embedded_texture(const char* name) {
	if (!name || name[0] == '\0')
		return NULL;
	for (unsigned int i = 0; i < textureCount; i++) {
		int oldOffset = ((int*)textures)[i + 1];
		BSPMIPTEX* oldTex = (BSPMIPTEX*)(textures + oldOffset);
		if (strcmp(name, oldTex->szName) == 0)
		{
			return oldTex;
		}
	}
	return NULL;
}

int Bsp::add_texture(const char* name, unsigned char* data, int width, int height) {
	if (width % 16 != 0 || height % 16 != 0) {
		logf("Dimensions not divisible by 16");
		return -1;
	}
	if (width > MAX_TEXTURE_DIMENSION || height > MAX_TEXTURE_DIMENSION) {
		logf("Width/height too large");
		return -1;
	}

	BSPMIPTEX* oldtex = find_embedded_texture(name);

	if (oldtex)
	{
		logf("Texture with name %s found in map. Just replace it.\n", name);
		if (oldtex->nWidth != width || oldtex->nHeight != height)
		{
			snprintf(oldtex->szName, sizeof(oldtex->szName), "%s", "-unused_texture");
			logf("Warning! Texture size different. Need rename old texture.\n");
			oldtex = NULL;
		}
		else if (oldtex->nOffsets[0] == 0)
		{
			snprintf(oldtex->szName, sizeof(oldtex->szName), "%s", "-unused_texture");
			logf("Warning! Texture pointer found. Need replace by new texture.\n");
		}
	}

	COLOR3 palette[256];
	memset(&palette, 0, sizeof(COLOR3) * 256);
	int colorCount = 0;

	// create pallete and full-rez mipmap
	unsigned char* mip[MIPLEVELS];
	mip[0] = new unsigned char[width * height];
	COLOR3* src = (COLOR3*)data;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int paletteIdx = -1;
			for (int k = 0; k < colorCount; k++) {
				if (*src == palette[k]) {
					paletteIdx = k;
					break;
				}
			}
			if (paletteIdx == -1) {
				if (colorCount >= 256) {
					logf("Too many colors");
					delete[] mip[0];
					return -1;
				}
				palette[colorCount] = *src;
				paletteIdx = colorCount;
				colorCount++;
			}

			mip[0][y * width + x] = paletteIdx;
			src++;
		}
	}

	int texDataSize = width * height + sizeof(COLOR3) * 256 + 4; // 4 = padding

	// generate mipmaps
	for (int i = 1; i < MIPLEVELS; i++) {
		int div = 1 << i;
		int mipWidth = width / div;
		int mipHeight = height / div;
		texDataSize += mipWidth * height;
		mip[i] = new unsigned char[mipWidth * mipHeight];

		src = (COLOR3*)data;
		for (int y = 0; y < mipHeight; y++) {
			for (int x = 0; x < mipWidth; x++) {

				int paletteIdx = -1;
				for (int k = 0; k < colorCount; k++) {
					if (*src == palette[k]) {
						paletteIdx = k;
						break;
					}
				}

				mip[i][y * mipWidth + x] = paletteIdx;
				src += div;
			}
		}
	}

	if (oldtex && oldtex->nOffsets[0] > 0)
	{
		memcpy((unsigned char*)oldtex + oldtex->nOffsets[0], mip[0], width * height);
		memcpy((unsigned char*)oldtex + oldtex->nOffsets[1], mip[1], (width >> 1) * (height >> 1));
		memcpy((unsigned char*)oldtex + oldtex->nOffsets[2], mip[2], (width >> 2) * (height >> 2));
		memcpy((unsigned char*)oldtex + oldtex->nOffsets[3], mip[3], (width >> 3) * (height >> 3));
		memcpy((unsigned char*)oldtex + (oldtex->nOffsets[3] + (width >> 3) * (height >> 3) + 2), palette, sizeof(COLOR3) * 256);
		for (int i = 0; i < MIPLEVELS; i++) {
			delete[] mip[i];
		}
		return 0;
	}
	else if (oldtex)
	{
		for (unsigned int i = 0; i < faceCount; i++)
		{
			BSPFACE& face = faces[i];
			BSPTEXTUREINFO& texinfo = texinfos[face.iTextureInfo];

			int texOffset = ((int*)textures)[texinfo.iMiptex + 1];
			BSPMIPTEX* tex = ((BSPMIPTEX*)(textures + texOffset));
			if (tex == oldtex)
				texinfo.iMiptex = textureCount;
		}
	}

	size_t newTexLumpSize = bsp_header.lump[LUMP_TEXTURES].nLength + sizeof(int) + sizeof(BSPMIPTEX) + texDataSize;
	unsigned char* newTexData = new unsigned char[newTexLumpSize];
	memset(newTexData, 0, sizeof(newTexLumpSize));

	// create new texture lump header
	int* newLumpHeader = (int*)newTexData;
	int* oldLumpHeader = (int*)lumps[LUMP_TEXTURES];
	*newLumpHeader = textureCount + 1;

	for (unsigned int i = 0; i < textureCount; i++) {
		*(newLumpHeader + i + 1) = *(oldLumpHeader + i + 1) + sizeof(int); // make room for the new offset
	}

	// copy old texture data
	size_t oldTexHeaderSize = (textureCount + 1) * sizeof(int);
	size_t newTexHeaderSize = oldTexHeaderSize + sizeof(int);
	size_t oldTexDatSize = bsp_header.lump[LUMP_TEXTURES].nLength - (textureCount + 1) * sizeof(int);
	memcpy(newTexData + newTexHeaderSize, lumps[LUMP_TEXTURES] + oldTexHeaderSize, oldTexDatSize);

	// add new texture to the end of the lump
	size_t newTexOffset = newTexHeaderSize + oldTexDatSize;
	newLumpHeader[textureCount + 1] = (int)newTexOffset;
	BSPMIPTEX* newMipTex = (BSPMIPTEX*)(newTexData + newTexOffset);
	newMipTex->nWidth = width;
	newMipTex->nHeight = height;
	memcpy(newMipTex->szName, name, MAXTEXTURENAME);

	newMipTex->nOffsets[0] = sizeof(BSPMIPTEX);
	newMipTex->nOffsets[1] = newMipTex->nOffsets[0] + width * height;
	newMipTex->nOffsets[2] = newMipTex->nOffsets[1] + (width >> 1) * (height >> 1);
	newMipTex->nOffsets[3] = newMipTex->nOffsets[2] + (width >> 2) * (height >> 2);
	size_t palleteOffset = newMipTex->nOffsets[3] + (width >> 3) * (height >> 3) + 2;

	memcpy(newTexData + newTexOffset + newMipTex->nOffsets[0], mip[0], width * height);
	memcpy(newTexData + newTexOffset + newMipTex->nOffsets[1], mip[1], (width >> 1) * (height >> 1));
	memcpy(newTexData + newTexOffset + newMipTex->nOffsets[2], mip[2], (width >> 2) * (height >> 2));
	memcpy(newTexData + newTexOffset + newMipTex->nOffsets[3], mip[3], (width >> 3) * (height >> 3));
	memcpy(newTexData + newTexOffset + palleteOffset, palette, sizeof(COLOR3) * 256);

	for (int i = 0; i < MIPLEVELS; i++) {
		delete[] mip[i];
	}

	replace_lump(LUMP_TEXTURES, newTexData, newTexLumpSize);

	return textureCount - 1;
}

int Bsp::create_leaf(int contents) {
	BSPLEAF* newLeaves = new BSPLEAF[leafCount + 1];
	memcpy(newLeaves, leaves, leafCount * sizeof(BSPLEAF));

	BSPLEAF& newLeaf = newLeaves[leafCount];
	memset(&newLeaf, 0, sizeof(BSPLEAF));

	newLeaf.nVisOffset = -1;
	newLeaf.nContents = contents;

	unsigned int newLeafIdx = leafCount;

	replace_lump(LUMP_LEAVES, newLeaves, (leafCount + 1) * sizeof(BSPLEAF));

	return newLeafIdx;
}

void Bsp::create_node_box(const vec3& min, const vec3& max, BSPMODEL* targetModel, int textureIdx) {

	// add new verts (1 for each corner)
	// TODO: subdivide faces to prevent max surface extents error
	unsigned int startVert = vertCount;
	{
		vec3* newVerts = new vec3[vertCount + 8];
		memcpy(newVerts, verts, vertCount * sizeof(vec3));

		newVerts[vertCount + 0] = vec3(min.x, min.y, min.z); // front-left-bottom
		newVerts[vertCount + 1] = vec3(max.x, min.y, min.z); // front-right-bottom
		newVerts[vertCount + 2] = vec3(max.x, max.y, min.z); // back-right-bottom
		newVerts[vertCount + 3] = vec3(min.x, max.y, min.z); // back-left-bottom

		newVerts[vertCount + 4] = vec3(min.x, min.y, max.z); // front-left-top
		newVerts[vertCount + 5] = vec3(max.x, min.y, max.z); // front-right-top
		newVerts[vertCount + 6] = vec3(max.x, max.y, max.z); // back-right-top
		newVerts[vertCount + 7] = vec3(min.x, max.y, max.z); // back-left-top

		replace_lump(LUMP_VERTICES, newVerts, (vertCount + 8) * sizeof(vec3));
	}

	// add new edges (4 for each face)
	// TODO: subdivide >512
	unsigned int startEdge = edgeCount;
	{
		BSPEDGE* newEdges = new BSPEDGE[edgeCount + 12];
		memcpy(newEdges, edges, edgeCount * sizeof(BSPEDGE));

		// left
		newEdges[startEdge + 0] = BSPEDGE(startVert + 3, startVert + 0);
		newEdges[startEdge + 1] = BSPEDGE(startVert + 4, startVert + 7);

		// right
		newEdges[startEdge + 2] = BSPEDGE(startVert + 1, startVert + 2); // bottom edge
		newEdges[startEdge + 3] = BSPEDGE(startVert + 6, startVert + 5); // right edge

		// front
		newEdges[startEdge + 4] = BSPEDGE(startVert + 0, startVert + 1); // bottom edge
		newEdges[startEdge + 5] = BSPEDGE(startVert + 5, startVert + 4); // top edge

		// back
		newEdges[startEdge + 6] = BSPEDGE(startVert + 3, startVert + 7); // left edge
		newEdges[startEdge + 7] = BSPEDGE(startVert + 6, startVert + 2); // right edge

		// bottom
		newEdges[startEdge + 8] = BSPEDGE(startVert + 3, startVert + 2);
		newEdges[startEdge + 9] = BSPEDGE(startVert + 1, startVert + 0);

		// top
		newEdges[startEdge + 10] = BSPEDGE(startVert + 7, startVert + 4);
		newEdges[startEdge + 11] = BSPEDGE(startVert + 5, startVert + 6);

		replace_lump(LUMP_EDGES, newEdges, (edgeCount + 12) * sizeof(BSPEDGE));
	}

	// add new surfedges (2 for each edge)
	unsigned int startSurfedge = surfedgeCount;
	{
		int* newSurfedges = new int[surfedgeCount + 24];
		memcpy(newSurfedges, surfedges, surfedgeCount * sizeof(int));

		// reverse cuz i fucked the edge order and I don't wanna redo
		for (int i = 12 - 1; i >= 0; i--) {
			int edgeIdx = startEdge + i;
			newSurfedges[startSurfedge + (i * 2)] = -edgeIdx; // negative = use second vertex in edge
			newSurfedges[startSurfedge + (i * 2) + 1] = edgeIdx;
		}

		replace_lump(LUMP_SURFEDGES, newSurfedges, (surfedgeCount + 24) * sizeof(int));
	}

	// add new planes (1 for each face/node)
	unsigned int startPlane = planeCount;
	{
		BSPPLANE* newPlanes = new BSPPLANE[planeCount + 6];
		memcpy(newPlanes, planes, planeCount * sizeof(BSPPLANE));

		newPlanes[startPlane + 0] = { vec3(1, 0, 0), min.x, PLANE_X }; // left
		newPlanes[startPlane + 1] = { vec3(1, 0, 0), max.x, PLANE_X }; // right
		newPlanes[startPlane + 2] = { vec3(0, 1, 0), min.y, PLANE_Y }; // front
		newPlanes[startPlane + 3] = { vec3(0, 1, 0), max.y, PLANE_Y }; // back
		newPlanes[startPlane + 4] = { vec3(0, 0, 1), min.z, PLANE_Z }; // bottom
		newPlanes[startPlane + 5] = { vec3(0, 0, 1), max.z, PLANE_Z }; // top

		replace_lump(LUMP_PLANES, newPlanes, (planeCount + 6) * sizeof(BSPPLANE));
	}

	unsigned int startTexinfo = texinfoCount;
	{
		BSPTEXTUREINFO* newTexinfos = new BSPTEXTUREINFO[texinfoCount + 6];
		memcpy(newTexinfos, texinfos, texinfoCount * sizeof(BSPTEXTUREINFO));

		vec3 up = vec3(0, 0, 1);
		vec3 right = vec3(1, 0, 0);
		vec3 forward = vec3(0, 1, 0);

		vec3 faceNormals[6]{
			vec3(-1, 0, 0),	// left
			vec3(1, 0, 0), // right
			vec3(0, 1, 0), // front
			vec3(0, -1, 0), // back
			vec3(0, 0, -1), // bottom
			vec3(0, 0, 1) // top
		};
		vec3 faceUp[6]{
			vec3(0, 0, 1),	// left
			vec3(0, 0, 1), // right
			vec3(0, 0, 1), // front
			vec3(0, 0, 1), // back
			vec3(0, -1, 0), // bottom
			vec3(0, 1, 0) // top
		};

		for (int i = 0; i < 6; i++) {
			BSPTEXTUREINFO& info = newTexinfos[startTexinfo + i];
			info.iMiptex = textureIdx;
			info.nFlags = TEX_SPECIAL;
			info.shiftS = 0;
			info.shiftT = 0;
			info.vT = faceUp[i];
			info.vS = crossProduct(faceUp[i], faceNormals[i]);
			// TODO: fit texture to face
		}

		replace_lump(LUMP_TEXINFO, newTexinfos, (texinfoCount + 6) * sizeof(BSPTEXTUREINFO));
	}

	// add new faces
	unsigned int startFace = faceCount;
	{
		BSPFACE* newFaces = new BSPFACE[faceCount + 6];
		memcpy(newFaces, faces, faceCount * sizeof(BSPFACE));

		for (int i = 0; i < 6; i++) {
			BSPFACE& face = newFaces[faceCount + i];
			face.iFirstEdge = startSurfedge + i * 4;
			face.iPlane = startPlane + i;
			face.nEdges = 4;
			face.nPlaneSide = i % 2 == 0; // even-numbered planes are inverted
			face.iTextureInfo = startTexinfo + i;
			face.nLightmapOffset = 0; // TODO: Lighting
			memset(face.nStyles, 255, 4);
		}

		replace_lump(LUMP_FACES, newFaces, (faceCount + 6) * sizeof(BSPFACE));
	}

	// Submodels don't use leaves like the world does. Everything except nContents is ignored.
	// There's really no need to create leaves for submodels. Every map will have a shared
	// SOLID leaf, and there should be at least one EMPTY leaf if the map isn't completely solid.
	// So, just find an existing EMPTY leaf. Also, water brushes work just fine with SOLID nodes.
	// The inner contents of a node is changed dynamically by entity properties.
	short sharedSolidLeaf = 0;
	short anyEmptyLeaf = 0;
	for (unsigned int i = 0; i < leafCount; i++) {
		if (leaves[i].nContents == CONTENTS_EMPTY) {
			anyEmptyLeaf = i;
			break;
		}
	}
	// If emptyLeaf is still 0 (SOLID), it means the map is fully solid, so the contents wouldn't matter.
	// Anyway, still setting this in case someone wants to copy the model to another map
	if (anyEmptyLeaf == 0) {
		anyEmptyLeaf = create_leaf(CONTENTS_EMPTY);
		targetModel->nVisLeafs = 1;
	}
	else {
		targetModel->nVisLeafs = 0;
	}

	// add new nodes
	unsigned int startNode = nodeCount;
	{
		BSPNODE* newNodes = new BSPNODE[nodeCount + 6];
		memcpy(newNodes, nodes, nodeCount * sizeof(BSPNODE));

		unsigned int nodeIdx = nodeCount;

		for (int k = 0; k < 6; k++) {
			BSPNODE& node = newNodes[nodeCount + k];
			memset(&node, 0, sizeof(BSPNODE));

			node.firstFace = startFace + k; // face required for decals
			node.nFaces = 1;
			node.iPlane = startPlane + k;
			// node mins/maxs don't matter for submodels. Leave them at 0.

			short insideContents = k == 5 ? ~sharedSolidLeaf : (short)(nodeCount + k + 1);
			short outsideContents = ~anyEmptyLeaf;

			// can't have negative normals on planes so children are swapped instead
			if (k % 2 == 0) {
				node.iChildren[0] = insideContents;
				node.iChildren[1] = outsideContents;
			}
			else {
				node.iChildren[0] = outsideContents;
				node.iChildren[1] = insideContents;
			}
		}

		replace_lump(LUMP_NODES, newNodes, (nodeCount + 6) * sizeof(BSPNODE));
	}

	targetModel->iHeadnodes[0] = startNode;
	targetModel->iFirstFace = startFace;
	targetModel->nFaces = 6;

	targetModel->nMaxs = vec3(FLT_MIN_COORD, FLT_MIN_COORD, FLT_MIN_COORD);
	targetModel->nMins = vec3(FLT_MAX_COORD, FLT_MAX_COORD, FLT_MAX_COORD);
	for (int i = 0; i < 8; i++) {
		vec3 v = verts[startVert + i];

		if (v.x > targetModel->nMaxs.x) targetModel->nMaxs.x = v.x;
		if (v.y > targetModel->nMaxs.y) targetModel->nMaxs.y = v.y;
		if (v.z > targetModel->nMaxs.z) targetModel->nMaxs.z = v.z;

		if (v.x < targetModel->nMins.x) targetModel->nMins.x = v.x;
		if (v.y < targetModel->nMins.y) targetModel->nMins.y = v.y;
		if (v.z < targetModel->nMins.z) targetModel->nMins.z = v.z;
	}
}

void Bsp::create_nodes(Solid& solid, BSPMODEL* targetModel) {

	std::vector<int> newVertIndexes;
	unsigned int startVert = vertCount;
	{
		vec3* newVerts = new vec3[vertCount + solid.hullVerts.size()];
		memcpy(newVerts, verts, vertCount * sizeof(vec3));

		for (unsigned int i = 0; i < solid.hullVerts.size(); i++) {
			newVerts[vertCount + i] = solid.hullVerts[i].pos;
			newVertIndexes.push_back(vertCount + i);
		}

		replace_lump(LUMP_VERTICES, newVerts, (vertCount + solid.hullVerts.size()) * sizeof(vec3));
	}

	// add new edges (not actually edges - just an indirection layer for the verts)
	// TODO: subdivide >512
	unsigned int startEdge = edgeCount;
	std::map<int, int> vertToSurfedge;
	{
		size_t addEdges = (solid.hullVerts.size() + 1) / 2;

		BSPEDGE* newEdges = new BSPEDGE[edgeCount + addEdges];
		memcpy(newEdges, edges, edgeCount * sizeof(BSPEDGE));

		unsigned int idx = 0;
		for (unsigned int i = 0; i < solid.hullVerts.size(); i += 2) {
			unsigned int v0 = i;
			unsigned int v1 = (i + 1) % solid.hullVerts.size();
			newEdges[startEdge + idx] = BSPEDGE(newVertIndexes[v0], newVertIndexes[v1]);

			vertToSurfedge[v0] = startEdge + idx;
			if (v1 > 0) {
				vertToSurfedge[v1] = -((int)(startEdge + idx)); // negative = use second vert
			}

			idx++;
		}
		replace_lump(LUMP_EDGES, newEdges, (edgeCount + addEdges) * sizeof(BSPEDGE));
	}

	// add new surfedges (2 for each edge)
	unsigned int startSurfedge = surfedgeCount;
	{
		size_t addSurfedges = 0;
		for (size_t i = 0; i < solid.faces.size(); i++) {
			addSurfedges += solid.faces[i].verts.size();
		}

		int* newSurfedges = new int[surfedgeCount + addSurfedges];
		memcpy(newSurfedges, surfedges, surfedgeCount * sizeof(int));

		unsigned int idx = 0;
		for (unsigned int i = 0; i < solid.faces.size(); i++) {
			auto& tmpFace = solid.faces[i];
			for (unsigned int k = 0; k < tmpFace.verts.size(); k++) {
				newSurfedges[startSurfedge + idx++] = vertToSurfedge[tmpFace.verts[k]];
			}
		}

		replace_lump(LUMP_SURFEDGES, newSurfedges, (surfedgeCount + addSurfedges) * sizeof(int));
	}

	// add new planes (1 for each face/node)
	// TODO: reuse existing planes (maybe not until shared stuff can be split when editing solids)
	unsigned int startPlane = planeCount;
	{
		BSPPLANE* newPlanes = new BSPPLANE[planeCount + solid.faces.size()];
		memcpy(newPlanes, planes, planeCount * sizeof(BSPPLANE));

		for (unsigned int i = 0; i < solid.faces.size(); i++) {
			newPlanes[startPlane + i] = solid.faces[i].plane;
		}

		replace_lump(LUMP_PLANES, newPlanes, (planeCount + solid.faces.size()) * sizeof(BSPPLANE));
	}

	// add new faces
	unsigned int startFace = faceCount;
	{
		BSPFACE* newFaces = new BSPFACE[faceCount + solid.faces.size()];
		memcpy(newFaces, faces, faceCount * sizeof(BSPFACE));

		unsigned int surfedgeOffset = 0;
		for (unsigned int i = 0; i < solid.faces.size(); i++) {
			BSPFACE& face = newFaces[faceCount + i];
			face.iFirstEdge = startSurfedge + surfedgeOffset;
			face.iPlane = startPlane + i;
			face.nEdges = (unsigned short)solid.faces[i].verts.size();
			face.nPlaneSide = solid.faces[i].planeSide;
			//face.iTextureInfo = startTexinfo + i;
			face.iTextureInfo = solid.faces[i].iTextureInfo;
			face.nLightmapOffset = 0; // TODO: Lighting
			memset(face.nStyles, 255, 4);

			surfedgeOffset += face.nEdges;
		}

		replace_lump(LUMP_FACES, newFaces, (faceCount + solid.faces.size()) * sizeof(BSPFACE));
	}

	//TODO: move to common function
	short sharedSolidLeaf = 0;
	short anyEmptyLeaf = 0;
	for (unsigned int i = 0; i < leafCount; i++) {
		if (leaves[i].nContents == CONTENTS_EMPTY) {
			anyEmptyLeaf = i;
			break;
		}
	}
	if (anyEmptyLeaf == 0) {
		anyEmptyLeaf = create_leaf(CONTENTS_EMPTY);
		targetModel->nVisLeafs = 1;
	}
	else {
		targetModel->nVisLeafs = 0;
	}

	// add new nodes
	unsigned int startNode = nodeCount;
	{
		BSPNODE* newNodes = new BSPNODE[nodeCount + solid.faces.size() + 1];
		memcpy(newNodes, nodes, nodeCount * sizeof(BSPNODE));

		unsigned int nodeIdx = nodeCount;

		for (int k = 0; k < solid.faces.size(); k++) {
			BSPNODE& node = newNodes[nodeCount + k];
			memset(&node, 0, sizeof(BSPNODE));

			node.firstFace = startFace + k; // face required for decals
			node.nFaces = 1;
			node.iPlane = startPlane + k;
			// node mins/maxs don't matter for submodels. Leave them at 0.

			short insideContents = k == solid.faces.size() - 1 ? ~sharedSolidLeaf : (short)(nodeCount + k + 1);
			short outsideContents = ~anyEmptyLeaf;

			// can't have negative normals on planes so children are swapped instead
			if (solid.faces[k].planeSide) {
				node.iChildren[0] = insideContents;
				node.iChildren[1] = outsideContents;
			}
			else {
				node.iChildren[0] = outsideContents;
				node.iChildren[1] = insideContents;
			}
		}

		replace_lump(LUMP_NODES, newNodes, (nodeCount + solid.faces.size()) * sizeof(BSPNODE));
	}

	targetModel->iHeadnodes[0] = startNode;
	targetModel->iHeadnodes[1] = CONTENTS_EMPTY;
	targetModel->iHeadnodes[2] = CONTENTS_EMPTY;
	targetModel->iHeadnodes[3] = CONTENTS_EMPTY;
	targetModel->iFirstFace = startFace;
	targetModel->nFaces = (int)solid.faces.size();

	targetModel->nMaxs = vec3(FLT_MIN_COORD, FLT_MIN_COORD, FLT_MIN_COORD);
	targetModel->nMins = vec3(FLT_MAX_COORD, FLT_MAX_COORD, FLT_MAX_COORD);
	for (int i = 0; i < solid.hullVerts.size(); i++) {
		vec3 v = verts[startVert + i];
		expandBoundingBox(v, targetModel->nMins, targetModel->nMaxs);
	}
}

int Bsp::create_clipnode_box(const vec3& mins, const vec3& maxs, BSPMODEL* targetModel, int targetHull, bool skipEmpty) {
	std::vector<BSPPLANE> addPlanes;
	std::vector<BSPCLIPNODE> addNodes;
	int solidNodeIdx = 0;

	for (int i = 1; i < MAX_MAP_HULLS; i++) {
		if (skipEmpty && targetModel->iHeadnodes[i] < 0) {
			continue;
		}
		if (targetHull > 0 && i != targetHull) {
			continue;
		}

		vec3 min = mins - default_hull_extents[i];
		vec3 max = maxs + default_hull_extents[i];

		size_t clipnodeIdx = clipnodeCount + addNodes.size();
		size_t planeIdx = planeCount + addPlanes.size();

		addPlanes.push_back({ vec3(1, 0, 0), min.x, PLANE_X }); // left
		addPlanes.push_back({ vec3(1, 0, 0), max.x, PLANE_X }); // right
		addPlanes.push_back({ vec3(0, 1, 0), min.y, PLANE_Y }); // front
		addPlanes.push_back({ vec3(0, 1, 0), max.y, PLANE_Y }); // back
		addPlanes.push_back({ vec3(0, 0, 1), min.z, PLANE_Z }); // bottom
		addPlanes.push_back({ vec3(0, 0, 1), max.z, PLANE_Z }); // top

		targetModel->iHeadnodes[i] = (int)(clipnodeCount + addNodes.size());

		for (int k = 0; k < 6; k++) {
			BSPCLIPNODE node;
			node.iPlane = (int)planeIdx++;

			int insideContents = k == 5 ? CONTENTS_SOLID : (int)(clipnodeIdx + 1);

			if (insideContents == CONTENTS_SOLID)
				solidNodeIdx = (int)clipnodeIdx;

			clipnodeIdx++;

			// can't have negative normals on planes so children are swapped instead
			if (k % 2 == 0) {
				node.iChildren[0] = insideContents;
				node.iChildren[1] = CONTENTS_EMPTY;
			}
			else {
				node.iChildren[0] = CONTENTS_EMPTY;
				node.iChildren[1] = insideContents;
			}

			addNodes.push_back(node);
		}
	}

	BSPPLANE* newPlanes = new BSPPLANE[planeCount + addPlanes.size()];
	memcpy(newPlanes, planes, planeCount * sizeof(BSPPLANE));
	memcpy(newPlanes + planeCount, &addPlanes[0], addPlanes.size() * sizeof(BSPPLANE));
	replace_lump(LUMP_PLANES, newPlanes, (planeCount + addPlanes.size()) * sizeof(BSPPLANE));

	BSPCLIPNODE* newClipnodes = new BSPCLIPNODE[clipnodeCount + addNodes.size()];
	memcpy(newClipnodes, clipnodes, clipnodeCount * sizeof(BSPCLIPNODE));
	memcpy(newClipnodes + clipnodeCount, &addNodes[0], addNodes.size() * sizeof(BSPCLIPNODE));
	replace_lump(LUMP_CLIPNODES, newClipnodes, (clipnodeCount + addNodes.size()) * sizeof(BSPCLIPNODE));

	return solidNodeIdx;
}

void Bsp::simplify_model_collision(int modelIdx, int hullIdx) {
	if (modelIdx < 0 || (unsigned int)modelIdx >= modelCount) {
		logf("Invalid model index %d. Must be 0-%d\n", modelIdx);
		return;
	}
	if (hullIdx >= MAX_MAP_HULLS) {
		logf("Invalid hull number. Valid hull numbers are 1-%d\n", MAX_MAP_HULLS);
		return;
	}

	BSPMODEL& model = models[modelIdx];

	if (model.iHeadnodes[1] < 0 && model.iHeadnodes[2] < 0 && model.iHeadnodes[3] < 0) {
		logf("Model has no clipnode hulls left to simplify\n");
		return;
	}

	if (hullIdx > 0 && model.iHeadnodes[hullIdx] < 0) {
		logf("Hull %d has no clipnodes\n", hullIdx);
		return;
	}

	if (model.iHeadnodes[0] < 0) {
		logf("Hull 0 was deleted from this model. Can't simplify.\n");
		// TODO: create verts from plane intersections
		return;
	}

	vec3 vertMin(FLT_MAX_COORD, FLT_MAX_COORD, FLT_MAX_COORD);
	vec3 vertMax(FLT_MIN_COORD, FLT_MIN_COORD, FLT_MIN_COORD);
	get_model_vertex_bounds(modelIdx, vertMin, vertMax);

	create_clipnode_box(vertMin, vertMax, &model, hullIdx, true);
}

int Bsp::create_clipnode() {
	BSPCLIPNODE* newNodes = new BSPCLIPNODE[clipnodeCount + 1];
	memcpy(newNodes, clipnodes, clipnodeCount * sizeof(BSPCLIPNODE));

	BSPCLIPNODE* newNode = &newNodes[clipnodeCount];
	memset(newNode, 0, sizeof(BSPCLIPNODE));

	replace_lump(LUMP_CLIPNODES, newNodes, (clipnodeCount + 1) * sizeof(BSPCLIPNODE));

	return clipnodeCount - 1;
}

int Bsp::create_plane() {
	BSPPLANE* newPlanes = new BSPPLANE[planeCount + 1];
	memcpy(newPlanes, planes, planeCount * sizeof(BSPPLANE));

	BSPPLANE& newPlane = newPlanes[planeCount];
	memset(&newPlane, 0, sizeof(BSPPLANE));

	replace_lump(LUMP_PLANES, newPlanes, (planeCount + 1) * sizeof(BSPPLANE));

	return planeCount - 1;
}

int Bsp::create_model() {
	BSPMODEL* newModels = new BSPMODEL[modelCount + 1];
	memcpy(newModels, models, modelCount * sizeof(BSPMODEL));

	BSPMODEL& newModel = newModels[modelCount];
	memset(&newModel, 0, sizeof(BSPMODEL));

	int newModelIdx = modelCount;
	replace_lump(LUMP_MODELS, newModels, (modelCount + 1) * sizeof(BSPMODEL));

	return newModelIdx;
}

int Bsp::create_texinfo() {
	BSPTEXTUREINFO* newTexinfos = new BSPTEXTUREINFO[texinfoCount + 1];
	memcpy(newTexinfos, texinfos, texinfoCount * sizeof(BSPTEXTUREINFO));

	BSPTEXTUREINFO& newTexinfo = newTexinfos[texinfoCount];
	memset(&newTexinfo, 0, sizeof(BSPTEXTUREINFO));

	replace_lump(LUMP_TEXINFO, newTexinfos, (texinfoCount + 1) * sizeof(BSPTEXTUREINFO));

	return texinfoCount - 1;
}

int Bsp::duplicate_model(int modelIdx) {
	STRUCTUSAGE usage(this);
	mark_model_structures(modelIdx, &usage, true);

	STRUCTREMAP remap(this);

	std::vector<BSPPLANE> newPlanes;
	for (unsigned int i = 0; i < usage.count.planes; i++) {
		if (usage.planes[i]) {
			remap.planes[i] = (int)(planeCount + newPlanes.size());
			newPlanes.push_back(planes[i]);
		}
	}

	std::vector<vec3> newVerts;
	for (unsigned int i = 0; i < usage.count.verts; i++) {
		if (usage.verts[i]) {
			remap.verts[i] = (int)(vertCount + newVerts.size());
			newVerts.push_back(verts[i]);
		}
	}

	std::vector<BSPEDGE> newEdges;
	for (unsigned int i = 0; i < usage.count.edges; i++) {
		if (usage.edges[i]) {
			remap.edges[i] = (int)(edgeCount + newEdges.size());

			BSPEDGE edge = edges[i];
			for (int k = 0; k < 2; k++)
				edge.iVertex[k] = remap.verts[edge.iVertex[k]];
			newEdges.push_back(edge);
		}
	}

	std::vector<int> newSurfedges;
	for (unsigned int i = 0; i < usage.count.surfEdges; i++) {
		if (usage.surfEdges[i]) {
			remap.surfEdges[i] = (int)(surfedgeCount + newSurfedges.size());

			int surfedge = remap.edges[abs(surfedges[i])];
			if (surfedges[i] < 0)
				surfedge = -surfedge;
			newSurfedges.push_back(surfedge);
		}
	}

	std::vector<BSPTEXTUREINFO> newTexinfo;
	for (unsigned int i = 0; i < usage.count.texInfos; i++) {
		if (usage.texInfo[i]) {
			remap.texInfo[i] = (int)(texinfoCount + newTexinfo.size());
			newTexinfo.push_back(texinfos[i]);
		}
	}

	std::vector<BSPFACE> newFaces;
	std::vector<COLOR3> newLightmaps;
	int lightmapAppendSz = 0;
	for (unsigned int i = 0; i < usage.count.faces; i++) {
		if (usage.faces[i]) {
			remap.faces[i] = (int)(faceCount + newFaces.size());

			BSPFACE face = faces[i];
			face.iFirstEdge = remap.surfEdges[face.iFirstEdge];
			face.iPlane = remap.planes[face.iPlane];
			face.iTextureInfo = remap.texInfo[face.iTextureInfo];

			// TODO: Check if face even has lighting
			int size[2];
			GetFaceLightmapSize(this, i, size);
			int lightmapCount = lightmap_count(i);
			int lightmapSz = size[0] * size[1] * lightmapCount;
			COLOR3* lightmapSrc = (COLOR3*)(lightdata + face.nLightmapOffset);
			for (int k = 0; k < lightmapSz; k++) {
				newLightmaps.push_back(lightmapSrc[k]);
			}

			face.nLightmapOffset = lightmapCount != 0 ? lightDataLength + lightmapAppendSz : -1;
			newFaces.push_back(face);

			lightmapAppendSz += lightmapSz * sizeof(COLOR3);
		}
	}

	std::vector<BSPNODE> newNodes;
	for (unsigned int i = 0; i < usage.count.nodes; i++) {
		if (usage.nodes[i]) {
			remap.nodes[i] = (int)(nodeCount + newNodes.size());
			newNodes.push_back(nodes[i]);
		}
	}
	for (size_t i = 0; i < newNodes.size(); i++) {
		BSPNODE& node = newNodes[i];
		node.firstFace = remap.faces[node.firstFace];
		node.iPlane = remap.planes[node.iPlane];

		for (int k = 0; k < 2; k++) {
			if (node.iChildren[k] > 0) {
				node.iChildren[k] = remap.nodes[node.iChildren[k]];
			}
		}
	}

	std::vector<BSPCLIPNODE> newClipnodes;
	for (unsigned int i = 0; i < usage.count.clipnodes; i++) {
		if (usage.clipnodes[i]) {
			remap.clipnodes[i] = (int)(clipnodeCount + newClipnodes.size());
			newClipnodes.push_back(clipnodes[i]);
		}
	}
	for (size_t i = 0; i < newClipnodes.size(); i++) {
		BSPCLIPNODE& clipnode = newClipnodes[i];
		clipnode.iPlane = remap.planes[clipnode.iPlane];

		for (int k = 0; k < 2; k++) {
			if (clipnode.iChildren[k] > 0) {
				clipnode.iChildren[k] = remap.clipnodes[clipnode.iChildren[k]];
			}
		}
	}

	// MAYBE TODO: duplicate leaves(?) + marksurfs + recacl vis + update undo command lumps

	if (newClipnodes.size())
		append_lump(LUMP_CLIPNODES, &newClipnodes[0], sizeof(BSPCLIPNODE) * newClipnodes.size());
	if (newEdges.size())
		append_lump(LUMP_EDGES, &newEdges[0], sizeof(BSPEDGE) * newEdges.size());
	if (newFaces.size())
		append_lump(LUMP_FACES, &newFaces[0], sizeof(BSPFACE) * newFaces.size());
	if (newNodes.size())
		append_lump(LUMP_NODES, &newNodes[0], sizeof(BSPNODE) * newNodes.size());
	if (newPlanes.size())
		append_lump(LUMP_PLANES, &newPlanes[0], sizeof(BSPPLANE) * newPlanes.size());
	if (newSurfedges.size())
		append_lump(LUMP_SURFEDGES, &newSurfedges[0], sizeof(int) * newSurfedges.size());
	if (newTexinfo.size())
		append_lump(LUMP_TEXINFO, &newTexinfo[0], sizeof(BSPTEXTUREINFO) * newTexinfo.size());
	if (newVerts.size())
		append_lump(LUMP_VERTICES, &newVerts[0], sizeof(vec3) * newVerts.size());
	if (newLightmaps.size())
		append_lump(LUMP_LIGHTING, &newLightmaps[0], sizeof(COLOR3) * newLightmaps.size());

	int newModelIdx = create_model();
	BSPMODEL& oldModel = models[modelIdx];
	BSPMODEL& newModel = models[newModelIdx];
	memcpy(&newModel, &oldModel, sizeof(BSPMODEL));

	newModel.iFirstFace = remap.faces[oldModel.iFirstFace];
	newModel.iHeadnodes[0] = oldModel.iHeadnodes[0] < 0 ? -1 : remap.nodes[oldModel.iHeadnodes[0]];
	for (int i = 1; i < MAX_MAP_HULLS; i++) {
		newModel.iHeadnodes[i] = oldModel.iHeadnodes[i] < 0 ? -1 : remap.clipnodes[oldModel.iHeadnodes[i]];
	}
	newModel.nVisLeafs = 0; // techinically should match the old model, but leaves aren't duplicated yet

	return newModelIdx;
}

BSPTEXTUREINFO* Bsp::get_unique_texinfo(int faceIdx) {
	BSPFACE& targetFace = faces[faceIdx];
	int targetInfo = targetFace.iTextureInfo;

	for (unsigned int i = 0; i < faceCount; i++) {
		if (i != faceIdx && faces[i].iTextureInfo == targetFace.iTextureInfo) {
			int newInfo = create_texinfo();
			texinfos[newInfo] = texinfos[targetInfo];
			targetInfo = newInfo;
			targetFace.iTextureInfo = newInfo;
			debugf("Create new texinfo\n");
			break;
		}
	}

	return &texinfos[targetInfo];
}

int Bsp::get_model_from_face(int faceIdx) {
	for (unsigned int i = 0; i < modelCount; i++) {
		BSPMODEL& model = models[i];
		if (isModelHasFaceIdx(model, faceIdx))
		{
			return i;
		}
	}
	return -1;
}

short Bsp::regenerate_clipnodes_from_nodes(int iNode, int hullIdx) {
	BSPNODE& node = nodes[iNode];

	switch (planes[node.iPlane].nType) {
	case PLANE_X: case PLANE_Y: case PLANE_Z:
	{
		// Skip this node. Bounding box clipnodes should have already been generated.
		// Only works for convex models.
		int childContents[2] = { 0, 0 };
		for (int i = 0; i < 2; i++) {
			if (node.iChildren[i] < 0) {
				BSPLEAF& leaf = leaves[~node.iChildren[i]];
				childContents[i] = leaf.nContents;
			}
		}

		int solidChild = childContents[0] == CONTENTS_EMPTY ? node.iChildren[1] : node.iChildren[0];
		int solidContents = childContents[0] == CONTENTS_EMPTY ? childContents[1] : childContents[0];

		if (solidChild < 0) {
			if (solidContents != CONTENTS_SOLID) {
				logf("UNEXPECTED SOLID CONTENTS %d\n", solidContents);
			}
			return CONTENTS_SOLID; // solid leaf
		}
		return regenerate_clipnodes_from_nodes(solidChild, hullIdx);
	}
	default:
		break;
	}

	int oldCount = clipnodeCount;
	int newClipnodeIdx = create_clipnode();
	clipnodes[newClipnodeIdx].iPlane = create_plane();

	int solidChild = -1;
	for (int i = 0; i < 2; i++) {
		if (node.iChildren[i] >= 0) {
			int childIdx = regenerate_clipnodes_from_nodes(node.iChildren[i], hullIdx);
			clipnodes[newClipnodeIdx].iChildren[i] = childIdx;
			solidChild = solidChild == -1 ? i : -1;
		}
		else {
			BSPLEAF& leaf = leaves[~node.iChildren[i]];
			clipnodes[newClipnodeIdx].iChildren[i] = leaf.nContents;
			if (leaf.nContents == CONTENTS_SOLID) {
				solidChild = i;
			}
		}
	}

	BSPPLANE& nodePlane = planes[node.iPlane];
	BSPPLANE& clipnodePlane = planes[clipnodes[newClipnodeIdx].iPlane];
	clipnodePlane = nodePlane;

	// TODO: pretty sure this isn't right. Angled stuff probably lerps between the hull dimensions
	float extent = 0;
	switch (clipnodePlane.nType) {
	case PLANE_X: case PLANE_ANYX: extent = default_hull_extents[hullIdx].x; break;
	case PLANE_Y: case PLANE_ANYY: extent = default_hull_extents[hullIdx].y; break;
	case PLANE_Z: case PLANE_ANYZ: extent = default_hull_extents[hullIdx].z; break;
	}

	// TODO: this won't work for concave solids. The node's face could be used to determine which
	// direction the plane should be extended but not all nodes will have faces. Also wouldn't be
	// enough to "link" clipnode planes to node planes during scaling because BSP trees might not match.
	if (solidChild != -1) {
		BSPPLANE& p = planes[clipnodes[newClipnodeIdx].iPlane];
		vec3 planePoint = p.vNormal * p.fDist;
		vec3 newPlanePoint = planePoint + p.vNormal * (solidChild == 0 ? -extent : extent);
		p.fDist = dotProduct(p.vNormal, newPlanePoint) / dotProduct(p.vNormal, p.vNormal);
	}

	return newClipnodeIdx;
}

void Bsp::regenerate_clipnodes(int modelIdx, int hullIdx) {
	BSPMODEL& model = models[modelIdx];

	for (int i = 1; i < MAX_MAP_HULLS; i++) {
		if (hullIdx >= 0 && hullIdx != i)
			continue;

		// first create a bounding box for the model. For some reason this is needed to prevent
		// planes from extended farther than they should. All clip types do this.
		int solidNodeIdx = create_clipnode_box(model.nMins, model.nMaxs, &model, i, false); // fills in the headnode

		for (int k = 0; k < 2; k++) {
			if (clipnodes[solidNodeIdx].iChildren[k] == CONTENTS_SOLID) {
				clipnodes[solidNodeIdx].iChildren[k] = regenerate_clipnodes_from_nodes(model.iHeadnodes[0], i);
			}
		}

		// TODO: create clipnodes to "cap" edges that are 90+ degrees (most CSG clip types do this)
		// that will fix broken collision around those edges (invisible solid areas)
	}
}

void Bsp::dump_lightmap(int faceIdx, const std::string& outputPath) {
	BSPFACE& face = faces[faceIdx];

	int mins[2];
	int extents[2];
	GetFaceExtents(this, faceIdx, mins, extents);

	int lightmapSz = extents[0] * extents[1];

	lodepng_encode24_file(outputPath.c_str(), (unsigned char*)lightdata + face.nLightmapOffset, extents[0], extents[1]);
}

void Bsp::dump_lightmap_atlas(const std::string& outputPath) {
	int lightmapWidth = MAX_SURFACE_EXTENT;

	int lightmapsPerDim = (int)ceil(sqrt(faceCount));
	int atlasDim = lightmapsPerDim * lightmapWidth;
	int sz = atlasDim * atlasDim;
	logf("ATLAS SIZE %d x %d (%.2f KB)", lightmapsPerDim, lightmapsPerDim, (sz * sizeof(COLOR3)) / 1024.0f);

	COLOR3* pngData = new COLOR3[sz];

	memset(pngData, 0, sz * sizeof(COLOR3));

	for (unsigned int i = 0; i < faceCount; i++) {
		BSPFACE& face = faces[i];

		if (face.nStyles[0] == 255)
			continue; // no lighting info

		int atlasX = (i % lightmapsPerDim) * lightmapWidth;
		int atlasY = (i / lightmapsPerDim) * lightmapWidth;

		int size[2];
		GetFaceLightmapSize(this, i, size);

		int lightmapWidth = size[0];
		int lightmapHeight = size[1];

		for (int y = 0; y < lightmapHeight; y++) {
			for (int x = 0; x < lightmapWidth; x++) {
				int dstX = atlasX + x;
				int dstY = atlasY + y;

				int lightmapOffset = (y * lightmapWidth + x) * sizeof(COLOR3);

				COLOR3* src = (COLOR3*)(lightdata + face.nLightmapOffset + lightmapOffset);

				pngData[dstY * atlasDim + dstX] = *src;
			}
		}
	}

	lodepng_encode24_file(outputPath.c_str(), (unsigned char*)pngData, atlasDim, atlasDim);
}

void Bsp::write_csg_outputs(std::string path) {
	BSPPLANE* thisPlanes = (BSPPLANE*)lumps[LUMP_PLANES];
	int numPlanes = bsp_header.lump[LUMP_PLANES].nLength / sizeof(BSPPLANE);

	// add flipped version of planes since face output files can't specify plane side
	BSPPLANE* newPlanes = new BSPPLANE[numPlanes * 2];
	memcpy(newPlanes, thisPlanes, numPlanes * sizeof(BSPPLANE));
	for (int i = 0; i < numPlanes; i++) {
		BSPPLANE flipped = thisPlanes[i];
		flipped.vNormal = { flipped.vNormal.x > 0 ? -flipped.vNormal.x : flipped.vNormal.x,
							flipped.vNormal.y > 0 ? -flipped.vNormal.y : flipped.vNormal.y,
							flipped.vNormal.z > 0 ? -flipped.vNormal.z : flipped.vNormal.z, };
		flipped.fDist = -flipped.fDist;
		newPlanes[numPlanes + i] = flipped;
	}
	delete[] lumps[LUMP_PLANES];
	lumps[LUMP_PLANES] = (unsigned char*)newPlanes;
	numPlanes *= 2;
	bsp_header.lump[LUMP_PLANES].nLength = numPlanes * sizeof(BSPPLANE);
	thisPlanes = newPlanes;

	std::ofstream pln_file(path + bsp_name + ".pln", std::ios::trunc | std::ios::binary);
	for (int i = 0; i < numPlanes; i++) {
		BSPPLANE& p = thisPlanes[i];
		CSGPLANE csgplane = {
			{p.vNormal.x, p.vNormal.y, p.vNormal.z},
			{0,0,0},
			p.fDist,
			p.nType
		};
		pln_file.write((char*)&csgplane, sizeof(CSGPLANE));
	}
	logf("Wrote %d planes\n", numPlanes);

	BSPFACE* thisFaces = (BSPFACE*)lumps[LUMP_FACES];
	int thisFaceCount = bsp_header.lump[LUMP_FACES].nLength / sizeof(BSPFACE);

	BSPMODEL* tmodels = (BSPMODEL*)lumps[LUMP_MODELS];
	BSPMODEL world = tmodels[0];

	for (int i = 0; i < 4; i++) {

		FILE* polyfile = NULL;
		fopen_s(&polyfile, (path + bsp_name + ".p" + std::to_string(i)).c_str(), "wb");
		if (polyfile)
		{
			write_csg_polys(world.iHeadnodes[i], polyfile, numPlanes / 2, i == 0);
			fprintf(polyfile, "-1 -1 -1 -1 -1\n"); // end of file marker (parsing fails without this)
			fclose(polyfile);
		}

		FILE* detailfile = NULL;
		fopen_s(&detailfile, (path + bsp_name + ".b" + std::to_string(i)).c_str(), "wb");
		if (detailfile)
		{
			fprintf(detailfile, "-1\n");
			fclose(detailfile);
		}
	}

	std::ofstream hsz_file(path + bsp_name + ".hsz", std::ios::trunc | std::ios::binary);
	const char* hullSizes = "0 0 0 0 0 0\n"
		"-16 -16 -36 16 16 36\n"
		"-32 -32 -32 32 32 32\n"
		"-16 -16 -18 16 16 18\n";
	hsz_file.write(hullSizes, strlen(hullSizes));

	std::ofstream bsp_file(path + bsp_name + "_new.bsp", std::ios::trunc | std::ios::binary);
	// calculate lump offsets
	int offset = sizeof(BSPHEADER);
	for (int i = 0; i < HEADER_LUMPS; i++) {
		bsp_header.lump[i].nOffset = offset;
		if (i == LUMP_ENTITIES || i == LUMP_PLANES || i == LUMP_TEXTURES || i == LUMP_TEXINFO) {
			offset += bsp_header.lump[i].nLength;
			if (i == LUMP_PLANES) {
				int count = bsp_header.lump[i].nLength / sizeof(BSPPLANE);
				printf("BSP HAS %d PLANES\n", count);
			}
		}
		else {
			bsp_header.lump[i].nLength = 0;
		}
	}
	bsp_file.write((char*)&bsp_header, sizeof(BSPHEADER));
	// write the lumps
	for (int i = 0; i < HEADER_LUMPS; i++) {
		bsp_file.write((char*)lumps[i], bsp_header.lump[i].nLength);
	}
}

void Bsp::write_csg_polys(short nodeIdx, FILE* polyfile, int flipPlaneSkip, bool debug) {
	if (nodeIdx >= 0) {
		write_csg_polys(nodes[nodeIdx].iChildren[0], polyfile, flipPlaneSkip, debug);
		write_csg_polys(nodes[nodeIdx].iChildren[1], polyfile, flipPlaneSkip, debug);
		return;
	}

	BSPLEAF& leaf = leaves[~nodeIdx];

	int detaillevel = 0; // no way to know which faces came from a func_detail
	int contents = leaf.nContents;

	for (int i = leaf.iFirstMarkSurface; i < leaf.iFirstMarkSurface + leaf.nMarkSurfaces; i++) {
		for (int z = 0; z < 2; z++) {
			BSPFACE& face = faces[marksurfs[i]];

			bool flipped = (z == 1 || face.nPlaneSide) && !(z == 1 && face.nPlaneSide);

			int iPlane = !flipped ? face.iPlane : face.iPlane + flipPlaneSkip;

			// FIXME : z always == 1
			// contents in front of the face
			int faceContents = z == 1 ? leaf.nContents : CONTENTS_SOLID;

			//int texInfo = z == 1 ? face.iTextureInfo : -1;

			if (debug) {
				BSPPLANE plane = planes[iPlane];
				logf("Writing face (%2.0f %2.0f %2.0f) %4.0f  %s\n",
					plane.vNormal.x, plane.vNormal.y, plane.vNormal.z, plane.fDist,
					(faceContents == CONTENTS_SOLID ? "SOLID" : "EMPTY"));
				if (flipped && false) {
					logf(" (flipped)");
				}
			}

			fprintf(polyfile, "%i %i %u %i %u\n", detaillevel, iPlane, face.iTextureInfo, faceContents, face.nEdges);

			if (flipped) {
				for (int e = (int)(face.iFirstEdge + face.nEdges) - 1; e >= (int)face.iFirstEdge; e--) {
					int edgeIdx = surfedges[e];
					BSPEDGE& edge = edges[abs(edgeIdx)];
					vec3 v = edgeIdx >= 0 ? verts[edge.iVertex[1]] : verts[edge.iVertex[0]];
					fprintf(polyfile, "%5.8f %5.8f %5.8f\n", v.x, v.y, v.z);
				}
			}
			else {
				for (unsigned int e = face.iFirstEdge; e < face.iFirstEdge + face.nEdges; e++) {
					int edgeIdx = surfedges[e];
					BSPEDGE& edge = edges[abs(edgeIdx)];
					vec3 v = edgeIdx >= 0 ? verts[edge.iVertex[1]] : verts[edge.iVertex[0]];
					fprintf(polyfile, "%5.8f %5.8f %5.8f\n", v.x, v.y, v.z);
				}
			}

			fprintf(polyfile, "\n");
		}
		if (debug)
			logf("\n");
	}
}

void Bsp::print_leaf(const BSPLEAF& leaf) {
	logf(getLeafContentsName(leaf.nContents));
	logf(" %d surfs, Min(%d, %d, %d), Max(%d %d %d)", leaf.nMarkSurfaces,
		leaf.nMins[0], leaf.nMins[1], leaf.nMins[2],
		leaf.nMaxs[0], leaf.nMaxs[1], leaf.nMaxs[2]);
}

void Bsp::update_lump_pointers() {
	planes = (BSPPLANE*)lumps[LUMP_PLANES];
	texinfos = (BSPTEXTUREINFO*)lumps[LUMP_TEXINFO];
	leaves = (BSPLEAF*)lumps[LUMP_LEAVES];
	models = (BSPMODEL*)lumps[LUMP_MODELS];
	nodes = (BSPNODE*)lumps[LUMP_NODES];
	clipnodes = (BSPCLIPNODE*)lumps[LUMP_CLIPNODES];
	faces = (BSPFACE*)lumps[LUMP_FACES];
	verts = (vec3*)lumps[LUMP_VERTICES];
	lightdata = lumps[LUMP_LIGHTING];
	surfedges = (int*)lumps[LUMP_SURFEDGES];
	edges = (BSPEDGE*)lumps[LUMP_EDGES];
	marksurfs = (unsigned short*)lumps[LUMP_MARKSURFACES];
	visdata = lumps[LUMP_VISIBILITY];
	textures = lumps[LUMP_TEXTURES];

	planeCount = bsp_header.lump[LUMP_PLANES].nLength / sizeof(BSPPLANE);
	texinfoCount = bsp_header.lump[LUMP_TEXINFO].nLength / sizeof(BSPTEXTUREINFO);
	leafCount = bsp_header.lump[LUMP_LEAVES].nLength / sizeof(BSPLEAF);
	modelCount = bsp_header.lump[LUMP_MODELS].nLength / sizeof(BSPMODEL);
	nodeCount = bsp_header.lump[LUMP_NODES].nLength / sizeof(BSPNODE);
	vertCount = bsp_header.lump[LUMP_VERTICES].nLength / sizeof(vec3);
	faceCount = bsp_header.lump[LUMP_FACES].nLength / sizeof(BSPFACE);
	clipnodeCount = bsp_header.lump[LUMP_CLIPNODES].nLength / sizeof(BSPCLIPNODE);
	marksurfCount = bsp_header.lump[LUMP_MARKSURFACES].nLength / sizeof(unsigned short);
	surfedgeCount = bsp_header.lump[LUMP_SURFEDGES].nLength / sizeof(int);
	edgeCount = bsp_header.lump[LUMP_EDGES].nLength / sizeof(BSPEDGE);
	textureCount = *((int*)(textures));
	lightDataLength = bsp_header.lump[LUMP_LIGHTING].nLength;
	visDataLength = bsp_header.lump[LUMP_VISIBILITY].nLength;

	if (planeCount > MAX_MAP_PLANES) logf("Overflowed Planes !!!\n");
	if (texinfoCount > MAX_MAP_TEXINFOS) logf("Overflowed texinfos !!!\n");
	if (leafCount > MAX_MAP_LEAVES) logf("Overflowed leaves !!!\n");
	if (modelCount > MAX_MAP_MODELS) logf("Overflowed models !!!\n");
	if (texinfoCount > MAX_MAP_TEXINFOS) logf("Overflowed texinfos !!!\n");
	if (nodeCount > MAX_MAP_NODES) logf("Overflowed nodes !!!\n");
	if (vertCount > MAX_MAP_VERTS) logf("Overflowed verts !!!\n");
	if (faceCount > MAX_MAP_FACES) logf("Overflowed faces !!!\n");
	if (clipnodeCount > MAX_MAP_CLIPNODES) logf("Overflowed clipnodes !!!\n");
	if (marksurfCount > MAX_MAP_MARKSURFS) logf("Overflowed marksurfs !!!\n");
	if (surfedgeCount > MAX_MAP_SURFEDGES) logf("Overflowed surfedges !!!\n");
	if (edgeCount > MAX_MAP_EDGES) logf("Overflowed edges !!!\n");
	if (textureCount > MAX_MAP_TEXTURES) logf("Overflowed textures !!!\n");
	if (lightDataLength > MAX_MAP_LIGHTDATA) logf("Overflowed lightdata !!!\n");
	if (visDataLength > MAX_MAP_VISDATA) logf("Overflowed visdata !!!\n");
}

void Bsp::replace_lump(int lumpIdx, void* newData, size_t newLength) {
	delete[] lumps[lumpIdx];
	lumps[lumpIdx] = (unsigned char*)newData;
	bsp_header.lump[lumpIdx].nLength = (int)newLength;
	update_lump_pointers();
}

void Bsp::append_lump(int lumpIdx, void* newData, size_t appendLength) {
	int oldLen = bsp_header.lump[lumpIdx].nLength;
	unsigned char* newLump = new unsigned char[oldLen + appendLength];

	memcpy(newLump, lumps[lumpIdx], oldLen);
	memcpy(newLump + oldLen, newData, appendLength);

	replace_lump(lumpIdx, newLump, oldLen + appendLength);
}

bool Bsp::isModelHasFaceIdx(const BSPMODEL& mdl, int faceid)
{
	if (faceid < mdl.iFirstFace)
		return false;
	if (faceid >= mdl.iFirstFace + mdl.nFaces)
		return false;
	return true;
}

void Bsp::ExportToObjWIP(std::string path, ExportObjOrder order, int iscale)
{
	if (!createDir(path))
	{
		logf("Error output path directory \"%s\" can't be created!\n", path.c_str());
		return;
	}

	float scale = iscale < 0 ? 1.0f / iscale : 1.0f * iscale;

	if (iscale == 1)
		scale = 1.0f;

	scale = abs(scale);

	FILE* f = NULL;
	logf("Export %s to %s\n", (bsp_name + ".obj").c_str(), path.c_str());
	logf("With %s x%i", iscale == 1 ? "scale" : iscale < 0 ? "downscale" : "upscale", abs(iscale));
	fopen_s(&f, (path + bsp_name + ".obj").c_str(), "wb");
	if (f)
	{
		fprintf(f, "# Exported using bspguy!\n");

		fprintf(f, "s off\n");

		fprintf(f, "mtllib materials.mtl\n");

		std::string groupname = std::string();

		//std::set<BSPMIPTEX*> texture_list;
		BspRenderer* bsprend = getBspRender();

		bsprend->reload();

		createDir(path + "textures");
		std::vector<std::string> materials;
		std::vector<std::string> matnames;

		int vertoffset = 1;

		int materialid = 0;
		int lastmaterialid = -1;

		for (unsigned int i = 0; i < faceCount; i++)
		{
			RenderFace* rface;
			RenderGroup* rgroup;
			if (!bsprend->getRenderPointers(i, &rface, &rgroup)) {
				logf("Bad face index\n");
				break;
			}

			BSPFACE& face = faces[i];
			BSPTEXTUREINFO& texinfo = texinfos[face.iTextureInfo];
			int texOffset = ((int*)textures)[texinfo.iMiptex + 1];
			BSPMIPTEX* tex = ((BSPMIPTEX*)(textures + texOffset));

			int mdlid = get_model_from_face(i);
			std::vector<int> entIds = get_model_ents_ids(mdlid);

			if (entIds.empty())
			{
				entIds.push_back(0);
			}

			materialid = -1;
			for (int m = 0; m < matnames.size(); m++)
			{
				if (matnames[m] == tex->szName)
					materialid = m;
			}
			if (materialid == -1)
			{
				materialid = (int)matnames.size();
				materials.emplace_back("newmtl textures" + std::to_string(materialid));
				if (toLowerCase(tex->szName) == "aaatrigger" ||
					toLowerCase(tex->szName) == "null" ||
					toLowerCase(tex->szName) == "sky" ||
					toLowerCase(tex->szName) == "noclip" ||
					toLowerCase(tex->szName) == "clip" ||
					toLowerCase(tex->szName) == "origin" ||
					toLowerCase(tex->szName) == "bevel" ||
					toLowerCase(tex->szName) == "hint" ||
					toLowerCase(tex->szName) == "uhh"
					)
				{
					materials.push_back("illum 4");
					materials.push_back("map_Kd " + std::string("textures/") + tex->szName + std::string(".bmp"));
					materials.push_back("map_d " + std::string("textures/") + tex->szName + std::string(".bmp"));
				}
				else
				{
					materials.push_back("map_Kd " + std::string("textures/") + tex->szName + std::string(".bmp"));
				}
				materials.push_back("");
				matnames.push_back(tex->szName);
			}

			if (!fileExists(path + std::string("textures/") + tex->szName + std::string(".bmp")))
			{
				if (tex->nOffsets[0] > 0)
				{
					WADTEX wadTex = tex;
					int lastMipSize = (wadTex.nWidth / 8) * (wadTex.nHeight / 8);

					COLOR3* palette = (COLOR3*)(wadTex.data + wadTex.nOffsets[3] + lastMipSize + 2 - 40);
					unsigned char* src = wadTex.data;

					COLOR3* imageData = new COLOR3[wadTex.nWidth * wadTex.nHeight];

					int sz = wadTex.nWidth * wadTex.nHeight;

					for (int k = 0; k < sz; k++) {
						imageData[k] = palette[src[k]];
						std::swap(imageData[k].b, imageData[k].r);
					}
					//tga_write((path + tex->szName + std::string(".obj")).c_str(), tex->nWidth, tex->nWidth, (unsigned char*)tex + tex->nOffsets[0], 3, 3);
					WriteBMP(path + std::string("textures/") + tex->szName + std::string(".bmp"), (unsigned char*)imageData, wadTex.nWidth, wadTex.nHeight, 3);
				}
				else
				{
					bool foundInWad = false;
					for (int r = 0; r < g_app->mapRenderers.size() && !foundInWad; r++)
					{
						Renderer* rend = g_app;
						for (int k = 0; k < rend->mapRenderers[r]->wads.size(); k++) {
							if (rend->mapRenderers[r]->wads[k]->hasTexture(tex->szName)) {
								foundInWad = true;

								WADTEX* wadTex = rend->mapRenderers[r]->wads[k]->readTexture(tex->szName);
								int lastMipSize = (wadTex->nWidth / 8) * (wadTex->nHeight / 8);
								COLOR3* palette = (COLOR3*)(wadTex->data + wadTex->nOffsets[3] + lastMipSize + 2 - 40);
								unsigned char* src = wadTex->data;
								COLOR3* imageData = new COLOR3[wadTex->nWidth * wadTex->nHeight];

								int sz = wadTex->nWidth * wadTex->nHeight;

								for (int m = 0; m < sz; m++) {
									imageData[m] = palette[src[m]];
									std::swap(imageData[m].b, imageData[m].r);
								}

								WriteBMP(path + std::string("textures/") + tex->szName + std::string(".bmp"), (unsigned char*)imageData, wadTex->nWidth, wadTex->nHeight, 3);

								delete[] imageData;
								delete wadTex;
								break;
							}
						}
					}
				}
			}


			for (int e = 0; e < entIds.size(); e++)
			{
				int entid = entIds[e];
				Entity* ent = ents[entid];
				vec3 origin_offset = ent->getOrigin();

				if ("Model_" + std::to_string(mdlid) + "_ent_" + std::to_string(entid) != groupname)
				{
					groupname = "Model_" + std::to_string(mdlid) + "_ent_" + std::to_string(entid);
					fprintf(f, "\no %s\n\n", groupname.c_str());
					fprintf(f, "\ng %s\n\n", groupname.c_str());
				}
				else
				{
					fprintf(f, "\n\n");
				}

				if (lastmaterialid != materialid)
					fprintf(f, "usemtl textures%i\n", materialid);

				lastmaterialid = materialid;

				for (int n = 0; n < rface->vertCount; n++)
				{
					BSPPLANE& plane = planes[face.iPlane];
					lightmapVert& vert = rgroup->verts[rface->vertOffset + n];
					vec3 org_pos = vec3(vert.x + origin_offset.x, vert.y + origin_offset.z, vert.z + -origin_offset.y);

					fprintf(f, "v %f %f %f\n", org_pos.x * scale, org_pos.y * scale, org_pos.z * scale);
				}
				for (int n = 0; n < rface->vertCount; n++)
				{
					lightmapVert& vert = rgroup->verts[rface->vertOffset + n];
					vec3 org_pos = vec3(vert.x + origin_offset.x, vert.y + origin_offset.z, vert.z + -origin_offset.y);
					vec3 pos = vec3(org_pos.x, -org_pos.z, -org_pos.y);

					float tw = 1.0f / (float)tex->nWidth;
					float th = 1.0f / (float)tex->nHeight;
					float fU = dotProduct(texinfo.vS, pos) + texinfo.shiftS;
					float fV = dotProduct(texinfo.vT, pos) + texinfo.shiftT;

					BSPPLANE& plane = planes[face.iPlane];

					fprintf(f, "vt %f %f\n", fU * tw, fV * th);
				}
				for (int n = 0; n < rface->vertCount; n++)
				{
					lightmapVert& vert = rgroup->verts[rface->vertOffset + n];
					BSPPLANE& plane = planes[face.iPlane];

					fprintf(f, "vn %f %f %f\n", plane.vNormal.x, plane.vNormal.z, plane.vNormal.y);
				}

				fprintf(f, "%s", "\nf");
				for (int n = 0; n < rface->vertCount; n++)
				{
					int id = vertoffset + n;

					fprintf(f, " %i/%i/%i", id, id, id);
				}

				vertoffset += rface->vertCount;
				fprintf(f, "%s", "\n");

			}
		}

		FILE* fmat = NULL;
		fopen_s(&fmat, (path + "materials.mtl").c_str(), "wt");

		if (fmat)
		{
			for (auto const& s : materials)
			{
				fprintf(fmat, "%s\n", s.c_str());
			}
			fclose(fmat);
		}

		fclose(f);
	}
	else
	{
		logf("Error file access!'n");
	}
}

struct ENTDATA
{
	int entid;
	std::vector<std::string> vecdata;
};

void Bsp::ExportToMapWIP(std::string path)
{
	if (!createDir(path))
	{
		logf("Error output path directory \"%s\" can't be created!\n", path.c_str());
		return;
	}
	FILE* f = NULL;
	logf("Export %s to %s\n", (bsp_name + ".map").c_str(), path.c_str());
	fopen_s(&f, (path + bsp_name + ".map").c_str(), "wb");
	if (f)
	{
		fprintf(f, "// Exported using bspguy!\n");

		BspRenderer* bsprend = getBspRender();

		bsprend->reload();

		createDir(path + "textures");

		for (unsigned int entIdx = 0; entIdx < ents.size(); entIdx++)
		{
			int modelIdx = entIdx == 0 ? 0 : bsprend->renderEnts[entIdx].modelIdx;
			if (modelIdx < 0 || modelIdx > bsprend->numRenderModels)
				continue;

			for (unsigned int i = 0; i < bsprend->renderModels[modelIdx].groupCount; i++)
			{
				logf("Export ent %u model %u group %d\n", entIdx, modelIdx, i);
				//RenderGroup& rgroup = bsprend->renderModels[modelIdx].renderGroups[i];

			}
		}
		fclose(f);
	}
	else
	{
		logf("Error file access!'n");
	}
}

BspRenderer* Bsp::getBspRender()
{
	if (!renderer)
		for (int i = 0; i < g_app->mapRenderers.size(); i++)
			if (g_app->mapRenderers[i]->map == this)
				renderer = g_app->mapRenderers[i];
	return renderer;
}