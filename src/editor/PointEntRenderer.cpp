#include "lang.h"
#include "PointEntRenderer.h"
#include "primitives.h"
#include "Renderer.h"
#include "Settings.h"
#include "log.h"

PointEntRenderer::PointEntRenderer(Fgd* fgd)
{
	this->fgd = fgd;
	defaultCubeGen = false;
	genPointEntCubes();
}

PointEntRenderer::~PointEntRenderer()
{
	for (size_t i = 0; i < entCubes.size(); i++)
	{
		delete entCubes[i]->axesBuffer;
		delete entCubes[i]->cubeBuffer;
		delete entCubes[i]->selectBuffer;
		delete entCubes[i]->wireframeBuffer;
	}
}

EntCube* PointEntRenderer::getEntCube(Entity* ent)
{
	std::string cname = ent->keyvalues["classname"];

	if (cubeMap.find(cname) != cubeMap.end())
	{
		return cubeMap[cname];
	}

	return entCubes[0]; // default purple cube from hammer
}

void PointEntRenderer::genPointEntCubes()
{
	// default purple cube
	if (!defaultCubeGen)
	{
		EntCube* defaultCube = new EntCube();
		defaultCube->color = { 220, 0, 220, 255 };
		defaultCube->mins = { -8, -8, -8 };
		defaultCube->maxs = { 8, 8, 8 };
		defaultCube->Textured = false;
		genCubeBuffers(defaultCube);
		entCubes.push_back(defaultCube);
		defaultCubeGen = true;
	}
	if (!fgd)
	{
		return;
	}

	size_t oldCubes = entCubes.size();

	for (size_t i = 0; i < fgd->classes.size(); i++)
	{
		FgdClass* fgdClass = fgd->classes[i];
		if (fgdClass->classType == FGD_CLASS_POINT)
		{
			EntCube* cube = new EntCube();
			cube->mins = fgdClass->mins;
			cube->maxs = fgdClass->maxs;
			cube->color = COLOR4(fgdClass->color, 255);
			cube->Textured = false;

			EntCube* matchingCube = getCubeMatchingProps(cube);
			if (!matchingCube)
			{
				genCubeBuffers(cube);
				entCubes.push_back(cube);
				cubeMap[fgdClass->name] = cube;
			}
			else
			{
				delete cube;
				cubeMap[fgdClass->name] = matchingCube;
			}
		}
	}

	if (entCubes.size() > oldCubes)
	{
		print_log(get_localized_string(LANG_0894), entCubes.size() - oldCubes);
	}
}

EntCube* PointEntRenderer::getCubeMatchingProps(EntCube* entCube)
{
	for (size_t i = 0; i < entCubes.size(); i++)
	{
		if (entCubes[i]->mins == entCube->mins
			&& entCubes[i]->maxs == entCube->maxs
			&& entCubes[i]->color == entCube->color
			&& entCubes[i]->Textured == entCube->Textured)
		{
			return entCubes[i];
		}
	}
	return NULL;
}

void PointEntRenderer::genCubeBuffers(EntCube* entCube)
{
	vec3 min = entCube->mins;
	vec3 max = entCube->maxs;

	// flip for HL coordinates
	min = vec3(min.x, min.z, -min.y);
	max = vec3(max.x, max.z, -max.y);

	delete entCube->cubeBuffer;

	if (!entCube->Textured)
	{
		cCube* cube = new cCube(min, max, entCube->color);
		// colors not where expected due to HL coordinate system
		cube->left.setColor(entCube->color * 0.66f);
		cube->right.setColor(entCube->color * 0.93f);
		cube->top.setColor(entCube->color * 0.40f);
		cube->back.setColor(entCube->color * 0.53f);
		entCube->cubeBuffer = new VertexBuffer(g_app->colorShader, cube, (6 * 6), GL_TRIANGLES);
	}
	else
	{
		tCube* cube = new tCube({ -1.0,min.y,min.z }, { 1.0,max.y, max.z });
		entCube->cubeBuffer = new VertexBuffer(g_app->modelShader, cube, 8, GL_QUADS);
	}

	COLOR4 selectColor = { 220, 0, 0, 255 };
	cCube* selectCube = new cCube(min, max, selectColor);
	// colors not where expected due to HL coordinate system
	selectCube->left.setColor(selectColor * 0.66f);
	selectCube->right.setColor(selectColor * 0.93f);
	selectCube->top.setColor(selectColor * 0.40f);
	selectCube->back.setColor(selectColor * 0.53f);

	delete entCube->selectBuffer;

	entCube->selectBuffer = new VertexBuffer(g_app->colorShader, selectCube, (6 * 6), GL_TRIANGLES);

	delete entCube->axesBuffer;

	cCubeAxes* axescube = new cCubeAxes(min, max);
	entCube->axesBuffer = new VertexBuffer(g_app->colorShader, axescube, (6 * 6), GL_TRIANGLES);


	vec3 vcube[8] = {
		vec3(min.x, min.y, min.z), // front-left-bottom
		vec3(max.x, min.y, min.z), // front-right-bottom
		vec3(max.x, max.y, min.z), // back-right-bottom
		vec3(min.x, max.y, min.z), // back-left-bottom

		vec3(min.x, min.y, max.z), // front-left-top
		vec3(max.x, min.y, max.z), // front-right-top
		vec3(max.x, max.y, max.z), // back-right-top
		vec3(min.x, max.y, max.z), // back-left-top
	};

	// edges
	cVert selectWireframe[12 * 2] = {
		cVert(vcube[0], entCube->sel_color), cVert(vcube[1], entCube->sel_color), // front-bottom
		cVert(vcube[1], entCube->sel_color), cVert(vcube[2], entCube->sel_color), // right-bottom
		cVert(vcube[2], entCube->sel_color), cVert(vcube[3], entCube->sel_color), // back-bottom
		cVert(vcube[3], entCube->sel_color), cVert(vcube[0], entCube->sel_color), // left-bottom

		cVert(vcube[4], entCube->sel_color), cVert(vcube[5], entCube->sel_color), // front-top
		cVert(vcube[5], entCube->sel_color), cVert(vcube[6], entCube->sel_color), // right-top
		cVert(vcube[6], entCube->sel_color), cVert(vcube[7], entCube->sel_color), // back-top
		cVert(vcube[7], entCube->sel_color), cVert(vcube[4], entCube->sel_color), // left-top

		cVert(vcube[0], entCube->sel_color), cVert(vcube[4], entCube->sel_color), // front-left-pillar
		cVert(vcube[1], entCube->sel_color), cVert(vcube[5], entCube->sel_color), // front-right-pillar
		cVert(vcube[2], entCube->sel_color), cVert(vcube[6], entCube->sel_color), // back-right-pillar
		cVert(vcube[3], entCube->sel_color), cVert(vcube[7], entCube->sel_color) // back-left-pillar
	};

	cVert* selectWireframeBuf = new cVert[12 * 2];
	memcpy(selectWireframeBuf, selectWireframe, sizeof(cVert) * 12 * 2);

	delete entCube->wireframeBuffer;

	entCube->wireframeBuffer = new VertexBuffer(g_app->colorShader, selectWireframeBuf, 2 * 12, GL_LINES);

	entCube->axesBuffer->ownData = true;
	entCube->cubeBuffer->ownData = true;
	entCube->selectBuffer->ownData = true;
	entCube->wireframeBuffer->ownData = true;
}
