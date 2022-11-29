#include <string.h>
#include "Command.h"
#include "Renderer.h"
#include "Gui.h"
#include <lodepng.h>
#include "icons/aaatrigger.h"

Command::Command(std::string _desc, int _mapIdx)
{
	this->desc = _desc;
	this->mapIdx = _mapIdx;
	debugf("New undo command added: %s\n", desc.c_str());
}

Bsp* Command::getBsp()
{
	if (mapIdx < 0 || mapIdx >= g_app->mapRenderers.size())
	{
		return NULL;
	}

	return g_app->mapRenderers[mapIdx]->map;
}

BspRenderer* Command::getBspRenderer()
{
	if (mapIdx < 0 || mapIdx >= g_app->mapRenderers.size())
	{
		return NULL;
	}

	return g_app->mapRenderers[mapIdx];
}


//
// Edit entity
//
EditEntityCommand::EditEntityCommand(std::string desc, PickInfo& pickInfo, Entity oldEntData, Entity newEntData)
	: Command(desc, g_app->getSelectedMapId())
{
	this->entIdx = pickInfo.GetSelectedEnt();
	this->oldEntData = Entity();
	this->newEntData = Entity();
	this->oldEntData = oldEntData;
	this->newEntData = newEntData;
	this->allowedDuringLoad = true;
}

EditEntityCommand::~EditEntityCommand()
{
}

void EditEntityCommand::execute()
{
	Entity* target = getEnt();
	*target = newEntData;
	refresh();
}

void EditEntityCommand::undo()
{
	Entity* target = getEnt();
	*target = oldEntData;
	refresh();
}

Entity* EditEntityCommand::getEnt()
{
	Bsp* map = getBsp();

	if (!map || entIdx < 0 || entIdx >= map->ents.size())
	{
		return NULL;
	}

	return map->ents[entIdx];
}

void EditEntityCommand::refresh()
{
	BspRenderer* renderer = getBspRenderer();
	if (!renderer)
		return;
	Entity* ent = getEnt();
	if (!ent)
		return;
	renderer->refreshEnt(entIdx);
	if (!ent->isBspModel())
	{
		renderer->refreshPointEnt(entIdx);
	}
	g_app->updateEntityState(ent);
	g_app->pickCount++; // force GUI update
	g_app->updateModelVerts();
}

size_t EditEntityCommand::memoryUsage()
{
	return sizeof(EditEntityCommand) + oldEntData.getMemoryUsage() + newEntData.getMemoryUsage();
}


//
// Delete entity
//
DeleteEntityCommand::DeleteEntityCommand(std::string desc, PickInfo& pickInfo)
	: Command(desc, g_app->getSelectedMapId())
{
	this->entIdx = pickInfo.GetSelectedEnt();
	this->entData = new Entity();
	*this->entData = *(g_app->getSelectedMap()->ents[entIdx]);
	this->allowedDuringLoad = true;
}

DeleteEntityCommand::~DeleteEntityCommand()
{
	if (entData)
		delete entData;
}

void DeleteEntityCommand::execute()
{
	if (entIdx < 0)
		return;

	Bsp* map = getBsp();

	if (!map)
		return;

	g_app->deselectObject();

	Entity* ent = map->ents[entIdx];

	map->ents.erase(map->ents.begin() + entIdx);

	refresh();

	delete ent;
}

void DeleteEntityCommand::undo()
{
	Bsp* map = getBsp();
	if (!map)
		return;

	Entity* newEnt = new Entity();
	*newEnt = *entData;
	map->ents.insert(map->ents.begin() + entIdx, newEnt);

	g_app->pickInfo.SetSelectedEnt(entIdx);

	refresh();
}

void DeleteEntityCommand::refresh()
{
	BspRenderer* renderer = getBspRenderer();
	renderer->preRenderEnts();
	g_app->gui->refresh();
}

size_t DeleteEntityCommand::memoryUsage()
{
	return sizeof(DeleteEntityCommand) + entData->getMemoryUsage();
}


//
// Create Entity
//
CreateEntityCommand::CreateEntityCommand(std::string desc, int mapIdx, Entity* entData) : Command(desc, mapIdx)
{
	this->entData = new Entity();
	*this->entData = *entData;
	this->allowedDuringLoad = true;
}

CreateEntityCommand::~CreateEntityCommand()
{
	if (entData)
	{
		delete entData;
	}
}

void CreateEntityCommand::execute()
{
	Bsp* map = getBsp();

	Entity* newEnt = new Entity();
	*newEnt = *entData;
	map->ents.push_back(newEnt);
	refresh();
}

void CreateEntityCommand::undo()
{
	Bsp* map = getBsp();

	g_app->deselectObject();

	delete map->ents[map->ents.size() - 1];
	map->ents.pop_back();
	refresh();
}

void CreateEntityCommand::refresh()
{
	BspRenderer* renderer = getBspRenderer();
	renderer->preRenderEnts();
	g_app->gui->refresh();
}

size_t CreateEntityCommand::memoryUsage()
{
	return sizeof(CreateEntityCommand) + entData->getMemoryUsage();
}


//
// Duplicate BSP Model command
//
DuplicateBspModelCommand::DuplicateBspModelCommand(std::string desc, PickInfo& pickInfo)
	: Command(desc, g_app->getSelectedMapId())
{
	int modelIdx = -1;

	int tmpentIdx = pickInfo.GetSelectedEnt();

	if (tmpentIdx >= 0)
	{
		modelIdx = g_app->getSelectedMap()->ents[tmpentIdx]->getBspModelIdx();
	}

	this->oldModelIdx = modelIdx;
	this->newModelIdx = -1;
	this->entIdx = entIdx;
	this->initialized = false;
	this->allowedDuringLoad = false;
	memset(&oldLumps, 0, sizeof(LumpState));
}

DuplicateBspModelCommand::~DuplicateBspModelCommand()
{
	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		if (oldLumps.lumps[i])
			delete[] oldLumps.lumps[i];
	}
}

void DuplicateBspModelCommand::execute()
{
	Bsp* map = getBsp();
	Entity* ent = map->ents[entIdx];
	BspRenderer* renderer = getBspRenderer();

	if (!initialized)
	{
		int dupLumps = CLIPNODES | EDGES | FACES | NODES | PLANES | SURFEDGES | TEXINFO | VERTICES | LIGHTING | MODELS;
		oldLumps = map->duplicate_lumps(dupLumps);
		initialized = true;
	}

	newModelIdx = map->duplicate_model(oldModelIdx);
	ent->setOrAddKeyvalue("model", "*" + std::to_string(newModelIdx));

	renderer->updateLightmapInfos();
	renderer->calcFaceMaths();
	renderer->preRenderFaces();
	renderer->preRenderEnts();
	renderer->reloadLightmaps();
	renderer->addClipnodeModel(newModelIdx);
	g_app->gui->refresh();

	g_app->deselectObject();
	/*
	if (g_app->pickInfo.entIdx[0] == entIdx) {
		g_modelIdx = newModelIdx;
		g_app->updateModelVerts();
	}
	*/
}

void DuplicateBspModelCommand::undo()
{
	Bsp* map = getBsp();
	BspRenderer* renderer = getBspRenderer();

	Entity* ent = map->ents[entIdx];
	map->replace_lumps(oldLumps);
	ent->setOrAddKeyvalue("model", "*" + std::to_string(oldModelIdx));

	renderer->reload();
	g_app->gui->refresh();

	g_app->deselectObject();
	/*
	if (g_app->pickInfo.entIdx[0] == entIdx) {
		g_modelIdx = oldModelIdx;
		g_app->updateModelVerts();
	}
	*/
}

size_t DuplicateBspModelCommand::memoryUsage()
{
	int size = sizeof(DuplicateBspModelCommand);

	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		size += oldLumps.lumpLen[i];
	}

	return size;
}


//
// Create BSP model
//
CreateBspModelCommand::CreateBspModelCommand(std::string desc, int mapIdx, Entity* entData, float size) : Command(desc, mapIdx)
{
	this->entData = new Entity();
	*this->entData = *entData;
	this->mdl_size = size;
	this->initialized = false;
	memset(&oldLumps, 0, sizeof(LumpState));
}

CreateBspModelCommand::~CreateBspModelCommand()
{
	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		if (oldLumps.lumps[i])
			delete[] oldLumps.lumps[i];
	}
	if (entData)
	{
		delete entData;
		entData = NULL;
	}
}

void CreateBspModelCommand::execute()
{
	Bsp* map = getBsp();
	if (!map)
		return;
	BspRenderer* renderer = getBspRenderer();
	if (!renderer)
		return;

	int aaatriggerIdx = getDefaultTextureIdx();

	if (!initialized)
	{
		int dupLumps = CLIPNODES | EDGES | FACES | NODES | PLANES | SURFEDGES | TEXINFO | VERTICES | LIGHTING | MODELS;
		if (aaatriggerIdx == -1)
		{
			dupLumps |= TEXTURES;
		}
		oldLumps = map->duplicate_lumps(dupLumps);
	}

	// add the aaatrigger texture if it doesn't already exist
	if (aaatriggerIdx == -1)
	{
		aaatriggerIdx = addDefaultTexture();
		renderer->reloadTextures();
	}

	vec3 mins = vec3(-mdl_size, -mdl_size, -mdl_size);
	vec3 maxs = vec3(mdl_size, mdl_size, mdl_size);
	int modelIdx = map->create_solid(mins, maxs, aaatriggerIdx);

	if (!initialized)
	{
		entData->addKeyvalue("model", "*" + std::to_string(modelIdx));
	}

	Entity* newEnt = new Entity();
	*newEnt = *entData;
	map->ents.push_back(newEnt);

	g_app->deselectObject();
	renderer->reload();
	g_app->gui->refresh();

	initialized = true;
}

void CreateBspModelCommand::undo()
{
	Bsp* map = getBsp();
	BspRenderer* renderer = getBspRenderer();

	if (!map || !renderer)
		return;

	map->replace_lumps(oldLumps);

	delete map->ents[map->ents.size() - 1];
	map->ents.pop_back();

	renderer->reload();
	g_app->gui->refresh();
	g_app->deselectObject();
}

size_t CreateBspModelCommand::memoryUsage()
{
	int size = sizeof(DuplicateBspModelCommand);

	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		size += oldLumps.lumpLen[i];
	}

	return size;
}

int CreateBspModelCommand::getDefaultTextureIdx()
{
	Bsp* map = getBsp();
	if (!map)
		return -1;

	unsigned int totalTextures = ((unsigned int*)map->textures)[0];
	for (unsigned int i = 0; i < totalTextures; i++)
	{
		int texOffset = ((int*)map->textures)[i + 1];
		BSPMIPTEX& tex = *((BSPMIPTEX*)(map->textures + texOffset));
		if (strcmp(tex.szName, "aaatrigger") == 0)
		{
			return i;
		}
	}

	return -1;
}

int CreateBspModelCommand::addDefaultTexture()
{
	Bsp* map = getBsp();
	if (!map)
		return -1;
	unsigned char* tex_dat = NULL;
	unsigned int w, h;

	lodepng_decode24(&tex_dat, &w, &h, aaatrigger_dat, sizeof(aaatrigger_dat));
	int aaatriggerIdx = map->add_texture("aaatrigger", tex_dat, w, h);
	//renderer->reloadTextures();

	lodepng_encode24_file("test.png", (unsigned char*)tex_dat, w, h);
	delete[] tex_dat;

	return aaatriggerIdx;
}


//
// Edit BSP model
//
EditBspModelCommand::EditBspModelCommand(std::string desc, PickInfo& pickInfo, LumpState oldLumps, LumpState newLumps,
										 vec3 oldOrigin) : Command(desc, g_app->getSelectedMapId())
{
	this->modelIdx = modelIdx;
	this->entIdx = pickInfo.GetSelectedEnt();
	this->oldLumps = oldLumps;
	this->newLumps = newLumps;
	this->allowedDuringLoad = false;
	this->oldOrigin = oldOrigin;
	this->newOrigin = (g_app->getSelectedMap()->ents[entIdx])->getOrigin();
}

EditBspModelCommand::~EditBspModelCommand()
{
	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		if (oldLumps.lumps[i])
			delete[] oldLumps.lumps[i];
		if (newLumps.lumps[i])
			delete[] newLumps.lumps[i];
	}
}

void EditBspModelCommand::execute()
{
	Bsp* map = getBsp();
	BspRenderer* renderer = getBspRenderer();
	if (!map || !renderer)
		return;

	map->replace_lumps(newLumps);
	map->ents[entIdx]->setOrAddKeyvalue("origin", newOrigin.toKeyvalueString());
	g_app->undoEntOrigin = newOrigin;

	refresh();
}

void EditBspModelCommand::undo()
{
	Bsp* map = getBsp();
	if (!map)
		return;

	map->replace_lumps(oldLumps);
	map->ents[entIdx]->setOrAddKeyvalue("origin", oldOrigin.toKeyvalueString());
	g_app->undoEntOrigin = oldOrigin;

	refresh();
}

void EditBspModelCommand::refresh()
{
	Bsp* map = getBsp();
	BspRenderer* renderer = getBspRenderer();
	Entity* ent = map->ents[entIdx];

	renderer->updateLightmapInfos();
	renderer->calcFaceMaths();
	renderer->refreshModel(modelIdx);
	renderer->refreshEnt(entIdx);
	g_app->gui->refresh();
	g_app->saveLumpState(map, 0xffffff, true);
	g_app->updateEntityState(ent);

	if (g_app->pickInfo.GetSelectedEnt() == entIdx)
	{
		g_app->updateModelVerts();
	}
}

size_t EditBspModelCommand::memoryUsage()
{
	int size = sizeof(DuplicateBspModelCommand);

	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		size += oldLumps.lumpLen[i] + newLumps.lumpLen[i];
	}

	return size;
}



//
// Clean Map
//
CleanMapCommand::CleanMapCommand(std::string desc, int mapIdx, LumpState oldLumps) : Command(desc, mapIdx)
{
	this->oldLumps = oldLumps;
	this->allowedDuringLoad = false;
}

CleanMapCommand::~CleanMapCommand()
{
	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		if (oldLumps.lumps[i])
			delete[] oldLumps.lumps[i];
	}
}

void CleanMapCommand::execute()
{
	Bsp* map = getBsp();
	BspRenderer* renderer = getBspRenderer();
	if (!map || !renderer)
		return;
	logf("Cleaning %s\n", map->bsp_name.c_str());
	map->remove_unused_model_structures().print_delete_stats(1);

	refresh();
}

void CleanMapCommand::undo()
{
	Bsp* map = getBsp();

	map->replace_lumps(oldLumps);

	refresh();
}

void CleanMapCommand::refresh()
{
	Bsp* map = getBsp();
	BspRenderer* renderer = getBspRenderer();

	renderer->reload();
	g_app->deselectObject();
	g_app->gui->refresh();
	g_app->saveLumpState(map, 0xffffffff, true);
}

size_t CleanMapCommand::memoryUsage()
{
	int size = sizeof(CleanMapCommand);

	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		size += oldLumps.lumpLen[i];
	}

	return size;
}



//
// Optimize Map
//
OptimizeMapCommand::OptimizeMapCommand(std::string desc, int mapIdx, LumpState oldLumps) : Command(desc, mapIdx)
{
	this->oldLumps = oldLumps;
	this->allowedDuringLoad = false;
}

OptimizeMapCommand::~OptimizeMapCommand()
{
	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		if (oldLumps.lumps[i])
			delete[] oldLumps.lumps[i];
	}
}

void OptimizeMapCommand::execute()
{
	Bsp* map = getBsp();
	BspRenderer* renderer = getBspRenderer();
	if (!map || !renderer)
		return;

	logf("Optimizing %s\n", map->bsp_name.c_str());
	if (!map->has_hull2_ents())
	{
		logf("    Redirecting hull 2 to hull 1 because there are no large monsters/pushables\n");
		map->delete_hull(2, 1);
	}

	bool oldVerbose = g_verbose;
	g_verbose = true;
	map->delete_unused_hulls(true).print_delete_stats(1);
	g_verbose = oldVerbose;

	refresh();
}

void OptimizeMapCommand::undo()
{
	Bsp* map = getBsp();

	map->replace_lumps(oldLumps);

	refresh();
}

void OptimizeMapCommand::refresh()
{
	Bsp* map = getBsp();
	BspRenderer* renderer = getBspRenderer();

	renderer->reload();
	g_app->deselectObject();
	g_app->gui->refresh();
	g_app->saveLumpState(map, 0xffffffff, true);
}

size_t OptimizeMapCommand::memoryUsage()
{
	int size = sizeof(OptimizeMapCommand);

	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		size += oldLumps.lumpLen[i];
	}

	return size;
}