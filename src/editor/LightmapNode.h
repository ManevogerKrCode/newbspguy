#pragma once
#include "Texture.h"

class LightmapNode
{
public:
	LightmapNode* child[2];
	int x, y, w, h;
	bool filled;

	LightmapNode(int offX, int offY, int mapW, int mapH);
	~LightmapNode(void);

	// places lightmap into the atlas, populating x/y coordinates
	// info width/height must be set before calling
	bool insert(int iw, int ih, int& outX, int& outY);
};

