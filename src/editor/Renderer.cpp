#include "Renderer.h"
#include "ShaderProgram.h"
#include "primitives.h"
#include "VertexBuffer.h"
#include "shaders.h"
#include "Gui.h"
#include <algorithm>
#include <map>
#include <sstream>
#include "filedialog/ImFileDialog.h"


AppSettings g_settings;
std::string g_settings_path = "";
std::string g_config_dir = "";

Renderer* g_app = NULL;


vec3 cameraOrigin;
vec3 cameraAngles;


// everything except VIS, ENTITIES, MARKSURFS

std::future<void> Renderer::fgdFuture;

void error_callback(int error, const char* description)
{
	logf("GLFW Error: %s\n", description);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		g_app->hideGui = !g_app->hideGui;
	}
}

void window_size_callback(GLFWwindow* window, int width, int height)
{
	if (g_settings.maximized || width == 0 || height == 0)
	{
		return; // ignore size change when maximized, or else iconifying doesn't change size at all
	}
	g_settings.windowWidth = width;
	g_settings.windowHeight = height;
}

void window_pos_callback(GLFWwindow* window, int x, int y)
{
	g_settings.windowX = x;
	g_settings.windowY = y;
}

void window_maximize_callback(GLFWwindow* window, int maximized)
{
	g_settings.maximized = maximized == GLFW_TRUE;
}

void window_close_callback(GLFWwindow* window)
{
	g_settings.save();
	logf("adios\n");
}

std::string GetWorkDir()
{
	if (g_settings.workingdir.find(':') == std::string::npos)
	{
		return g_settings.gamedir + g_settings.workingdir;
	}
	return g_settings.workingdir;
}

void AppSettings::loadDefault()
{
	settingLoaded = false;

	windowWidth = 800;
	windowHeight = 600;
	windowX = 0;
#ifdef WIN32
	windowY = 30;
#else
	windowY = 0;
#endif
	maximized = 0;
	fontSize = 22.f;
	gamedir = std::string();
	workingdir = "/bspguy_work/";

	lastdir = "";
	undoLevels = 64;
	verboseLogs = false;
	debug_open = false;
	keyvalue_open = false;
	transform_open = false;
	log_open = false;
	settings_open = false;
	limits_open = false;
	entreport_open = false;
	show_transform_axes = false;
	settings_tab = 0;

	render_flags = g_render_flags = RENDER_TEXTURES | RENDER_LIGHTMAPS | RENDER_SPECIAL
		| RENDER_ENTS | RENDER_SPECIAL_ENTS | RENDER_POINT_ENTS | RENDER_WIREFRAME | RENDER_ENT_CONNECTIONS
		| RENDER_ENT_CLIPNODES;

	vsync = true;
	backUpMap = true;
	preserveCrc32 = false;
	autoImportEnt = false;
	sameDirForEnt = false;

	moveSpeed = 4.0f;
	fov = 75.0f;
	zfar = 262144.0f;
	rotSpeed = 5.0f;

	fgdPaths.clear();
	resPaths.clear();

	conditionalPointEntTriggers.clear();
	entsThatNeverNeedAnyHulls.clear();
	entsThatNeverNeedCollision.clear();
	passableEnts.clear();
	playerOnlyTriggers.clear();
	monsterOnlyTriggers.clear();

	ResetBspLimits();
}

void AppSettings::reset()
{
	loadDefault();

	fgdPaths.clear();
	fgdPaths.push_back("/moddir/GameDefinitionFile.fgd");

	resPaths.clear();
	resPaths.push_back("/moddir/");
	resPaths.push_back("/moddir_addon/");

	conditionalPointEntTriggers.clear();
	conditionalPointEntTriggers.push_back("trigger_once");
	conditionalPointEntTriggers.push_back("trigger_multiple");
	conditionalPointEntTriggers.push_back("trigger_counter");
	conditionalPointEntTriggers.push_back("trigger_gravity");
	conditionalPointEntTriggers.push_back("trigger_teleport");

	entsThatNeverNeedAnyHulls.clear();
	entsThatNeverNeedAnyHulls.push_back("env_bubbles");
	entsThatNeverNeedAnyHulls.push_back("func_tankcontrols");
	entsThatNeverNeedAnyHulls.push_back("func_traincontrols");
	entsThatNeverNeedAnyHulls.push_back("func_vehiclecontrols");
	entsThatNeverNeedAnyHulls.push_back("trigger_autosave"); // obsolete in sven
	entsThatNeverNeedAnyHulls.push_back("trigger_endsection"); // obsolete in sven

	entsThatNeverNeedCollision.clear();
	entsThatNeverNeedCollision.push_back("func_illusionary");
	entsThatNeverNeedCollision.push_back("func_mortar_field");

	passableEnts.clear();
	passableEnts.push_back("func_door");
	passableEnts.push_back("func_door_rotating");
	passableEnts.push_back("func_pendulum");
	passableEnts.push_back("func_tracktrain");
	passableEnts.push_back("func_train");
	passableEnts.push_back("func_water");
	passableEnts.push_back("momentary_door");

	playerOnlyTriggers.clear();
	playerOnlyTriggers.push_back("func_ladder");
	playerOnlyTriggers.push_back("game_zone_player");
	playerOnlyTriggers.push_back("player_respawn_zone");
	playerOnlyTriggers.push_back("trigger_cdaudio");
	playerOnlyTriggers.push_back("trigger_changelevel");
	playerOnlyTriggers.push_back("trigger_transition");

	monsterOnlyTriggers.clear();
	monsterOnlyTriggers.push_back("func_monsterclip");
	monsterOnlyTriggers.push_back("trigger_monsterjump");
}

void AppSettings::load()
{
	std::ifstream file(g_settings_path);
	if (file.is_open())
	{
		int lines_readed = 0;
		std::string line;
		while (getline(file, line))
		{
			if (line.empty())
				continue;

			size_t eq = line.find('=');
			if (eq == std::string::npos)
			{
				continue;
			}
			lines_readed++;

			std::string key = trimSpaces(line.substr(0, eq));
			std::string val = trimSpaces(line.substr(eq + 1));

			if (key == "window_width")
			{
				g_settings.windowWidth = atoi(val.c_str());
			}
			else if (key == "window_height")
			{
				g_settings.windowHeight = atoi(val.c_str());
			}
			else if (key == "window_x")
			{
				g_settings.windowX = atoi(val.c_str());
			}
			else if (key == "window_y")
			{
				g_settings.windowY = atoi(val.c_str());
			}
			else if (key == "window_maximized")
			{
				g_settings.maximized = atoi(val.c_str());
			}
			else if (key == "debug_open")
			{
				g_settings.debug_open = atoi(val.c_str()) != 0;
			}
			else if (key == "keyvalue_open")
			{
				g_settings.keyvalue_open = atoi(val.c_str()) != 0;
			}
			else if (key == "transform_open")
			{
				g_settings.transform_open = atoi(val.c_str()) != 0;
			}
			else if (key == "log_open")
			{
				g_settings.log_open = atoi(val.c_str()) != 0;
			}
			else if (key == "settings_open")
			{
				g_settings.settings_open = atoi(val.c_str()) != 0;
			}
			else if (key == "limits_open")
			{
				g_settings.limits_open = atoi(val.c_str()) != 0;
			}
			else if (key == "entreport_open")
			{
				g_settings.entreport_open = atoi(val.c_str()) != 0;
			}
			else if (key == "settings_tab")
			{
				g_settings.settings_tab = atoi(val.c_str());
			}
			else if (key == "vsync")
			{
				g_settings.vsync = atoi(val.c_str()) != 0;
			}
			else if (key == "show_transform_axes")
			{
				g_settings.show_transform_axes = atoi(val.c_str()) != 0;
			}
			else if (key == "verbose_logs")
			{
				g_settings.verboseLogs = atoi(val.c_str()) != 0;
			}
			else if (key == "fov")
			{
				g_settings.fov = (float)atof(val.c_str());
			}
			else if (key == "zfar")
			{
				g_settings.zfar = (float)atof(val.c_str());
			}
			else if (key == "move_speed")
			{
				g_settings.moveSpeed = (float)atof(val.c_str());
			}
			else if (key == "rot_speed")
			{
				g_settings.rotSpeed = (float)atof(val.c_str());
			}
			else if (key == "render_flags")
			{
				g_settings.render_flags = atoi(val.c_str());
			}
			else if (key == "font_size")
			{
				g_settings.fontSize = (float)atof(val.c_str());
			}
			else if (key == "undo_levels")
			{
				g_settings.undoLevels = atoi(val.c_str());
			}
			else if (key == "gamedir")
			{
				g_settings.gamedir = val;
			}
			else if (key == "workingdir")
			{
				g_settings.workingdir = val;
			}
			else if (key == "lastdir")
			{
				g_settings.lastdir = val;
			}
			else if (key == "fgd")
			{
				fgdPaths.push_back(val);
			}
			else if (key == "res")
			{
				resPaths.push_back(val);
			}
			else if (key == "savebackup")
			{
				g_settings.backUpMap = atoi(val.c_str()) != 0;
			}
			else if (key == "save_crc")
			{
				g_settings.preserveCrc32 = atoi(val.c_str()) != 0;
			}
			else if (key == "auto_import_ent")
			{
				g_settings.autoImportEnt = atoi(val.c_str()) != 0;
			}
			else if (key == "same_dir_for_ent")
			{
				g_settings.sameDirForEnt = atoi(val.c_str()) != 0;
			}
			else if (key == "optimizer_cond_ents")
			{
				conditionalPointEntTriggers.push_back(val);
			}
			else if (key == "optimizer_no_hulls_ents")
			{
				entsThatNeverNeedAnyHulls.push_back(val);
			}
			else if (key == "optimizer_no_collision_ents")
			{
				entsThatNeverNeedCollision.push_back(val);
			}
			else if (key == "optimizer_passable_ents")
			{
				passableEnts.push_back(val);
			}
			else if (key == "optimizer_player_hull_ents")
			{
				playerOnlyTriggers.push_back(val);
			}
			else if (key == "optimizer_monster_hull_ents")
			{
				monsterOnlyTriggers.push_back(val);
			}
		}

		if (g_settings.windowY == -32000 &&
			g_settings.windowX == -32000)
		{
			g_settings.windowY = 0;
			g_settings.windowX = 0;
		}


#ifdef WIN32
		// Fix invisibled window header for primary screen.
		if (g_settings.windowY >= 0 && g_settings.windowY < 30)
		{
			g_settings.windowY = 30;
		}
#endif

		// Restore default window height if invalid.
		if (windowHeight <= 0 || windowWidth <= 0)
		{
			windowHeight = 600;
			windowWidth = 800;
		}

		if (lines_readed > 0)
			g_settings.settingLoaded = true;
		else
			logf("Failed to load user config: %s\n", g_settings_path.c_str());
	}
	else
	{
		reset();
		logf("Failed to open user config: %s\n", g_settings_path.c_str());
	}
}

void AppSettings::save(std::string path)
{
	if (!g_settings.settingLoaded || !g_app->gui->settingLoaded)
		return;

	std::ostringstream file;

	file << "window_width=" << g_settings.windowWidth << std::endl;
	file << "window_height=" << g_settings.windowHeight << std::endl;
	file << "window_x=" << g_settings.windowX << std::endl;
	file << "window_y=" << g_settings.windowY << std::endl;
	file << "window_maximized=" << g_settings.maximized << std::endl;

	file << "debug_open=" << g_settings.debug_open << std::endl;
	file << "keyvalue_open=" << g_settings.keyvalue_open << std::endl;
	file << "transform_open=" << g_settings.transform_open << std::endl;
	file << "log_open=" << g_settings.log_open << std::endl;
	file << "settings_open=" << g_settings.settings_open << std::endl;
	file << "limits_open=" << g_settings.limits_open << std::endl;
	file << "entreport_open=" << g_settings.entreport_open << std::endl;

	file << "settings_tab=" << g_settings.settings_tab << std::endl;

	file << "gamedir=" << g_settings.gamedir << std::endl;
	file << "workingdir=" << g_settings.workingdir << std::endl;
	file << "lastdir=" << g_settings.lastdir << std::endl;
	for (int i = 0; i < fgdPaths.size(); i++)
	{
		file << "fgd=" << g_settings.fgdPaths[i] << std::endl;
	}

	for (int i = 0; i < resPaths.size(); i++)
	{
		file << "res=" << g_settings.resPaths[i] << std::endl;
	}

	for (int i = 0; i < conditionalPointEntTriggers.size(); i++)
	{
		file << "optimizer_cond_ents=" << g_settings.conditionalPointEntTriggers[i] << std::endl;
	}

	for (int i = 0; i < entsThatNeverNeedAnyHulls.size(); i++)
	{
		file << "optimizer_no_hulls_ents=" << g_settings.entsThatNeverNeedAnyHulls[i] << std::endl;
	}

	for (int i = 0; i < entsThatNeverNeedCollision.size(); i++)
	{
		file << "optimizer_no_collision_ents=" << g_settings.entsThatNeverNeedCollision[i] << std::endl;
	}

	for (int i = 0; i < passableEnts.size(); i++)
	{
		file << "optimizer_passable_ents=" << g_settings.passableEnts[i] << std::endl;
	}

	for (int i = 0; i < playerOnlyTriggers.size(); i++)
	{
		file << "optimizer_player_hull_ents=" << g_settings.playerOnlyTriggers[i] << std::endl;
	}

	for (int i = 0; i < monsterOnlyTriggers.size(); i++)
	{
		file << "optimizer_monster_hull_ents=" << g_settings.monsterOnlyTriggers[i] << std::endl;
	}

	file << "vsync=" << g_settings.vsync << std::endl;
	file << "show_transform_axes=" << g_settings.show_transform_axes << std::endl;
	file << "verbose_logs=" << g_settings.verboseLogs << std::endl;
	file << "fov=" << g_settings.fov << std::endl;
	file << "zfar=" << g_settings.zfar << std::endl;
	file << "move_speed=" << g_settings.moveSpeed << std::endl;
	file << "rot_speed=" << g_settings.rotSpeed << std::endl;
	file << "render_flags=" << g_settings.render_flags << std::endl;
	file << "font_size=" << g_settings.fontSize << std::endl;
	file << "undo_levels=" << g_settings.undoLevels << std::endl;
	file << "savebackup=" << g_settings.backUpMap << std::endl;
	file << "save_crc=" << g_settings.preserveCrc32 << std::endl;
	file << "auto_import_ent" << g_settings.autoImportEnt << std::endl;
	file << "same_dir_for_ent" << g_settings.sameDirForEnt << std::endl;

	file.flush();

	writeFile(g_settings_path, file.str());
}

void AppSettings::save()
{
	if (!dirExists(g_config_dir))
	{
		createDir(g_config_dir);
	}
	g_app->saveSettings();
	save(g_settings_path);
}

int g_scroll = 0;

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	g_scroll += (int)round(yoffset);
}

Renderer::Renderer()
{
	g_settings.loadDefault();
	g_settings.load();

	if (!glfwInit())
	{
		logf("GLFW initialization failed\n");
		return;
	}

	glfwSetErrorCallback(error_callback);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	window = glfwCreateWindow(g_settings.windowWidth, g_settings.windowHeight, "bspguy", NULL, NULL);

	if (g_settings.settingLoaded)
	{
		glfwSetWindowPos(window, g_settings.windowX, g_settings.windowY);

		// setting size again to fix issue where window is too small because it was
		// moved to a monitor with a different DPI than the one it was created for
		glfwSetWindowSize(window, g_settings.windowWidth, g_settings.windowHeight);
		if (g_settings.maximized)
		{
			glfwMaximizeWindow(window);
		}
	}

	if (!window)
	{
		logf("Window creation failed. Maybe your PC doesn't support OpenGL 3.0\n");
		return;
	}

	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, key_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetWindowSizeCallback(window, window_size_callback);
	glfwSetWindowPosCallback(window, window_pos_callback);
	glfwSetWindowCloseCallback(window, window_close_callback);
	glfwSetWindowMaximizeCallback(window, window_maximize_callback);

	glewInit();

	// init to black screen instead of white
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glfwSwapBuffers(window);
	glfwSwapInterval(1);


	gui = new Gui(this);

	bspShader = new ShaderProgram(Shaders::g_shader_multitexture_vertex, Shaders::g_shader_multitexture_fragment);
	bspShader->setMatrixes(&matmodel, &matview, &projection, &modelView, &modelViewProjection);
	bspShader->setMatrixNames(NULL, "modelViewProjection");

	fullBrightBspShader = new ShaderProgram(Shaders::g_shader_fullbright_vertex, Shaders::g_shader_fullbright_fragment);
	fullBrightBspShader->setMatrixes(&matmodel, &matview, &projection, &modelView, &modelViewProjection);
	fullBrightBspShader->setMatrixNames(NULL, "modelViewProjection");

	colorShader = new ShaderProgram(Shaders::g_shader_cVert_vertex, Shaders::g_shader_cVert_fragment);
	colorShader->setMatrixes(&matmodel, &matview, &projection, &modelView, &modelViewProjection);
	colorShader->setMatrixNames(NULL, "modelViewProjection");
	colorShader->setVertexAttributeNames("vPosition", "vColor", NULL);

	colorShader->bind();
	unsigned int colorMultId = glGetUniformLocation(colorShader->ID, "colorMult");
	glUniform4f(colorMultId, 1, 1, 1, 1);


	clearSelection();


	oldLeftMouse = curLeftMouse = oldRightMouse = curRightMouse = 0;

	g_app = this;

	g_progress.simpleMode = true;

	pointEntRenderer = new PointEntRenderer(NULL, colorShader);

	loadSettings();

	reloading = true;
	fgdFuture = std::async(std::launch::async, &Renderer::loadFgds, this);

	memset(&undoLumpState, 0, sizeof(LumpState));

	//cameraOrigin = vec3(51, 427, 234);
	//cameraAngles = vec3(41, 0, -170);
}

Renderer::~Renderer()
{
	glfwTerminate();
}
void Renderer::renderLoop()
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);

	{
		line_verts = new cVert[2];
		lineBuf = new VertexBuffer(colorShader, COLOR_4B | POS_3F, line_verts, 2, GL_LINES);
	}

	{
		plane_verts = new cQuad(cVert(), cVert(), cVert(), cVert());
		planeBuf = new VertexBuffer(colorShader, COLOR_4B | POS_3F, plane_verts, 6, GL_TRIANGLES);
	}

	{
		moveAxes.dimColor[0] = {110, 0, 160, 255};
		moveAxes.dimColor[1] = {0, 0, 220, 255};
		moveAxes.dimColor[2] = {0, 160, 0, 255};
		moveAxes.dimColor[3] = {160, 160, 160, 255};

		moveAxes.hoverColor[0] = {128, 64, 255, 255};
		moveAxes.hoverColor[1] = {64, 64, 255, 255};
		moveAxes.hoverColor[2] = {64, 255, 64, 255};
		moveAxes.hoverColor[3] = {255, 255, 255, 255};

		// flipped for HL coords
		moveAxes.buffer = new VertexBuffer(colorShader, COLOR_4B | POS_3F, &moveAxes.model, 6 * 6 * 4, GL_TRIANGLES);
		moveAxes.numAxes = 4;
	}

	{
		scaleAxes.dimColor[0] = {110, 0, 160, 255};
		scaleAxes.dimColor[1] = {0, 0, 220, 255};
		scaleAxes.dimColor[2] = {0, 160, 0, 255};

		scaleAxes.dimColor[3] = {110, 0, 160, 255};
		scaleAxes.dimColor[4] = {0, 0, 220, 255};
		scaleAxes.dimColor[5] = {0, 160, 0, 255};

		scaleAxes.hoverColor[0] = {128, 64, 255, 255};
		scaleAxes.hoverColor[1] = {64, 64, 255, 255};
		scaleAxes.hoverColor[2] = {64, 255, 64, 255};

		scaleAxes.hoverColor[3] = {128, 64, 255, 255};
		scaleAxes.hoverColor[4] = {64, 64, 255, 255};
		scaleAxes.hoverColor[5] = {64, 255, 64, 255};

		// flipped for HL coords
		scaleAxes.buffer = new VertexBuffer(colorShader, COLOR_4B | POS_3F, &scaleAxes.model, 6 * 6 * 6, GL_TRIANGLES);
		scaleAxes.numAxes = 6;
	}

	updateDragAxes();

	g_time = glfwGetTime();

	double lastFrameTime = g_time;
	double lastTitleTime = g_time;


	while (!glfwWindowShouldClose(window))
	{
		g_time = glfwGetTime();
		g_frame_counter++;

		Bsp* map = getSelectedMap();
		if (g_time - lastTitleTime > 0.5)
		{
			lastTitleTime = g_time;
			if (map)
			{
				glfwSetWindowTitle(window, std::string(std::string("bspguy - ") + map->bsp_path).c_str());
			}
		}
		glfwPollEvents();

		double frameDelta = g_time - lastFrameTime;
		frameTimeScale = 0.05 / frameDelta;
		double fps = 1.0 / frameDelta;

		//FIXME : frameTimeScale = 0.05f / frameDelta ???
		frameTimeScale = 100.0 / fps;

		lastFrameTime = g_time;

		double spin = g_time * 2;

		matmodel.loadIdentity();
		matmodel.rotateZ((float)spin);
		matmodel.rotateX((float)spin);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		setupView();
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);

		drawEntConnections();

		isLoading = reloading;

		Bsp* selectedMap = getSelectedMap();

		std::set<int> modelidskip;
		for (size_t i = 0; i < mapRenderers.size(); i++)
		{
			int highlightEnt = -1;
			Bsp* curMap = mapRenderers[i]->map;
			if (!curMap)
				continue;

			if (map == curMap && pickMode == PICK_OBJECT)
			{
				highlightEnt = pickInfo.entIdx;
			}

			if (selectedMap && getSelectedMap() != curMap && !curMap->is_model)
			{
				continue;
			}

			if (curMap->ents.size() && !isLoading)
			{
				if (curMap->is_model)
				{
					for (size_t n = 0; n < mapRenderers.size(); n++)
					{
						if (n == i)
							continue;

						Bsp* anotherMap = mapRenderers[n]->map;
						if (anotherMap && anotherMap->ents.size())
						{
							vec3 anotherMapOrigin = anotherMap->ents[0]->getOrigin();
							for (int s = 0; s < (int)anotherMap->ents.size(); s++)
							{
								Entity* tmpEnt = anotherMap->ents[s];
								if (tmpEnt->hasKey("model"))
								{
									if (!modelidskip.count(s))
									{
										if (basename(tmpEnt->keyvalues["model"]) == basename(curMap->bsp_path))
										{
											curMap->ents[0]->setOrAddKeyvalue("origin", (tmpEnt->getOrigin() + anotherMapOrigin).toKeyvalueString());
											modelidskip.insert(s);
											break;
										}
									}
								}
							}
						}
					}
				}
			}

			mapRenderers[i]->render(highlightEnt, transformTarget == TRANSFORM_VERTEX, clipnodeRenderHull);


			if (!mapRenderers[i]->isFinishedLoading())
			{
				isLoading = true;
			}
		}

		matmodel.loadIdentity();
		colorShader->bind();

		if (map)
		{
			if (debugClipnodes && pickInfo.modelIdx > 0)
			{
				BSPMODEL& pickModel = map->models[pickInfo.modelIdx];
				glDisable(GL_CULL_FACE);
				int currentPlane = 0;
				drawClipnodes(map, pickModel.iHeadnodes[1], currentPlane, debugInt);
				debugIntMax = currentPlane - 1;
				glEnable(GL_CULL_FACE);
			}

			if (debugNodes && pickInfo.modelIdx > 0)
			{
				BSPMODEL& pickModel = map->models[pickInfo.modelIdx];
				glDisable(GL_CULL_FACE);
				int currentPlane = 0;
				drawNodes(map, pickModel.iHeadnodes[0], currentPlane, debugNode);
				debugNodeMax = currentPlane - 1;
				glEnable(GL_CULL_FACE);
			}

			if (g_render_flags & RENDER_ORIGIN)
			{
				colorShader->bind();
				matmodel.loadIdentity();
				colorShader->pushMatrix(MAT_MODEL);
				vec3 offset = map->getBspRender()->mapOffset.flip();
				matmodel.translate(offset.x, offset.y, offset.z);
				colorShader->updateMatrixes();
				drawLine(debugPoint - vec3(32, 0, 0), debugPoint + vec3(32, 0, 0), {128, 128, 255, 255});
				drawLine(debugPoint - vec3(0, 32, 0), debugPoint + vec3(0, 32, 0), {0, 255, 0, 255});
				drawLine(debugPoint - vec3(0, 0, 32), debugPoint + vec3(0, 0, 32), {0, 0, 255, 255});
				colorShader->popMatrix(MAT_MODEL);
			}
		}

		if (entConnectionPoints && (g_render_flags & RENDER_ENT_CONNECTIONS))
		{
			matmodel.loadIdentity();
			colorShader->updateMatrixes();
			glDisable(GL_DEPTH_TEST);
			entConnectionPoints->drawFull();
			glEnable(GL_DEPTH_TEST);
		}

		bool isScalingObject = transformMode == TRANSFORM_SCALE && transformTarget == TRANSFORM_OBJECT;
		bool isMovingOrigin = transformMode == TRANSFORM_MOVE && transformTarget == TRANSFORM_ORIGIN;
		bool isTransformingValid = (!modelUsesSharedStructures || (transformMode == TRANSFORM_MOVE && transformTarget != TRANSFORM_VERTEX)) && (isTransformableSolid || isScalingObject);
		bool isTransformingWorld = pickInfo.entIdx == 0 && transformTarget != TRANSFORM_OBJECT;

		if (showDragAxes && pickMode == pick_modes::PICK_OBJECT)
		{
			if (!movingEnt && !isTransformingWorld && pickInfo.entIdx >= 0 && (isTransformingValid || isMovingOrigin))
			{
				drawTransformAxes();
			}
		}

		if (pickInfo.entIdx == 0)
		{
			if (map && map->is_model)
			{
				map->selectModelEnt();
				if (pickInfo.entIdx == 0)
					pickInfo.entIdx = -1;
			}
		}
		if (pickInfo.modelIdx > 0 && pickMode == PICK_OBJECT)
		{
			if (transformTarget == TRANSFORM_VERTEX && isTransformableSolid)
			{
				drawModelVerts();
			}
			if (transformTarget == TRANSFORM_ORIGIN)
			{
				drawModelOrigin();
			}
		}

		vec3 forward, right, up;
		makeVectors(cameraAngles, forward, right, up);
		//logf("DRAW %.1f %.1f %.1f -> %.1f %.1f %.1f\n", pickStart.x, pickStart.y, pickStart.z, pickDir.x, pickDir.y, pickDir.z);

		if (!g_app->hideGui)
			gui->draw();

		controls();

		glfwSwapBuffers(window);

		if (reloading && fgdFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
		{
			postLoadFgds();
			reloading = reloadingGameDir = false;
		}

		int glerror = glGetError();
		if (glerror != GL_NO_ERROR)
		{
			logf("Got OpenGL Error: %d\n", glerror);
		}
	}

	glfwTerminate();
}

void Renderer::postLoadFgds()
{
	delete pointEntRenderer;
	delete fgd;

	pointEntRenderer = swapPointEntRenderer;
	fgd = pointEntRenderer->fgd;

	for (int i = 0; i < mapRenderers.size(); i++)
	{
		mapRenderers[i]->pointEntRenderer = pointEntRenderer;
		mapRenderers[i]->preRenderEnts();
		if (reloadingGameDir)
		{
			mapRenderers[i]->reloadTextures();
		}
	}

	swapPointEntRenderer = NULL;
}

void Renderer::postLoadFgdsAndTextures()
{
	if (reloading)
	{
		logf("Previous reload not finished. Aborting reload.");
		return;
	}
	reloading = reloadingGameDir = true;
	fgdFuture = std::async(std::launch::async, &Renderer::loadFgds, this);
}

void Renderer::clearMaps()
{
	for (int i = 0; i < mapRenderers.size(); i++)
	{
		delete mapRenderers[i];
	}
	mapRenderers.clear();
	clearSelection();
	clearUndoCommands();
	clearRedoCommands();

	logf("Cleared map list\n");
}

void Renderer::reloadMaps()
{
	std::vector<std::string> reloadPaths;
	for (int i = 0; i < mapRenderers.size(); i++)
	{
		reloadPaths.push_back(mapRenderers[i]->map->bsp_path);
		delete mapRenderers[i];
	}
	mapRenderers.clear();
	clearSelection();
	for (int i = 0; i < reloadPaths.size(); i++)
	{
		addMap(new Bsp(reloadPaths[i]));
	}

	clearUndoCommands();
	clearRedoCommands();

	logf("Reloaded maps\n");
}

void Renderer::saveSettings()
{
	g_settings.debug_open = gui->showDebugWidget;
	g_settings.keyvalue_open = gui->showKeyvalueWidget;
	g_settings.transform_open = gui->showTransformWidget;
	g_settings.log_open = gui->showLogWidget;
	g_settings.settings_open = gui->showSettingsWidget;
	g_settings.limits_open = gui->showLimitsWidget;
	g_settings.entreport_open = gui->showEntityReport;
	g_settings.settings_tab = gui->settingsTab;
	g_settings.vsync = gui->vsync;
	g_settings.show_transform_axes = showDragAxes;
	g_settings.verboseLogs = g_verbose;
	g_settings.zfar = zFar;
	g_settings.fov = fov;
	g_settings.render_flags = g_render_flags;
	g_settings.fontSize = gui->fontSize;
	g_settings.undoLevels = undoLevels;
	g_settings.moveSpeed = moveSpeed;
	g_settings.rotSpeed = rotationSpeed;
}

void Renderer::loadSettings()
{
	gui->showDebugWidget = g_settings.debug_open;
	gui->showKeyvalueWidget = g_settings.keyvalue_open;
	gui->showTransformWidget = g_settings.transform_open;
	gui->showLogWidget = g_settings.log_open;
	gui->showSettingsWidget = g_settings.settings_open;
	gui->showLimitsWidget = g_settings.limits_open;
	gui->showEntityReport = g_settings.entreport_open;

	gui->settingsTab = g_settings.settings_tab;
	gui->openSavedTabs = true;

	gui->vsync = g_settings.vsync;
	showDragAxes = g_settings.show_transform_axes;
	g_verbose = g_settings.verboseLogs;
	zFar = g_settings.zfar;
	fov = g_settings.fov;
	g_render_flags = g_settings.render_flags;
	gui->fontSize = g_settings.fontSize;
	undoLevels = g_settings.undoLevels;
	rotationSpeed = g_settings.rotSpeed;
	moveSpeed = g_settings.moveSpeed;

	gui->shouldReloadFonts = true;

	glfwSwapInterval(gui->vsync ? 1 : 0);

	gui->settingLoaded = true;
}

void Renderer::loadFgds()
{
	Fgd* mergedFgd = NULL;
	for (int i = 0; i < g_settings.fgdPaths.size(); i++)
	{
		Fgd* tmp = new Fgd(g_settings.fgdPaths[i]);
		if (!tmp->parse())
		{
			tmp->path = g_settings.gamedir + g_settings.fgdPaths[i];
			if (!tmp->parse())
			{
				continue;
			}
		}

		if (i == 0 || !mergedFgd)
		{
			mergedFgd = tmp;
		}
		else
		{
			mergedFgd->merge(tmp);
			delete tmp;
		}
	}

	swapPointEntRenderer = new PointEntRenderer(mergedFgd, colorShader);
}

void Renderer::drawModelVerts()
{
	Bsp* map = g_app->getSelectedMap();
	if (!modelVertBuff || modelVerts.empty() || !map || pickInfo.entIdx < 0)
		return;
	glClear(GL_DEPTH_BUFFER_BIT);

	Entity* ent = map->ents[pickInfo.entIdx];
	vec3 mapOffset = map->getBspRender()->mapOffset;
	vec3 renderOffset = mapOffset.flip();
	vec3 localCameraOrigin = cameraOrigin - mapOffset;

	COLOR4 vertDimColor = {200, 200, 200, 255};
	COLOR4 vertHoverColor = {255, 255, 255, 255};
	COLOR4 edgeDimColor = {255, 128, 0, 255};
	COLOR4 edgeHoverColor = {255, 255, 0, 255};
	COLOR4 selectColor = {0, 128, 255, 255};
	COLOR4 hoverSelectColor = {96, 200, 255, 255};
	vec3 entOrigin = ent->getOrigin();

	if (modelUsesSharedStructures)
	{
		vertDimColor = {32, 32, 32, 255};
		edgeDimColor = {64, 64, 32, 255};
	}

	int cubeIdx = 0;
	for (int i = 0; i < modelVerts.size(); i++)
	{
		vec3 ori = modelVerts[i].pos + entOrigin;
		float s = (ori - localCameraOrigin).length() * vertExtentFactor;
		ori = ori.flip();

		if (anyEdgeSelected)
		{
			s = 0; // can't select certs when edges are selected
		}

		vec3 min = vec3(-s, -s, -s) + ori;
		vec3 max = vec3(s, s, s) + ori;
		COLOR4 color;
		if (modelVerts[i].selected)
		{
			color = i == hoverVert ? hoverSelectColor : selectColor;
		}
		else
		{
			color = i == hoverVert ? vertHoverColor : vertDimColor;
		}
		modelVertCubes[cubeIdx++] = cCube(min, max, color);
	}

	for (int i = 0; i < modelEdges.size(); i++)
	{
		vec3 ori = getEdgeControlPoint(modelVerts, modelEdges[i]) + entOrigin;
		float s = (ori - localCameraOrigin).length() * vertExtentFactor;
		ori = ori.flip();

		if (anyVertSelected && !anyEdgeSelected)
		{
			s = 0; // can't select edges when verts are selected
		}

		vec3 min = vec3(-s, -s, -s) + ori;
		vec3 max = vec3(s, s, s) + ori;
		COLOR4 color;
		if (modelEdges[i].selected)
		{
			color = i == hoverEdge ? hoverSelectColor : selectColor;
		}
		else
		{
			color = i == hoverEdge ? edgeHoverColor : edgeDimColor;
		}
		modelVertCubes[cubeIdx++] = cCube(min, max, color);
	}

	matmodel.loadIdentity();
	matmodel.translate(renderOffset.x, renderOffset.y, renderOffset.z);
	colorShader->updateMatrixes();
	modelVertBuff->drawFull();
}

void Renderer::drawModelOrigin()
{
	if (!modelOriginBuff)
		return;

	glClear(GL_DEPTH_BUFFER_BIT);

	Bsp* map = g_app->getSelectedMap();
	vec3 mapOffset = map->getBspRender()->mapOffset;

	COLOR4 vertDimColor = {0, 200, 0, 255};
	COLOR4 vertHoverColor = {128, 255, 128, 255};
	COLOR4 selectColor = {0, 128, 255, 255};
	COLOR4 hoverSelectColor = {96, 200, 255, 255};

	if (modelUsesSharedStructures)
	{
		vertDimColor = {32, 32, 32, 255};
	}

	vec3 ori = transformedOrigin + mapOffset;
	float s = (ori - cameraOrigin).length() * vertExtentFactor;
	ori = ori.flip();

	vec3 min = vec3(-s, -s, -s) + ori;
	vec3 max = vec3(s, s, s) + ori;
	COLOR4 color;
	if (originSelected)
	{
		color = originHovered ? hoverSelectColor : selectColor;
	}
	else
	{
		color = originHovered ? vertHoverColor : vertDimColor;
	}
	modelOriginCube = cCube(min, max, color);

	matmodel.loadIdentity();
	colorShader->updateMatrixes();
	modelOriginBuff->drawFull();
}

void Renderer::drawTransformAxes()
{
	glClear(GL_DEPTH_BUFFER_BIT);
	updateDragAxes();
	glDisable(GL_CULL_FACE);
	if (transformMode == TRANSFORM_SCALE && transformTarget == TRANSFORM_OBJECT)
	{
		vec3 ori = scaleAxes.origin;
		matmodel.translate(ori.x, ori.z, -ori.y);
		colorShader->updateMatrixes();
		scaleAxes.buffer->drawFull();
	}
	if (transformMode == TRANSFORM_MOVE)
	{
		vec3 ori = moveAxes.origin;
		matmodel.translate(ori.x, ori.z, -ori.y);
		colorShader->updateMatrixes();
		moveAxes.buffer->drawFull();
	}
	dragDelta = vec3();
}

void Renderer::drawEntConnections()
{
	if (entConnections && (g_render_flags & RENDER_ENT_CONNECTIONS))
	{
		matmodel.loadIdentity();
		colorShader->updateMatrixes();
		entConnections->drawFull();
	}
}

void Renderer::controls()
{
	canControl = !gui->imgui_io->WantCaptureKeyboard && !gui->imgui_io->WantTextInput && !gui->imgui_io->WantCaptureMouseUnlessPopupClose;

	for (int i = GLFW_KEY_SPACE; i < GLFW_KEY_LAST; i++)
	{
		pressed[i] = glfwGetKey(window, i) == GLFW_PRESS;
		released[i] = glfwGetKey(window, i) == GLFW_RELEASE;
	}

	DebugKeyPressed = pressed[GLFW_KEY_F1];

	anyCtrlPressed = pressed[GLFW_KEY_LEFT_CONTROL] || pressed[GLFW_KEY_RIGHT_CONTROL];
	anyAltPressed = pressed[GLFW_KEY_LEFT_ALT] || pressed[GLFW_KEY_RIGHT_ALT];
	anyShiftPressed = pressed[GLFW_KEY_LEFT_SHIFT] || pressed[GLFW_KEY_RIGHT_SHIFT];


	if (canControl)
	{
		if (anyCtrlPressed && (oldPressed[GLFW_KEY_A] || pressed[GLFW_KEY_A]) && released[GLFW_KEY_A]
			&& pickMode == PICK_FACE && selectedFaces.size())
		{
			Bsp* map = getSelectedMap();
			if (map)
			{
				BSPFACE& selface = map->faces[selectedFaces[0]];
				BSPTEXTUREINFO& seltexinfo = map->texinfos[selface.iTextureInfo];
				deselectFaces();
				for (unsigned int i = 0; i < map->faceCount; i++)
				{
					BSPFACE& face = map->faces[i];
					BSPTEXTUREINFO& texinfo = map->texinfos[face.iTextureInfo];
					if (texinfo.iMiptex == seltexinfo.iMiptex)
					{
						if (map->getBspRender())
							map->getBspRender()->highlightFace(i, true);
						selectedFaces.push_back(i);
					}
				}
			}
		}

		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		vec2 mousePos((float)xpos, (float)ypos);

		cameraOrigin += getMoveDir() * (float)frameTimeScale;


		moveGrabbedEnt();

		vertexEditControls();

		cameraContextMenus();

		cameraRotationControls(mousePos);

		makeVectors(cameraAngles, cameraForward, cameraRight, cameraUp);

		cameraObjectHovering();

		cameraPickingControls();

		shortcutControls();
	}

	if (!gui->imgui_io->WantTextInput)
	{
		globalShortcutControls();
	}

	oldLeftMouse = curLeftMouse;
	curLeftMouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
	oldRightMouse = curRightMouse;
	curRightMouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);

	for (int i = GLFW_KEY_SPACE; i < GLFW_KEY_LAST; i++)
	{
		oldPressed[i] = pressed[i];
		oldReleased[i] = released[i];
	}

	oldScroll = g_scroll;
}

void Renderer::vertexEditControls()
{
	canTransform = true;

	if (transformTarget == TRANSFORM_VERTEX)
	{
		canTransform = false;
		anyEdgeSelected = false;
		anyVertSelected = false;

		for (int i = 0; i < modelVerts.size(); i++)
		{
			if (modelVerts[i].selected)
			{
				canTransform = true;
				anyVertSelected = true;
				break;
			}
		}

		for (int i = 0; i < modelEdges.size(); i++)
		{
			if (modelEdges[i].selected)
			{
				canTransform = true;
				anyEdgeSelected = true;
			}
		}

	}

	if (!isTransformableSolid)
	{
		canTransform = (transformTarget == TRANSFORM_OBJECT || transformTarget == TRANSFORM_ORIGIN) && transformMode == TRANSFORM_MOVE;
	}


	if (pressed[GLFW_KEY_F] && !oldPressed[GLFW_KEY_F])
	{
		if (!anyCtrlPressed)
		{
			splitModelFace();
		}
		else
		{
			gui->showEntityReport = true;
		}
	}

	if (canTransform)
	{
		canTransform = pickMode == pick_modes::PICK_OBJECT;
	}
}

void Renderer::cameraPickingControls()
{
	if (curLeftMouse == GLFW_PRESS || oldLeftMouse == GLFW_PRESS)
	{
		bool transforming = transformAxisControls();

		bool anyHover = hoverVert != -1 || hoverEdge != -1;
		if (transformTarget == TRANSFORM_VERTEX && isTransformableSolid && anyHover)
		{
			if (oldLeftMouse != GLFW_PRESS)
			{
				if (!anyCtrlPressed)
				{
					for (int i = 0; i < modelEdges.size(); i++)
					{
						modelEdges[i].selected = false;
					}
					for (int i = 0; i < modelVerts.size(); i++)
					{
						modelVerts[i].selected = false;
					}
					anyVertSelected = false;
					anyEdgeSelected = false;
				}

				if (hoverVert != -1 && !anyEdgeSelected)
				{
					modelVerts[hoverVert].selected = !modelVerts[hoverVert].selected;
					anyVertSelected = modelVerts[hoverVert].selected;
				}
				else if (hoverEdge != -1 && !(anyVertSelected && !anyEdgeSelected))
				{
					modelEdges[hoverEdge].selected = !modelEdges[hoverEdge].selected;
					for (int i = 0; i < 2; i++)
					{
						TransformVert& vert = modelVerts[modelEdges[hoverEdge].verts[i]];
						vert.selected = modelEdges[hoverEdge].selected;
					}
					anyEdgeSelected = modelEdges[hoverEdge].selected;
				}

				vertPickCount++;
				applyTransform();
			}

			transforming = true;
		}

		if (transformTarget == TRANSFORM_ORIGIN && originHovered)
		{
			if (oldLeftMouse != GLFW_PRESS)
			{
				originSelected = !originSelected;
			}

			transforming = true;
		}

		// object picking
		if (!transforming && oldLeftMouse != GLFW_PRESS)
		{
			applyTransform();
			Bsp* map = getSelectedMap();
			if (invalidSolid)
			{
				logf("Reverting invalid solid changes\n");
				for (int i = 0; i < modelVerts.size(); i++)
				{
					modelVerts[i].pos = modelVerts[i].startPos = modelVerts[i].undoPos;
				}
				for (int i = 0; i < modelFaceVerts.size(); i++)
				{
					modelFaceVerts[i].pos = modelFaceVerts[i].startPos = modelFaceVerts[i].undoPos;
					if (modelFaceVerts[i].ptr)
					{
						*modelFaceVerts[i].ptr = modelFaceVerts[i].pos;
					}
				}
				invalidSolid = !map->vertex_manipulation_sync(pickInfo.modelIdx, modelVerts, false, true);
				gui->reloadLimits();
				if (pickInfo.entIdx >= 0)
				{
					int modelIdx = map->ents[pickInfo.entIdx]->getBspModelIdx();
					if (modelIdx >= 0)
						map->getBspRender()->refreshModel(modelIdx);
				}
			}

			pickObject();
			pickCount++;
		}
	}
	else
	{ // left mouse not pressed
		pickClickHeld = false;
		if (draggingAxis != -1)
		{
			draggingAxis = -1;
			applyTransform();
			

			if (pickInfo.entIdx >= 0)
			{
				Entity* ent = SelectedMap->ents[pickInfo.entIdx];
				if (ent && undoEntityState->getOrigin() != ent->getOrigin())
				{
					pushEntityUndoState("Move Entity");
				}
			}
		}
	}
}

void Renderer::applyTransform(bool forceUpdate)
{
	Bsp* map = getSelectedMap();

	if (!isTransformableSolid || (modelUsesSharedStructures && (transformMode != TRANSFORM_MOVE || transformTarget == TRANSFORM_VERTEX)))
	{
		return;
	}

	if (pickInfo.modelIdx > 0 && pickMode == PICK_OBJECT)
	{
		bool transformingVerts = transformTarget == TRANSFORM_VERTEX;
		bool scalingObject = transformTarget == TRANSFORM_OBJECT && transformMode == TRANSFORM_SCALE;
		bool movingOrigin = transformTarget == TRANSFORM_ORIGIN;
		bool actionIsUndoable = false;

		bool anyVertsChanged = false;
		for (int i = 0; i < modelVerts.size(); i++)
		{
			if (modelVerts[i].pos != modelVerts[i].startPos || modelVerts[i].pos != modelVerts[i].undoPos)
			{
				anyVertsChanged = true;
			}
		}

		if (anyVertsChanged && (transformingVerts || scalingObject || forceUpdate))
		{

			invalidSolid = !map->vertex_manipulation_sync(pickInfo.modelIdx, modelVerts, false, true);
			gui->reloadLimits();

			for (int i = 0; i < modelVerts.size(); i++)
			{
				modelVerts[i].startPos = modelVerts[i].pos;
				if (!invalidSolid)
				{
					modelVerts[i].undoPos = modelVerts[i].pos;
				}
			}
			for (int i = 0; i < modelFaceVerts.size(); i++)
			{
				modelFaceVerts[i].startPos = modelFaceVerts[i].pos;
				if (!invalidSolid)
				{
					modelFaceVerts[i].undoPos = modelFaceVerts[i].pos;
				}
			}

			if (scalingObject)
			{
				for (int i = 0; i < scaleTexinfos.size(); i++)
				{
					BSPTEXTUREINFO& info = map->texinfos[scaleTexinfos[i].texinfoIdx];
					scaleTexinfos[i].oldShiftS = info.shiftS;
					scaleTexinfos[i].oldShiftT = info.shiftT;
					scaleTexinfos[i].oldS = info.vS;
					scaleTexinfos[i].oldT = info.vT;
				}
			}

			actionIsUndoable = !invalidSolid;
		}

		if (movingOrigin && pickInfo.modelIdx >= 0)
		{
			if (oldOrigin != transformedOrigin)
			{
				vec3 delta = transformedOrigin - oldOrigin;

				g_progress.hide = true;
				map->move(delta * -1, pickInfo.modelIdx);
				g_progress.hide = false;

				oldOrigin = transformedOrigin;
				map->getBspRender()->refreshModel(pickInfo.modelIdx);

				for (int i = 0; i < map->ents.size(); i++)
				{
					Entity* ent = map->ents[i];
					if (ent->getBspModelIdx() == pickInfo.modelIdx)
					{
						ent->setOrAddKeyvalue("origin", (ent->getOrigin() + delta).toKeyvalueString());
						map->getBspRender()->refreshEnt(i);
					}
				}

				updateModelVerts();
				//map->getBspRender()->reloadLightmaps();

				actionIsUndoable = true;
			}
		}

		if (actionIsUndoable)
		{
			pushModelUndoState("Edit BSP Model", EDIT_MODEL_LUMPS);
		}
	}
}

void Renderer::cameraRotationControls(vec2 mousePos)
{
// camera rotation
	if (draggingAxis == -1 && curRightMouse == GLFW_PRESS)
	{
		if (!cameraIsRotating)
		{
			lastMousePos = mousePos;
			cameraIsRotating = true;
			totalMouseDrag = vec2();
		}
		else
		{
			vec2 drag = mousePos - lastMousePos;
			cameraAngles.z += drag.x * rotationSpeed * 0.1f;
			cameraAngles.x += drag.y * rotationSpeed * 0.1f;

			totalMouseDrag += vec2(abs(drag.x), abs(drag.y));

			cameraAngles.x = clamp(cameraAngles.x, -90.0f, 90.0f);
			if (cameraAngles.z > 180.0f)
			{
				cameraAngles.z -= 360.0f;
			}
			else if (cameraAngles.z < -180.0f)
			{
				cameraAngles.z += 360.0f;
			}
			lastMousePos = mousePos;
		}

		ImGui::SetWindowFocus(NULL);
		ImGui::ClearActiveID();
	}
	else
	{
		cameraIsRotating = false;
		totalMouseDrag = vec2();
	}
}

void Renderer::cameraObjectHovering()
{
	originHovered = false;
	Bsp* map = getSelectedMap();
	if (!map || (modelUsesSharedStructures && (transformMode != TRANSFORM_MOVE || transformTarget == TRANSFORM_VERTEX)))
		return;

	vec3 mapOffset;
	if (map->getBspRender())
		mapOffset = map->getBspRender()->mapOffset;

	if (transformTarget == TRANSFORM_VERTEX && pickInfo.entIdx > 0)
	{
		vec3 pickStart, pickDir;
		getPickRay(pickStart, pickDir);
		PickInfo vertPick = PickInfo();
		vertPick.bestDist = FLT_MAX_COORD;

		Entity* ent = SelectedMap->ents[pickInfo.entIdx];
		vec3 entOrigin = ent->getOrigin();

		hoverEdge = -1;
		if (!(anyVertSelected && !anyEdgeSelected))
		{
			for (int i = 0; i < modelEdges.size(); i++)
			{
				vec3 ori = getEdgeControlPoint(modelVerts, modelEdges[i]) + entOrigin + mapOffset;
				float s = (ori - cameraOrigin).length() * vertExtentFactor * 2.0f;
				vec3 min = vec3(-s, -s, -s) + ori;
				vec3 max = vec3(s, s, s) + ori;
				if (pickAABB(pickStart, pickDir, min, max, vertPick.bestDist))
				{
					hoverEdge = i;
				}
			}
		}

		hoverVert = -1;
		if (!anyEdgeSelected)
		{
			for (int i = 0; i < modelVerts.size(); i++)
			{
				vec3 ori = entOrigin + modelVerts[i].pos + mapOffset;
				float s = (ori - cameraOrigin).length() * vertExtentFactor * 2.0f;
				vec3 min = vec3(-s, -s, -s) + ori;
				vec3 max = vec3(s, s, s) + ori;
				if (pickAABB(pickStart, pickDir, min, max, vertPick.bestDist))
				{
					hoverVert = i;
				}
			}
		}
	}

	if (transformTarget == TRANSFORM_ORIGIN && pickInfo.modelIdx > 0)
	{
		vec3 pickStart, pickDir;
		getPickRay(pickStart, pickDir);
		PickInfo vertPick = PickInfo();
		vertPick.bestDist = FLT_MAX_COORD;

		vec3 ori = transformedOrigin + mapOffset;
		float s = (ori - cameraOrigin).length() * vertExtentFactor * 2.0f;
		vec3 min = vec3(-s, -s, -s) + ori;
		vec3 max = vec3(s, s, s) + ori;
		originHovered = pickAABB(pickStart, pickDir, min, max, vertPick.bestDist);
	}

	if (transformTarget == TRANSFORM_VERTEX && transformMode == TRANSFORM_SCALE)
		return; // 3D scaling disabled in vertex edit mode

	// axis handle hovering
	TransformAxes& activeAxes = *(transformMode == TRANSFORM_SCALE ? &scaleAxes : &moveAxes);
	hoverAxis = -1;
	if (showDragAxes && !movingEnt && hoverVert == -1 && hoverEdge == -1)
	{
		vec3 pickStart, pickDir;
		getPickRay(pickStart, pickDir);
		PickInfo axisPick = PickInfo();
		axisPick.bestDist = FLT_MAX_COORD;

		if (map->getBspRender())
		{
			vec3 origin = activeAxes.origin;

			int axisChecks = transformMode == TRANSFORM_SCALE ? activeAxes.numAxes : 3;
			for (int i = 0; i < axisChecks; i++)
			{
				if (pickAABB(pickStart, pickDir, origin + activeAxes.mins[i], origin + activeAxes.maxs[i], axisPick.bestDist))
				{
					hoverAxis = i;
				}
			}

			// center cube gets priority for selection (hard to select from some angles otherwise)
			if (transformMode == TRANSFORM_MOVE)
			{
				float bestDist = FLT_MAX_COORD;
				if (pickAABB(pickStart, pickDir, origin + activeAxes.mins[3], origin + activeAxes.maxs[3], bestDist))
				{
					hoverAxis = 3;
				}
			}
		}
	}
}

void Renderer::cameraContextMenus()
{
// context menus
	bool wasTurning = cameraIsRotating && totalMouseDrag.length() >= 1;
	if (draggingAxis == -1 && curRightMouse == GLFW_RELEASE && oldRightMouse != GLFW_RELEASE && !wasTurning)
	{
		vec3 pickStart, pickDir;
		getPickRay(pickStart, pickDir);

		PickInfo tempPick = PickInfo();
		tempPick.bestDist = FLT_MAX_COORD;

		Bsp* map = getSelectedMap();

		map->getBspRender()->pickPoly(pickStart, pickDir, clipnodeRenderHull, tempPick, &map);

		if (tempPick.entIdx < 0)
		{
			for (int i = 0; i < mapRenderers.size(); i++)
			{
				if (getSelectedMap() == mapRenderers[i]->map->parentMap && mapRenderers[i]->pickPoly(pickStart, pickDir, clipnodeRenderHull, tempPick, &map) && tempPick.entIdx > 0)
				{
					selectMap(map);
					break;
				}
			}
		}

		if (tempPick.entIdx != 0 && tempPick.entIdx == pickInfo.entIdx)
		{
			gui->openContextMenu(pickInfo.entIdx);
		}
		else
		{
			gui->openContextMenu(-1);
		}
	}
}

void Renderer::moveGrabbedEnt()
{
// grabbing
	if (movingEnt && pickInfo.entIdx)
	{
		if (g_scroll != oldScroll)
		{
			float moveScale = pressed[GLFW_KEY_LEFT_SHIFT] ? 4.0f : 2.0f;
			if (pressed[GLFW_KEY_LEFT_CONTROL])
				moveScale = 1.0f;
			if (g_scroll < oldScroll)
				moveScale *= -1;

			grabDist += 16 * moveScale;
		}

		Bsp* map = g_app->getSelectedMap();
		vec3 mapOffset = map->getBspRender()->mapOffset;
		vec3 delta = ((cameraOrigin - mapOffset) + cameraForward * grabDist) - grabStartOrigin;
		Entity* ent = map->ents[pickInfo.entIdx];

		vec3 tmpOrigin = grabStartEntOrigin;
		vec3 offset = getEntOffset(map, ent);
		vec3 newOrigin = (tmpOrigin + delta) - offset;
		vec3 rounded = gridSnappingEnabled ? snapToGrid(newOrigin) : newOrigin;

		transformedOrigin = this->oldOrigin = rounded;

		ent->setOrAddKeyvalue("origin", rounded.toKeyvalueString(!gridSnappingEnabled));
		map->getBspRender()->refreshEnt(pickInfo.entIdx);
		updateEntConnectionPositions();
	}
	else
	{
		ungrabEnt();
	}
}

void Renderer::shortcutControls()
{
	if (pickMode == PICK_OBJECT)
	{
		bool anyEnterPressed = (pressed[GLFW_KEY_ENTER] && !oldPressed[GLFW_KEY_ENTER]) ||
			(pressed[GLFW_KEY_KP_ENTER] && !oldPressed[GLFW_KEY_KP_ENTER]);

		if (pressed[GLFW_KEY_G] == GLFW_PRESS && oldPressed[GLFW_KEY_G] != GLFW_PRESS)
		{
			if (!movingEnt)
				grabEnt();
			else
			{
				ungrabEnt();
			}
		}
		if (anyCtrlPressed && pressed[GLFW_KEY_C] && !oldPressed[GLFW_KEY_C])
		{
			copyEnt();
		}
		if (anyCtrlPressed && pressed[GLFW_KEY_X] && !oldPressed[GLFW_KEY_X])
		{
			cutEnt();
		}
		if (anyCtrlPressed && pressed[GLFW_KEY_V] && !oldPressed[GLFW_KEY_V])
		{
			pasteEnt(false);
		}
		if (anyCtrlPressed && pressed[GLFW_KEY_M] && !oldPressed[GLFW_KEY_M])
		{
			gui->showTransformWidget = !gui->showTransformWidget;
		}
		if (anyCtrlPressed && pressed[GLFW_KEY_G] && !oldPressed[GLFW_KEY_G])
		{
			gui->showGOTOWidget = !gui->showGOTOWidget;
			gui->showGOTOWidget_update = true;
		}
		if (anyAltPressed && anyEnterPressed)
		{
			gui->showKeyvalueWidget = !gui->showKeyvalueWidget;
		}
		if (pressed[GLFW_KEY_DELETE] && !oldPressed[GLFW_KEY_DELETE])
		{
			deleteEnt();
		}
	}
	else if (pickMode == PICK_FACE)
	{
		if (anyCtrlPressed && pressed[GLFW_KEY_C] && !oldPressed[GLFW_KEY_C])
		{
			gui->copyTexture();
		}
		if (anyCtrlPressed && pressed[GLFW_KEY_V] && !oldPressed[GLFW_KEY_V])
		{
			gui->pasteTexture();
		}
	}
}

void Renderer::globalShortcutControls()
{
	if (anyCtrlPressed && pressed[GLFW_KEY_Z] && !oldPressed[GLFW_KEY_Z])
	{
		undo();
	}
	if (anyCtrlPressed && pressed[GLFW_KEY_Y] && !oldPressed[GLFW_KEY_Y])
	{
		redo();
	}
}

void Renderer::pickObject()
{
	if (!getSelectedMap())
		return;
	bool pointEntWasSelected = pickInfo.entIdx >= 0;
	if (pointEntWasSelected)
	{
		Entity* ent = SelectedMap->ents[pickInfo.entIdx];
		pointEntWasSelected = ent && !ent->isBspModel();
	}
	int oldSelectedEntIdx = pickInfo.entIdx;

	Bsp* map = getSelectedMap();
	vec3 pickStart, pickDir;
	getPickRay(pickStart, pickDir);

	int oldEntIdx = pickInfo.entIdx;

	clearSelection();
	selectMap(map);

	pickInfo.bestDist = FLT_MAX_COORD;

	map->getBspRender()->preRenderEnts();
	map->getBspRender()->pickPoly(pickStart, pickDir, clipnodeRenderHull, pickInfo, &map);

	for (int i = 0; i < mapRenderers.size(); i++)
	{
		if (map == mapRenderers[i]->map->parentMap)
		{
			mapRenderers[i]->preRenderEnts();
			mapRenderers[i]->pickPoly(pickStart, pickDir, clipnodeRenderHull, pickInfo,&map);
		}
	}

	selectMap(map);

	if (movingEnt && oldEntIdx != pickInfo.entIdx)
	{
		ungrabEnt();
	}

	if (isTransformableSolid || pickInfo.modelIdx > 0)
	{
//getSelectedMap()->print_model_hull(pickInfo.modelIdx, 0);
	}
	else
	{
		if (transformMode == TRANSFORM_SCALE)
			transformMode = TRANSFORM_MOVE;
		transformTarget = TRANSFORM_OBJECT;
	}

	isTransformableSolid = pickInfo.modelIdx > 0 || pickInfo.entIdx > 0;

	if ((pickMode == PICK_OBJECT || !anyCtrlPressed))
	{
		deselectFaces();
	}

	if (pickMode == PICK_OBJECT)
	{
		updateModelVerts();
	}
	else if (pickMode == PICK_FACE)
	{
		gui->showLightmapEditorUpdate = true;

		if (pickInfo.modelIdx >= 0 && pickInfo.faceIdx >= 0)
		{
			bool select = true;
			for (int i = 0; i < selectedFaces.size(); i++)
			{
				if (selectedFaces[i] == pickInfo.faceIdx)
				{
					select = false;
					selectedFaces.erase(selectedFaces.begin() + i);
					break;
				}
			}
			if (map && map->getBspRender())
				map->getBspRender()->highlightFace(pickInfo.faceIdx, select);

			if (select)
				selectedFaces.push_back(pickInfo.faceIdx);

		}
	}

	if (pointEntWasSelected)
	{
		for (int i = 0; i < mapRenderers.size(); i++)
		{
			mapRenderers[i]->refreshPointEnt(oldSelectedEntIdx);
		}
	}

	pickClickHeld = true;

	updateEntConnections();

	if (SelectedMap && pickInfo.entIdx)
	{
		selectEnt(SelectedMap, pickInfo.entIdx);
	}
}

bool Renderer::transformAxisControls()
{
	TransformAxes& activeAxes = *(transformMode == TRANSFORM_SCALE ? &scaleAxes : &moveAxes);
	Bsp* map = g_app->getSelectedMap();

	if (!isTransformableSolid || !canTransform || pickClickHeld || pickInfo.entIdx < 0 || !map)
	{
		return false;
	}

	// axis handle dragging
	if (showDragAxes && !movingEnt && hoverAxis != -1 && draggingAxis == -1)
	{
		draggingAxis = hoverAxis;

		Entity* ent = map->ents[pickInfo.entIdx];

		axisDragEntOriginStart = getEntOrigin(map, ent);
		axisDragStart = getAxisDragPoint(axisDragEntOriginStart);
	}

	if (showDragAxes && !movingEnt && draggingAxis >= 0)
	{
		Entity* ent = map->ents[pickInfo.entIdx];

		activeAxes.model[draggingAxis].setColor(activeAxes.hoverColor[draggingAxis]);

		vec3 dragPoint = getAxisDragPoint(axisDragEntOriginStart);
		if (gridSnappingEnabled)
		{
			dragPoint = snapToGrid(dragPoint);
		}
		vec3 delta = dragPoint - axisDragStart;
		if (delta.IsZero())
			return false;


		float moveScale = pressed[GLFW_KEY_LEFT_SHIFT] ? 2.0f : 1.0f;
		if (pressed[GLFW_KEY_LEFT_CONTROL] == GLFW_PRESS)
			moveScale = 0.1f;

		float maxDragDist = 8192; // don't throw ents out to infinity
		for (int i = 0; i < 3; i++)
		{
			if (i != draggingAxis % 3)
				((float*)&delta)[i] = 0;
			else
				((float*)&delta)[i] = clamp(((float*)&delta)[i] * moveScale, -maxDragDist, maxDragDist);
		}

		dragDelta = delta;

		if (transformMode == TRANSFORM_MOVE)
		{
			if (transformTarget == TRANSFORM_VERTEX)
			{
				moveSelectedVerts(delta);
				if (curLeftMouse == GLFW_PRESS && oldLeftMouse != GLFW_PRESS)
				{
					pushModelUndoState("Move verts", EDIT_MODEL_LUMPS);
				}
			}
			else if (transformTarget == TRANSFORM_OBJECT)
			{
				if (moveOrigin || ent->getBspModelIdx() < 0)
				{
					vec3 offset = getEntOffset(map, ent);
					vec3 newOrigin = (axisDragEntOriginStart + delta) - offset;
					vec3 rounded = gridSnappingEnabled ? snapToGrid(newOrigin) : newOrigin;

					ent->setOrAddKeyvalue("origin", rounded.toKeyvalueString(!gridSnappingEnabled));
					map->getBspRender()->refreshEnt(pickInfo.entIdx);
					updateEntConnectionPositions();
					if (curLeftMouse == GLFW_PRESS && oldLeftMouse != GLFW_PRESS)
					{
						pushEntityUndoState("Move Entity");
					}
				}
				else
				{
					map->move(delta, ent->getBspModelIdx(), true);
					map->getBspRender()->refreshEnt(pickInfo.entIdx);
					map->getBspRender()->refreshModel(ent->getBspModelIdx());
					updateEntConnectionPositions();
					if (curLeftMouse == GLFW_PRESS && oldLeftMouse != GLFW_PRESS)
					{
						pushModelUndoState("Move Model", EDIT_MODEL_LUMPS | ENTITIES);
					}
				}
			}
			else if (transformTarget == TRANSFORM_ORIGIN)
			{
				transformedOrigin = (oldOrigin + delta);
				transformedOrigin = gridSnappingEnabled ? snapToGrid(transformedOrigin) : transformedOrigin;
				map->getBspRender()->refreshEnt(pickInfo.entIdx);
				updateEntConnectionPositions();
				if (curLeftMouse == GLFW_PRESS && oldLeftMouse != GLFW_PRESS)
				{
					pushEntityUndoState("Move Origin");
				}
			}

		}
		else
		{
			if (ent->isBspModel() && abs(delta.length()) >= EPSILON)
			{
				vec3 scaleDirs[6]{
					vec3(1, 0, 0),
					vec3(0, 1, 0),
					vec3(0, 0, 1),
					vec3(-1, 0, 0),
					vec3(0, -1, 0),
					vec3(0, 0, -1),
				};

				scaleSelectedObject(delta, scaleDirs[draggingAxis]);
				map->getBspRender()->refreshModel(ent->getBspModelIdx());
				if (curLeftMouse == GLFW_PRESS && oldLeftMouse != GLFW_PRESS)
				{
					pushModelUndoState("Scale Model", EDIT_MODEL_LUMPS);
				}
			}
		}

		return true;
	}

	return false;
}

vec3 Renderer::getMoveDir()
{
	mat4x4 rotMat;
	rotMat.loadIdentity();
	rotMat.rotateX(PI * cameraAngles.x / 180.0f);
	rotMat.rotateZ(PI * cameraAngles.z / 180.0f);

	vec3 forward, right, up;
	makeVectors(cameraAngles, forward, right, up);


	vec3 wishdir(0, 0, 0);
	if (pressed[GLFW_KEY_A])
	{
		wishdir -= right;
	}
	if (pressed[GLFW_KEY_D])
	{
		wishdir += right;
	}
	if (pressed[GLFW_KEY_W])
	{
		wishdir += forward;
	}
	if (pressed[GLFW_KEY_S])
	{
		wishdir -= forward;
	}

	wishdir *= moveSpeed;

	if (anyShiftPressed)
		wishdir *= 4.0f;
	if (anyCtrlPressed)
		wishdir *= 0.1f;
	return wishdir;
}

void Renderer::getPickRay(vec3& start, vec3& pickDir)
{
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	// invert ypos
	ypos = windowHeight - ypos;

	// translate mouse coordinates so that the origin lies in the center and is a scaler from +/-1.0
	float mouseX = (((float)xpos / (float)windowWidth) * 2.0f) - 1.0f;
	float mouseY = (((float)ypos / (float)windowHeight) * 2.0f) - 1.0f;

	// http://schabby.de/picking-opengl-ray-tracing/
	vec3 forward, right, up;
	makeVectors(cameraAngles, forward, right, up);

	vec3 tview = forward.normalize(1.0f);
	vec3 h = crossProduct(tview, up).normalize(1.0f); // 3D float std::vector
	vec3 v = crossProduct(h, tview).normalize(1.0f); // 3D float std::vector

	// convert fovy to radians 
	float rad = fov * (PI / 180.0f);
	float vLength = tan(rad / 2.0f) * zNear;
	float hLength = vLength * (windowWidth / (float)windowHeight);

	v *= vLength;
	h *= hLength;

	// linear combination to compute intersection of picking ray with view port plane
	start = cameraOrigin + tview * zNear + h * mouseX + v * mouseY;

	// compute direction of picking ray by subtracting intersection point with camera position
	pickDir = (start - cameraOrigin).normalize(1.0f);
}

Bsp* Renderer::getSelectedMap()
{
// auto select if one map
	if (!SelectedMap && mapRenderers.size() == 1)
	{
		SelectedMap = mapRenderers[0]->map;
	}

	return SelectedMap;
}

int Renderer::getSelectedMapId()
{
	for (int i = 0; i < mapRenderers.size(); i++)
	{
		BspRenderer* s = mapRenderers[i];
		if (s->map && s->map == getSelectedMap())
		{
			return i;
		}
	}
	return -1;
}

void Renderer::selectMapId(int id)
{
	for (int i = 0; i < mapRenderers.size(); i++)
	{
		BspRenderer* s = mapRenderers[i];
		if (s->map)
		{
			SelectedMap = s->map;
			return;
		}
	}
	SelectedMap = NULL;
}

void Renderer::selectMap(Bsp* map)
{
	SelectedMap = map;
}

void Renderer::deselectMap(Bsp* map)
{
	SelectedMap = NULL;
}

void Renderer::clearSelection()
{
	pickInfo = PickInfo();
}

BspRenderer* Renderer::getMapContainingCamera()
{
	for (int i = 0; i < mapRenderers.size(); i++)
	{
		Bsp* map = mapRenderers[i]->map;

		vec3 mins, maxs;
		map->get_bounding_box(mins, maxs);

		if (cameraOrigin.x > mins.x && cameraOrigin.y > mins.y && cameraOrigin.z > mins.z &&
			cameraOrigin.x < maxs.x && cameraOrigin.y < maxs.y && cameraOrigin.z < maxs.z)
		{
			return map->getBspRender();
		}
	}

	return NULL;
}

void Renderer::setupView()
{
	glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

	glViewport(0, 0, windowWidth, windowHeight);

	projection.perspective(fov, (float)windowWidth / (float)windowHeight, zNear, zFar);

	matview.loadIdentity();
	matview.rotateX(PI * cameraAngles.x / 180.0f);
	matview.rotateY(PI * cameraAngles.z / 180.0f);
	matview.translate(-cameraOrigin.x, -cameraOrigin.z, cameraOrigin.y);
}

void Renderer::reloadBspModels()
{
	isModelsReloading = true;

	if (!mapRenderers.size())
	{
		isModelsReloading = false;
		return;
	}

	int modelcount = 0;

	for (int i = 0; i < mapRenderers.size(); i++)
	{
		if (mapRenderers[i]->map->is_model)
		{
			modelcount++;
		}
	}

	if (modelcount == mapRenderers.size())
	{
		isModelsReloading = false;
		return;
	}

	std::vector<BspRenderer*> sorted_renders;

	for (int i = 0; i < mapRenderers.size(); i++)
	{
		if (!mapRenderers[i]->map->is_model)
		{
			sorted_renders.push_back(mapRenderers[i]);
		}
		else
		{
			delete mapRenderers[i];
		}
	}

	mapRenderers = std::move(sorted_renders);

	std::vector<std::string> tryPaths = {
		"./"
	};

	tryPaths.insert(tryPaths.end(), g_settings.resPaths.begin(), g_settings.resPaths.end());

	for (auto bsprend : mapRenderers)
	{
		if (bsprend)
		{
			for (auto const& entity : bsprend->map->ents)
			{
				if (entity->hasKey("model"))
				{
					std::string modelPath = entity->keyvalues["model"];
					if (modelPath.find(".bsp") != std::string::npos)
					{
						for (int i = 0; i < tryPaths.size(); i++)
						{
							std::string tryPath = tryPaths[i] + modelPath;
							if (!fileExists(tryPath))
								tryPath = g_settings.gamedir + tryPaths[i] + modelPath;
							if (fileExists(tryPath))
							{
								Bsp* tmpBsp = new Bsp(tryPath);
								tmpBsp->is_model = true;
								tmpBsp->parentMap = bsprend->map;
								if (tmpBsp->bsp_valid)
								{
									BspRenderer* mapRenderer = new BspRenderer(tmpBsp, bspShader, fullBrightBspShader, colorShader, pointEntRenderer);
									mapRenderers.push_back(mapRenderer);
								}
								break;
							}
						}
					}
				}
			}
		}
	}

	isModelsReloading = false;
}

void Renderer::addMap(Bsp* map)
{
	if (!map->bsp_valid)
	{
		logf("Invalid map!\n");
		return;
	}

	if (!map->is_model)
	{
		clearSelection();
		selectMap(map);
	}

	BspRenderer* mapRenderer = new BspRenderer(map, bspShader, fullBrightBspShader, colorShader, pointEntRenderer);

	mapRenderers.push_back(mapRenderer);

	gui->checkValidHulls();

	// Pick default map
	if (!getSelectedMap())
	{
		clearSelection();
		selectMap(map);
		/*
		* TODO: move camera to center of map
		// Move camera to first entity with origin
		for(auto const & ent : map->ents)
		{
			if (ent->getOrigin() != vec3())
			{
				cameraOrigin = ent->getOrigin();
				break;
			}
		}
		*/
	}
}

void Renderer::drawLine(const vec3& start, const vec3& end, COLOR4 color)
{
	line_verts[0].x = start.x;
	line_verts[0].y = start.z;
	line_verts[0].z = -start.y;
	line_verts[0].c = color;

	line_verts[1].x = end.x;
	line_verts[1].y = end.z;
	line_verts[1].z = -end.y;
	line_verts[1].c = color;

	lineBuf->drawFull();
}

void Renderer::drawPlane(BSPPLANE& plane, COLOR4 color)
{

	vec3 ori = plane.vNormal * plane.fDist;
	vec3 crossDir = abs(plane.vNormal.z) > 0.9f ? vec3(1, 0, 0) : vec3(0, 0, 1);
	vec3 right = crossProduct(plane.vNormal, crossDir);
	vec3 up = crossProduct(right, plane.vNormal);

	float s = 100.0f;

	vec3 topLeft = vec3(ori + right * -s + up * s).flip();
	vec3 topRight = vec3(ori + right * s + up * s).flip();
	vec3 bottomLeft = vec3(ori + right * -s + up * -s).flip();
	vec3 bottomRight = vec3(ori + right * s + up * -s).flip();

	cVert topLeftVert(topLeft, color);
	cVert topRightVert(topRight, color);
	cVert bottomLeftVert(bottomLeft, color);
	cVert bottomRightVert(bottomRight, color);

	plane_verts->v1 = bottomRightVert;
	plane_verts->v2 = bottomLeftVert;
	plane_verts->v3 = topLeftVert;
	plane_verts->v4 = topRightVert;

	planeBuf->drawFull();
}

void Renderer::drawClipnodes(Bsp* map, int iNode, int& currentPlane, int activePlane)
{
	if (iNode == -1)
		return;
	BSPCLIPNODE& node = map->clipnodes[iNode];

	if (currentPlane == activePlane)
		drawPlane(map->planes[node.iPlane], {255, 255, 255, 255});
	currentPlane++;

	for (int i = 0; i < 2; i++)
	{
		if (node.iChildren[i] >= 0)
		{
			drawClipnodes(map, node.iChildren[i], currentPlane, activePlane);
		}
	}
}

void Renderer::drawNodes(Bsp* map, int iNode, int& currentPlane, int activePlane)
{
	if (iNode == -1)
		return;
	BSPNODE& node = map->nodes[iNode];

	if (currentPlane == activePlane)
		drawPlane(map->planes[node.iPlane], {255, 128, 128, 255});
	currentPlane++;

	for (int i = 0; i < 2; i++)
	{
		if (node.iChildren[i] >= 0)
		{
			drawNodes(map, node.iChildren[i], currentPlane, activePlane);
		}
	}
}

vec3 Renderer::getEntOrigin(Bsp* map, Entity* ent)
{
	vec3 origin = ent->hasKey("origin") ? parseVector(ent->keyvalues["origin"]) : vec3(0, 0, 0);
	return origin + getEntOffset(map, ent);
}

vec3 Renderer::getEntOffset(Bsp* map, Entity* ent)
{
	if (ent->isBspModel())
	{
		BSPMODEL& tmodel = map->models[ent->getBspModelIdx()];
		return tmodel.nMins + (tmodel.nMaxs - tmodel.nMins) * 0.5f;
	}
	return vec3(0, 0, 0);
}

void Renderer::updateDragAxes(vec3 delta)
{
	Bsp* map = g_app->getSelectedMap();
	Entity* ent = NULL;
	vec3 mapOffset;

	if (map && map->getBspRender() && pickInfo.entIdx >= 0)
	{
		ent = map->ents[pickInfo.entIdx];
		mapOffset = map->getBspRender()->mapOffset;
	}
	else
	{
		return;
	}

	vec3 localCameraOrigin = cameraOrigin - mapOffset;

	vec3 entMin, entMax;
	// set origin of the axes
	if (transformMode == TRANSFORM_SCALE)
	{
		if (ent && ent->isBspModel())
		{

			map->get_model_vertex_bounds(ent->getBspModelIdx(), entMin, entMax);
			vec3 modelOrigin = entMin + (entMax - entMin) * 0.5f;

			entMax -= modelOrigin;
			entMin -= modelOrigin;

			scaleAxes.origin = modelOrigin;
			if (ent->hasKey("origin"))
			{
				scaleAxes.origin += parseVector(ent->keyvalues["origin"]);
			}
			scaleAxes.origin += delta;
		}
	}
	else
	{
		if (ent)
		{
			if (transformTarget == TRANSFORM_ORIGIN)
			{
				moveAxes.origin = transformedOrigin;
				moveAxes.origin += delta;
				debugVec0 = transformedOrigin + delta;
			}
			else
			{
				moveAxes.origin = getEntOrigin(map, ent);
				moveAxes.origin += delta;
			}
		}

		if (pickInfo.entIdx <= 0)
		{
			moveAxes.origin -= mapOffset;
		}

		if (transformTarget == TRANSFORM_VERTEX)
		{
			vec3 entOrigin = ent ? ent->getOrigin() : vec3();
			vec3 min(FLT_MAX_COORD, FLT_MAX_COORD, FLT_MAX_COORD);
			vec3 max(FLT_MIN_COORD, FLT_MIN_COORD, FLT_MIN_COORD);
			int selectTotal = 0;
			for (int i = 0; i < modelVerts.size(); i++)
			{
				if (modelVerts[i].selected)
				{
					vec3 v = modelVerts[i].pos + entOrigin;
					if (v.x < min.x) min.x = v.x;
					if (v.y < min.y) min.y = v.y;
					if (v.z < min.z) min.z = v.z;
					if (v.x > max.x) max.x = v.x;
					if (v.y > max.y) max.y = v.y;
					if (v.z > max.z) max.z = v.z;
					selectTotal++;
				}
			}
			if (selectTotal != 0)
			{
				moveAxes.origin = min + (max - min) * 0.5f;
				moveAxes.origin += delta;
			}
		}
	}

	TransformAxes& activeAxes = *(transformMode == TRANSFORM_SCALE ? &scaleAxes : &moveAxes);

	float baseScale = (activeAxes.origin - localCameraOrigin).length() * 0.005f;
	float s = baseScale;
	float s2 = baseScale * 2;
	float d = baseScale * 32;

	// create the meshes
	if (transformMode == TRANSFORM_SCALE)
	{
		vec3 axisMins[6] = {
			vec3(0, -s, -s) + vec3(entMax.x,0,0), // x+
			vec3(-s, 0, -s) + vec3(0,entMax.y,0), // y+
			vec3(-s, -s, 0) + vec3(0,0,entMax.z), // z+

			vec3(-d, -s, -s) + vec3(entMin.x,0,0), // x-
			vec3(-s, -d, -s) + vec3(0,entMin.y,0), // y-
			vec3(-s, -s, -d) + vec3(0,0,entMin.z)  // z-
		};
		vec3 axisMaxs[6] = {
			vec3(d, s, s) + vec3(entMax.x,0,0), // x+
			vec3(s, d, s) + vec3(0,entMax.y,0), // y+
			vec3(s, s, d) + vec3(0,0,entMax.z), // z+

			vec3(0, s, s) + vec3(entMin.x,0,0), // x-
			vec3(s, 0, s) + vec3(0,entMin.y,0), // y-
			vec3(s, s, 0) + vec3(0,0,entMin.z)  // z-
		};

		scaleAxes.model[0] = cCube(axisMins[0], axisMaxs[0], scaleAxes.dimColor[0]);
		scaleAxes.model[1] = cCube(axisMins[1], axisMaxs[1], scaleAxes.dimColor[1]);
		scaleAxes.model[2] = cCube(axisMins[2], axisMaxs[2], scaleAxes.dimColor[2]);

		scaleAxes.model[3] = cCube(axisMins[3], axisMaxs[3], scaleAxes.dimColor[3]);
		scaleAxes.model[4] = cCube(axisMins[4], axisMaxs[4], scaleAxes.dimColor[4]);
		scaleAxes.model[5] = cCube(axisMins[5], axisMaxs[5], scaleAxes.dimColor[5]);

		// flip to HL coords
		cVert* verts = (cVert*)scaleAxes.model;
		for (int i = 0; i < 6 * 6 * 6; i++)
		{
			float tmp = verts[i].z;
			verts[i].z = -verts[i].y;
			verts[i].y = tmp;
		}

		// larger mins/maxs so you can be less precise when selecting them
		s *= 4;
		vec3 grabAxisMins[6] = {
			vec3(0, -s, -s) + vec3(entMax.x,0,0), // x+
			vec3(-s, 0, -s) + vec3(0,entMax.y,0), // y+
			vec3(-s, -s, 0) + vec3(0,0,entMax.z), // z+

			vec3(-d, -s, -s) + vec3(entMin.x,0,0), // x-
			vec3(-s, -d, -s) + vec3(0,entMin.y,0), // y-
			vec3(-s, -s, -d) + vec3(0,0,entMin.z)  // z-
		};
		vec3 grabAxisMaxs[6] = {
			vec3(d, s, s) + vec3(entMax.x,0,0), // x+
			vec3(s, d, s) + vec3(0,entMax.y,0), // y+
			vec3(s, s, d) + vec3(0,0,entMax.z), // z+

			vec3(0, s, s) + vec3(entMin.x,0,0), // x-
			vec3(s, 0, s) + vec3(0,entMin.y,0), // y-
			vec3(s, s, 0) + vec3(0,0,entMin.z)  // z-
		};

		for (int i = 0; i < 6; i++)
		{
			scaleAxes.mins[i] = grabAxisMins[i];
			scaleAxes.maxs[i] = grabAxisMaxs[i];
		}
	}
	else
	{
  // flipped for HL coords
		moveAxes.model[0] = cCube(vec3(0, -s, -s), vec3(d, s, s), moveAxes.dimColor[0]);
		moveAxes.model[2] = cCube(vec3(-s, 0, -s), vec3(s, d, s), moveAxes.dimColor[2]);
		moveAxes.model[1] = cCube(vec3(-s, -s, 0), vec3(s, s, -d), moveAxes.dimColor[1]);
		moveAxes.model[3] = cCube(vec3(-s2, -s2, -s2), vec3(s2, s2, s2), moveAxes.dimColor[3]);

		// larger mins/maxs so you can be less precise when selecting them
		s *= 4;
		s2 *= 1.5f;

		activeAxes.mins[0] = vec3(0, -s, -s);
		activeAxes.mins[1] = vec3(-s, 0, -s);
		activeAxes.mins[2] = vec3(-s, -s, 0);
		activeAxes.mins[3] = vec3(-s2, -s2, -s2);

		activeAxes.maxs[0] = vec3(d, s, s);
		activeAxes.maxs[1] = vec3(s, d, s);
		activeAxes.maxs[2] = vec3(s, s, d);
		activeAxes.maxs[3] = vec3(s2, s2, s2);
	}


	if (draggingAxis >= 0 && draggingAxis < activeAxes.numAxes)
	{
		activeAxes.model[draggingAxis].setColor(activeAxes.hoverColor[draggingAxis]);
	}
	else if (hoverAxis >= 0 && hoverAxis < activeAxes.numAxes)
	{
		activeAxes.model[hoverAxis].setColor(activeAxes.hoverColor[hoverAxis]);
	}
	else if (gui->guiHoverAxis >= 0 && gui->guiHoverAxis < activeAxes.numAxes)
	{
		activeAxes.model[gui->guiHoverAxis].setColor(activeAxes.hoverColor[gui->guiHoverAxis]);
	}

	activeAxes.origin += mapOffset;
}

vec3 Renderer::getAxisDragPoint(vec3 origin)
{
	vec3 pickStart, pickDir;
	getPickRay(pickStart, pickDir);

	vec3 axisNormals[3] = {
		vec3(1,0,0),
		vec3(0,1,0),
		vec3(0,0,1)
	};

	// get intersection points between the pick ray and each each movement direction plane
	float dots[3];
	for (int i = 0; i < 3; i++)
	{
		dots[i] = abs(dotProduct(cameraForward, axisNormals[i]));
	}

	// best movement planee is most perpindicular to the camera direction
	// and ignores the plane being moved
	int bestMovementPlane = 0;
	switch (draggingAxis % 3)
	{
		case 0: bestMovementPlane = dots[1] > dots[2] ? 1 : 2; break;
		case 1: bestMovementPlane = dots[0] > dots[2] ? 0 : 2; break;
		case 2: bestMovementPlane = dots[1] > dots[0] ? 1 : 0; break;
	}

	float fDist = ((float*)&origin)[bestMovementPlane];
	float intersectDist;
	rayPlaneIntersect(pickStart, pickDir, axisNormals[bestMovementPlane], fDist, intersectDist);

	// don't let ents zoom out to infinity
	if (intersectDist < 0)
	{
		intersectDist = 0;
	}

	return pickStart + pickDir * intersectDist;
}

void Renderer::updateModelVerts()
{

	if (modelVertBuff)
	{
		delete modelVertBuff;
		delete[] modelVertCubes;
		modelVertBuff = NULL;
		modelVertCubes = NULL;
		modelOriginBuff = NULL;
		scaleTexinfos.clear();
		modelEdges.clear();
		modelVerts.clear();
		modelFaceVerts.clear();
	}

	if (pickInfo.modelIdx <= 0)
	{
		originSelected = false;
		modelUsesSharedStructures = false;
		updateSelectionSize();
		return;
	}

	Bsp* map = g_app->getSelectedMap();
	int modelIdx = map->ents[pickInfo.entIdx]->getBspModelIdx();

	if (modelOriginBuff)
	{
		delete modelOriginBuff;
	}


	Entity* ent = map->ents[pickInfo.entIdx];

	if (ent)
	{
		transformedOrigin = oldOrigin = ent->getOrigin();
	}

	modelOriginBuff = new VertexBuffer(colorShader, COLOR_4B | POS_3F, &modelOriginCube, 6 * 6, GL_TRIANGLES);


	modelUsesSharedStructures = modelIdx >= 0 && map->does_model_use_shared_structures(modelIdx);

	updateSelectionSize();

	if (!map->is_convex(modelIdx))
	{
		return;
	}

	scaleTexinfos = map->getScalableTexinfos(modelIdx);
	map->getModelPlaneIntersectVerts(pickInfo.modelIdx, modelVerts); // for vertex manipulation + scaling
	modelFaceVerts = map->getModelVerts(pickInfo.modelIdx); // for scaling only

	Solid modelSolid;
	if (!getModelSolid(modelVerts, map, modelSolid))
	{
		modelVerts.clear();
		modelFaceVerts.clear();
		scaleTexinfos.clear();
		return;
	};
	modelEdges = modelSolid.hullEdges;

	size_t numCubes = modelVerts.size() + modelEdges.size();
	modelVertCubes = new cCube[numCubes];
	modelVertBuff = new VertexBuffer(colorShader, COLOR_4B | POS_3F, modelVertCubes, (int)(6 * 6 * numCubes), GL_TRIANGLES);
	//logf("%d intersection points\n", modelVerts.size());
}

void Renderer::updateSelectionSize()
{
	selectionSize = vec3();
	Bsp* map = getSelectedMap();
	if (!map)
	{
		return;
	}

	if (pickInfo.modelIdx == 0)
	{
		vec3 mins, maxs;
		map->get_bounding_box(mins, maxs);
		selectionSize = maxs - mins;
	}
	else if (pickInfo.modelIdx > 0)
	{
		vec3 mins, maxs;
		if (map->models[pickInfo.modelIdx].nFaces == 0)
		{
			mins = map->models[pickInfo.modelIdx].nMins;
			maxs = map->models[pickInfo.modelIdx].nMaxs;
		}
		else
		{
			map->get_model_vertex_bounds(pickInfo.modelIdx, mins, maxs);
		}
		selectionSize = maxs - mins;
	}
	else if (pickInfo.entIdx >= 0)
	{
		Entity* ent = map->ents[pickInfo.entIdx];
		EntCube* cube = pointEntRenderer->getEntCube(ent);
		if (cube)
			selectionSize = cube->maxs - cube->mins;
	}
}

void Renderer::updateEntConnections()
{
	if (entConnections)
	{
		delete entConnections;
		delete entConnectionPoints;
		entConnections = NULL;
		entConnectionPoints = NULL;
	}

	Bsp* map = getSelectedMap();

	if (!(g_render_flags & RENDER_ENT_CONNECTIONS))
	{
		return;
	}

	if (map && pickInfo.entIdx >= 0)
	{
		Entity* ent = map->ents[pickInfo.entIdx];
		std::vector<std::string> targetNames = ent->getTargets();
		std::vector<Entity*> targets;
		std::vector<Entity*> callers;
		std::vector<Entity*> callerAndTarget; // both a target and a caller
		std::string thisName;
		if (ent->hasKey("targetname"))
		{
			thisName = ent->keyvalues["targetname"];
		}

		for (int k = 0; k < map->ents.size(); k++)
		{
			ent = map->ents[k];

			if (k == pickInfo.entIdx)
				continue;

			bool isTarget = false;
			if (ent->hasKey("targetname"))
			{
				std::string tname = ent->keyvalues["targetname"];
				for (int i = 0; i < targetNames.size(); i++)
				{
					if (tname == targetNames[i])
					{
						isTarget = true;
						break;
					}
				}
			}

			bool isCaller = thisName.length() && ent->hasTarget(thisName);

			if (isTarget && isCaller)
			{
				callerAndTarget.push_back(ent);
			}
			else if (isTarget)
			{
				targets.push_back(ent);
			}
			else if (isCaller)
			{
				callers.push_back(ent);
			}
		}

		if (targets.empty() && callers.empty() && callerAndTarget.empty())
		{
			return;
		}

		size_t numVerts = targets.size() * 2 + callers.size() * 2 + callerAndTarget.size() * 2;
		size_t numPoints = callers.size() + targets.size() + callerAndTarget.size();
		cVert* lines = new cVert[numVerts];
		cCube* points = new cCube[numPoints];

		const COLOR4 targetColor = {255, 255, 0, 255};
		const COLOR4 callerColor = {0, 255, 255, 255};
		const COLOR4 bothColor = {0, 255, 0, 255};

		vec3 srcPos = getEntOrigin(map, ent).flip();
		int idx = 0;
		int cidx = 0;
		float s = 1.5f;
		vec3 extent = vec3(s, s, s);

		for (size_t i = 0; i < targets.size(); i++)
		{
			vec3 ori = getEntOrigin(map, targets[i]).flip();
			points[cidx++] = cCube(ori - extent, ori + extent, targetColor);
			lines[idx++] = cVert(srcPos, targetColor);
			lines[idx++] = cVert(ori, targetColor);
		}
		for (size_t i = 0; i < callers.size(); i++)
		{
			vec3 ori = getEntOrigin(map, callers[i]).flip();
			points[cidx++] = cCube(ori - extent, ori + extent, callerColor);
			lines[idx++] = cVert(srcPos, callerColor);
			lines[idx++] = cVert(ori, callerColor);
		}
		for (size_t i = 0; i < callerAndTarget.size() && cidx < numPoints && idx < numVerts; i++)
		{
			vec3 ori = getEntOrigin(map, callerAndTarget[i]).flip();
			points[cidx++] = cCube(ori - extent, ori + extent, bothColor);
			lines[idx++] = cVert(srcPos, bothColor);
			lines[idx++] = cVert(ori, bothColor);
		}

		entConnections = new VertexBuffer(colorShader, COLOR_4B | POS_3F, lines, (int)numVerts, GL_LINES);
		entConnectionPoints = new VertexBuffer(colorShader, COLOR_4B | POS_3F, points, (int)(numPoints * 6 * 6), GL_TRIANGLES);
		entConnections->ownData = true;
		entConnectionPoints->ownData = true;
	}
}

void Renderer::updateEntConnectionPositions()
{
	if (entConnections && pickInfo.entIdx)
	{
		Entity* ent = SelectedMap->ents[pickInfo.entIdx];
		vec3 pos = getEntOrigin(getSelectedMap(), ent).flip();

		cVert* verts = (cVert*)entConnections->data;
		for (int i = 0; i < entConnections->numVerts; i += 2)
		{
			verts[i].x = pos.x;
			verts[i].y = pos.y;
			verts[i].z = pos.z;
		}
	}
}

bool Renderer::getModelSolid(std::vector<TransformVert>& hullVerts, Bsp* map, Solid& outSolid)
{
	outSolid.faces.clear();
	outSolid.hullEdges.clear();
	outSolid.hullVerts.clear();
	outSolid.hullVerts = hullVerts;

	// get verts for each plane
	std::map<int, std::vector<int>> planeVerts;
	for (int i = 0; i < hullVerts.size(); i++)
	{
		for (int k = 0; k < hullVerts[i].iPlanes.size(); k++)
		{
			int iPlane = hullVerts[i].iPlanes[k];
			planeVerts[iPlane].push_back(i);
		}
	}

	vec3 centroid = getCentroid(hullVerts);

	// sort verts CCW on each plane to get edges
	for (auto it = planeVerts.begin(); it != planeVerts.end(); ++it)
	{
		int iPlane = it->first;
		std::vector<int> verts = it->second;
		BSPPLANE& plane = map->planes[iPlane];
		if (verts.size() < 2)
		{
			logf("Plane with less than 2 verts!?\n"); // hl_c00 pipe in green water place
			return false;
		}

		std::vector<vec3> tempVerts(verts.size());
		for (int i = 0; i < verts.size(); i++)
		{
			tempVerts[i] = hullVerts[verts[i]].pos;
		}

		std::vector<int> orderedVerts = getSortedPlanarVertOrder(tempVerts);
		for (int i = 0; i < orderedVerts.size(); i++)
		{
			orderedVerts[i] = verts[orderedVerts[i]];
			tempVerts[i] = hullVerts[orderedVerts[i]].pos;
		}

		Face face;
		face.plane = plane;

		vec3 orderedVertsNormal = getNormalFromVerts(tempVerts);

		// get plane normal, flipping if it points inside the solid
		vec3 faceNormal = plane.vNormal;
		vec3 planeDir = ((plane.vNormal * plane.fDist) - centroid).normalize();
		face.planeSide = 1;
		if (dotProduct(planeDir, plane.vNormal) > 0)
		{
			faceNormal = faceNormal.invert();
			face.planeSide = 0;
		}

		// reverse vert order if not CCW when viewed from outside the solid
		if (dotProduct(orderedVertsNormal, faceNormal) < 0)
		{
			reverse(orderedVerts.begin(), orderedVerts.end());
		}

		for (int i = 0; i < orderedVerts.size(); i++)
		{
			face.verts.push_back(orderedVerts[i]);
		}
		face.iTextureInfo = 1; // TODO
		outSolid.faces.push_back(face);

		for (int i = 0; i < orderedVerts.size(); i++)
		{
			HullEdge edge;
			edge.verts[0] = orderedVerts[i];
			edge.verts[1] = orderedVerts[(i + 1) % orderedVerts.size()];
			edge.selected = false;

			// find the planes that this edge joins
			vec3 midPoint = getEdgeControlPoint(hullVerts, edge);
			int planeCount = 0;
			for (auto it2 = planeVerts.begin(); it2 != planeVerts.end(); ++it2)
			{
				int iPlane2 = it2->first;
				BSPPLANE& p = map->planes[iPlane2];
				float dist = dotProduct(midPoint, p.vNormal) - p.fDist;
				if (abs(dist) < EPSILON)
				{
					edge.planes[planeCount % 2] = iPlane2;
					planeCount++;
				}
			}
			if (planeCount != 2)
			{
				logf("ERROR: Edge connected to %d planes!\n", planeCount);
				return false;
			}

			outSolid.hullEdges.push_back(edge);
		}
	}

	return true;
}

void Renderer::scaleSelectedObject(float x, float y, float z)
{
	vec3 minDist;
	vec3 maxDist;

	for (int i = 0; i < modelVerts.size(); i++)
	{
		vec3 v = modelVerts[i].startPos;
		if (v.x > maxDist.x) maxDist.x = v.x;
		if (v.x < minDist.x) minDist.x = v.x;

		if (v.y > maxDist.y) maxDist.y = v.y;
		if (v.y < minDist.y) minDist.y = v.y;

		if (v.z > maxDist.z) maxDist.z = v.z;
		if (v.z < minDist.z) minDist.z = v.z;
	}
	vec3 distRange = maxDist - minDist;

	vec3 dir;
	dir.x = (distRange.x * x) - distRange.x;
	dir.y = (distRange.y * y) - distRange.y;
	dir.z = (distRange.z * z) - distRange.z;

	scaleSelectedObject(dir, vec3());
}

void Renderer::scaleSelectedObject(vec3 dir, const vec3& fromDir)
{
	if (pickInfo.modelIdx <= 0)
		return;

	Bsp* map = g_app->getSelectedMap();

	bool scaleFromOrigin = abs(fromDir.x) < EPSILON && abs(fromDir.y) < EPSILON && abs(fromDir.z) < EPSILON;

	vec3 minDist = vec3(FLT_MAX_COORD, FLT_MAX_COORD, FLT_MAX_COORD);
	vec3 maxDist = vec3(FLT_MIN_COORD, FLT_MIN_COORD, FLT_MIN_COORD);

	for (int i = 0; i < modelVerts.size(); i++)
	{
		expandBoundingBox(modelVerts[i].startPos, minDist, maxDist);
	}
	for (int i = 0; i < modelFaceVerts.size(); i++)
	{
		expandBoundingBox(modelFaceVerts[i].startPos, minDist, maxDist);
	}

	vec3 distRange = maxDist - minDist;

	vec3 scaleFromDist = minDist;
	if (scaleFromOrigin)
	{
		scaleFromDist = minDist + (maxDist - minDist) * 0.5f;
	}
	else
	{
		if (fromDir.x < 0)
		{
			scaleFromDist.x = maxDist.x;
			dir.x = -dir.x;
		}
		if (fromDir.y < 0)
		{
			scaleFromDist.y = maxDist.y;
			dir.y = -dir.y;
		}
		if (fromDir.z < 0)
		{
			scaleFromDist.z = maxDist.z;
			dir.z = -dir.z;
		}
	}

	// scale planes
	for (int i = 0; i < modelVerts.size(); i++)
	{
		vec3 stretchFactor = (modelVerts[i].startPos - scaleFromDist) / distRange;
		modelVerts[i].pos = modelVerts[i].startPos + dir * stretchFactor;
		if (gridSnappingEnabled)
		{
			modelVerts[i].pos = snapToGrid(modelVerts[i].pos);
		}
	}

	// scale visible faces
	for (int i = 0; i < modelFaceVerts.size(); i++)
	{
		vec3 stretchFactor = (modelFaceVerts[i].startPos - scaleFromDist) / distRange;
		modelFaceVerts[i].pos = modelFaceVerts[i].startPos + dir * stretchFactor;
		if (gridSnappingEnabled)
		{
			modelFaceVerts[i].pos = snapToGrid(modelFaceVerts[i].pos);
		}
		if (modelFaceVerts[i].ptr)
		{
			*modelFaceVerts[i].ptr = modelFaceVerts[i].pos;
		}
	}

	// update planes for picking
	invalidSolid = !map->vertex_manipulation_sync(pickInfo.modelIdx, modelVerts, false, false);

	updateSelectionSize();

	//
	// TODO: I have no idea what I'm doing but this code scales axis-aligned texture coord axes correctly.
	//       Rewrite all of this after understanding texture axes.
	//

	if (!textureLock)
		return;

	minDist = vec3(FLT_MAX_COORD, FLT_MAX_COORD, FLT_MAX_COORD);
	maxDist = vec3(FLT_MIN_COORD, FLT_MIN_COORD, FLT_MIN_COORD);

	for (int i = 0; i < modelFaceVerts.size(); i++)
	{
		expandBoundingBox(modelFaceVerts[i].pos, minDist, maxDist);
	}
	vec3 newDistRange = maxDist - minDist;
	vec3 scaleFactor = distRange / newDistRange;

	mat4x4 scaleMat;
	scaleMat.loadIdentity();
	scaleMat.scale(scaleFactor.x, scaleFactor.y, scaleFactor.z);

	for (int i = 0; i < scaleTexinfos.size(); i++)
	{
		ScalableTexinfo& oldinfo = scaleTexinfos[i];
		BSPTEXTUREINFO& info = map->texinfos[scaleTexinfos[i].texinfoIdx];

		info.vS = (scaleMat * vec4(oldinfo.oldS, 1)).xyz();
		info.vT = (scaleMat * vec4(oldinfo.oldT, 1)).xyz();

		float shiftS = oldinfo.oldShiftS;
		float shiftT = oldinfo.oldShiftT;

		// magic guess-and-check code that somehow works some of the time
		// also its shit
		for (int k = 0; k < 3; k++)
		{
			vec3 stretchDir;
			if (k == 0) stretchDir = vec3(dir.x, 0, 0).normalize();
			if (k == 1) stretchDir = vec3(0, dir.y, 0).normalize();
			if (k == 2) stretchDir = vec3(0, 0, dir.z).normalize();

			float refDist = 0;
			if (k == 0) refDist = scaleFromDist.x;
			if (k == 1) refDist = scaleFromDist.y;
			if (k == 2) refDist = scaleFromDist.z;

			vec3 texFromDir;
			if (k == 0) texFromDir = dir * vec3(1, 0, 0);
			if (k == 1) texFromDir = dir * vec3(0, 1, 0);
			if (k == 2) texFromDir = dir * vec3(0, 0, 1);

			float dotS = dotProduct(oldinfo.oldS.normalize(), stretchDir);
			float dotT = dotProduct(oldinfo.oldT.normalize(), stretchDir);

			float dotSm = dotProduct(texFromDir, info.vS) < 0 ? 1.0f : -1.0f;
			float dotTm = dotProduct(texFromDir, info.vT) < 0 ? 1.0f : -1.0f;

			// hurr dur oh god im fucking retarded huurr
			if (k == 0 && dotProduct(texFromDir, fromDir) < 0 != fromDir.x < 0)
			{
				dotSm *= -1.0f;
				dotTm *= -1.0f;
			}
			if (k == 1 && dotProduct(texFromDir, fromDir) < 0 != fromDir.y < 0)
			{
				dotSm *= -1.0f;
				dotTm *= -1.0f;
			}
			if (k == 2 && dotProduct(texFromDir, fromDir) < 0 != fromDir.z < 0)
			{
				dotSm *= -1.0f;
				dotTm *= -1.0f;
			}

			float vsdiff = info.vS.length() - oldinfo.oldS.length();
			float vtdiff = info.vT.length() - oldinfo.oldT.length();

			shiftS += (refDist * vsdiff * abs(dotS)) * dotSm;
			shiftT += (refDist * vtdiff * abs(dotT)) * dotTm;
		}

		info.shiftS = shiftS;
		info.shiftT = shiftT;
	}
}

void Renderer::moveSelectedVerts(const vec3& delta)
{
	for (int i = 0; i < modelVerts.size(); i++)
	{
		if (modelVerts[i].selected)
		{
			modelVerts[i].pos = modelVerts[i].startPos + delta;
			if (gridSnappingEnabled)
				modelVerts[i].pos = snapToGrid(modelVerts[i].pos);
			if (modelVerts[i].ptr)
				*modelVerts[i].ptr = modelVerts[i].pos;
		}
	}

	Bsp* map = getSelectedMap();
	if (map && pickInfo.entIdx >= 0)
	{
		Entity* ent = SelectedMap->ents[pickInfo.entIdx];
		invalidSolid = !map->vertex_manipulation_sync(pickInfo.modelIdx, modelVerts, true, false);
		map->getBspRender()->refreshModel(ent->getBspModelIdx());
	}
}

bool Renderer::splitModelFace()
{
	Bsp* map = getSelectedMap();
	if (!map)
		return false;
	BspRenderer* mapRenderer = map->getBspRender();
	// find the pseudo-edge to split with
	std::vector<int> selectedEdges;
	for (int i = 0; i < modelEdges.size(); i++)
	{
		if (modelEdges[i].selected)
		{
			selectedEdges.push_back(i);
		}
	}

	if (selectedEdges.size() != 2)
	{
		logf("Exactly 2 edges must be selected before splitting a face\n");
		return false;
	}
	if (pickInfo.entIdx < 0)
	{
		logf("No selected entity\n");
		return false;
	}
	Entity* ent = map->ents[pickInfo.entIdx];

	HullEdge& edge1 = modelEdges[selectedEdges[0]];
	HullEdge& edge2 = modelEdges[selectedEdges[1]];
	int commonPlane = -1;
	for (int i = 0; i < 2 && commonPlane == -1; i++)
	{
		int thisPlane = edge1.planes[i];
		for (int k = 0; k < 2; k++)
		{
			int otherPlane = edge2.planes[k];
			if (thisPlane == otherPlane)
			{
				commonPlane = thisPlane;
				break;
			}
		}
	}

	if (commonPlane == -1)
	{
		logf("Can't split edges that don't share a plane\n");
		return false;
	}

	vec3 splitPoints[2] = {
		getEdgeControlPoint(modelVerts, edge1),
		getEdgeControlPoint(modelVerts, edge2)
	};

	std::vector<int> modelPlanes;


	BSPMODEL& tmodel = map->models[ent->getBspModelIdx()];
	map->getNodePlanes(tmodel.iHeadnodes[0], modelPlanes);

	// find the plane being split
	int commonPlaneIdx = -1;
	for (int i = 0; i < modelPlanes.size(); i++)
	{
		if (modelPlanes[i] == commonPlane)
		{
			commonPlaneIdx = i;
			break;
		}
	}
	if (commonPlaneIdx == -1)
	{
		logf("Failed to find splitting plane");
		return false;
	}

	// extrude split points so that the new planes aren't coplanar
	{
		int i0 = edge1.verts[0];
		int i1 = edge1.verts[1];
		int i2 = edge2.verts[0];
		if (i2 == i1 || i2 == i0)
			i2 = edge2.verts[1];

		vec3 v0 = modelVerts[i0].pos;
		vec3 v1 = modelVerts[i1].pos;
		vec3 v2 = modelVerts[i2].pos;

		vec3 e1 = (v1 - v0).normalize();
		vec3 e2 = (v2 - v0).normalize();
		vec3 normal = crossProduct(e1, e2).normalize();

		vec3 centroid = getCentroid(modelVerts);
		vec3 faceDir = (centroid - v0).normalize();
		if (dotProduct(faceDir, normal) > 0)
		{
			normal *= -1;
		}

		for (int i = 0; i < 2; i++)
			splitPoints[i] += normal * 4;
	}

	// replace split plane with 2 new slightly-angled planes
	{
		vec3 planeVerts[2][3] = {
			{
				splitPoints[0],
				modelVerts[edge1.verts[1]].pos,
				splitPoints[1]
			},
			{
				splitPoints[0],
				splitPoints[1],
				modelVerts[edge1.verts[0]].pos
			}
		};

		modelPlanes.erase(modelPlanes.begin() + commonPlaneIdx);
		for (int i = 0; i < 2; i++)
		{
			vec3 e1 = (planeVerts[i][1] - planeVerts[i][0]).normalize();
			vec3 e2 = (planeVerts[i][2] - planeVerts[i][0]).normalize();
			vec3 normal = crossProduct(e1, e2).normalize();

			int newPlaneIdx = map->create_plane();
			BSPPLANE& plane = map->planes[newPlaneIdx];
			plane.update(normal, getDistAlongAxis(normal, planeVerts[i][0]));
			modelPlanes.push_back(newPlaneIdx);
		}
	}

	// create a new model from the new set of planes
	std::vector<TransformVert> newHullVerts;
	if (!map->getModelPlaneIntersectVerts(ent->getBspModelIdx(), modelPlanes, newHullVerts))
	{
		logf("Can't split here because the model would not be convex\n");
		return false;
	}

	Solid newSolid;
	if (!getModelSolid(newHullVerts, map, newSolid))
	{
		logf("Splitting here would invalidate the solid\n");
		return false;
	}

	// test that all planes have at least 3 verts
	{
		std::map<int, std::vector<vec3>> planeVerts;
		for (int i = 0; i < newHullVerts.size(); i++)
		{
			for (int k = 0; k < newHullVerts[i].iPlanes.size(); k++)
			{
				int iPlane = newHullVerts[i].iPlanes[k];
				planeVerts[iPlane].push_back(newHullVerts[i].pos);
			}
		}
		for (auto it = planeVerts.begin(); it != planeVerts.end(); ++it)
		{
			std::vector<vec3>& verts = it->second;

			if (verts.size() < 3)
			{
				logf("Can't split here because a face with less than 3 verts would be created\n");
				return false;
			}
		}
	}

	// copy textures/UVs from the old model
	{
		BSPMODEL& oldModel = map->models[ent->getBspModelIdx()];
		for (int i = 0; i < newSolid.faces.size(); i++)
		{
			Face& solidFace = newSolid.faces[i];
			BSPFACE* bestMatch = NULL;
			float bestdot = FLT_MIN_COORD;
			for (int k = 0; k < oldModel.nFaces; k++)
			{
				BSPFACE& bspface = map->faces[oldModel.iFirstFace + k];
				BSPPLANE& plane = map->planes[bspface.iPlane];
				vec3 bspFaceNormal = bspface.nPlaneSide ? plane.vNormal.invert() : plane.vNormal;
				vec3 solidFaceNormal = solidFace.planeSide ? solidFace.plane.vNormal.invert() : solidFace.plane.vNormal;
				float dot = dotProduct(bspFaceNormal, solidFaceNormal);
				if (dot > bestdot)
				{
					bestdot = dot;
					bestMatch = &bspface;
				}
			}
			if (bestMatch)
			{
				solidFace.iTextureInfo = bestMatch->iTextureInfo;
			}
		}
	}

	int modelIdx = map->create_solid(newSolid, ent->getBspModelIdx());

	for (int i = 0; i < modelVerts.size(); i++)
	{
		modelVerts[i].selected = false;
	}
	for (int i = 0; i < modelEdges.size(); i++)
	{
		modelEdges[i].selected = false;
	}

	pushModelUndoState("Split Face", EDIT_MODEL_LUMPS);

	mapRenderer->updateLightmapInfos();
	mapRenderer->calcFaceMaths();
	mapRenderer->refreshModel(modelIdx);
	updateModelVerts();

	gui->reloadLimits();
	return true;
}

void Renderer::scaleSelectedVerts(float x, float y, float z)
{

	TransformAxes& activeAxes = *(transformMode == TRANSFORM_SCALE ? &scaleAxes : &moveAxes);
	vec3 fromOrigin = activeAxes.origin;

	vec3 min(FLT_MAX_COORD, FLT_MAX_COORD, FLT_MAX_COORD);
	vec3 max(FLT_MIN_COORD, FLT_MIN_COORD, FLT_MIN_COORD);
	int selectTotal = 0;
	for (int i = 0; i < modelVerts.size(); i++)
	{
		if (modelVerts[i].selected)
		{
			vec3 v = modelVerts[i].pos;
			if (v.x < min.x) min.x = v.x;
			if (v.y < min.y) min.y = v.y;
			if (v.z < min.z) min.z = v.z;
			if (v.x > max.x) max.x = v.x;
			if (v.y > max.y) max.y = v.y;
			if (v.z > max.z) max.z = v.z;
			selectTotal++;
		}
	}
	if (selectTotal != 0)
		fromOrigin = min + (max - min) * 0.5f;

	debugVec0 = fromOrigin;

	for (int i = 0; i < modelVerts.size(); i++)
	{

		if (modelVerts[i].selected)
		{
			vec3 delta = modelVerts[i].startPos - fromOrigin;
			modelVerts[i].pos = fromOrigin + delta * vec3(x, y, z);
			if (gridSnappingEnabled)
				modelVerts[i].pos = snapToGrid(modelVerts[i].pos);
			if (modelVerts[i].ptr)
				*modelVerts[i].ptr = modelVerts[i].pos;
		}
	}
	Bsp* map = getSelectedMap();
	if (map)
	{
		invalidSolid = !map->vertex_manipulation_sync(pickInfo.modelIdx, modelVerts, true, false);
		if (pickInfo.entIdx >= 0)
		{
			Entity* ent = map->ents[pickInfo.entIdx];
			map->getBspRender()->refreshModel(ent->getBspModelIdx());
		}
		updateSelectionSize();
	}
}

vec3 Renderer::getEdgeControlPoint(std::vector<TransformVert>& hullVerts, HullEdge& edge)
{
	vec3 v0 = hullVerts[edge.verts[0]].pos;
	vec3 v1 = hullVerts[edge.verts[1]].pos;
	return v0 + (v1 - v0) * 0.5f;
}

vec3 Renderer::getCentroid(std::vector<TransformVert>& hullVerts)
{
	vec3 centroid;
	for (int i = 0; i < hullVerts.size(); i++)
	{
		centroid += hullVerts[i].pos;
	}
	return centroid / (float)hullVerts.size();
}

vec3 Renderer::snapToGrid(const vec3& pos)
{
	float snapSize = (float)pow(2.0f, gridSnapLevel);

	float x = round((pos.x) / snapSize) * snapSize;
	float y = round((pos.y) / snapSize) * snapSize;
	float z = round((pos.z) / snapSize) * snapSize;

	return vec3(x, y, z);
}

void Renderer::grabEnt()
{
	if (pickInfo.entIdx <= 0)
	{
		movingEnt = false;
		return;
	}
	movingEnt = true;
	Bsp* map = g_app->getSelectedMap();
	vec3 mapOffset = map->getBspRender()->mapOffset;
	vec3 localCamOrigin = cameraOrigin - mapOffset;
	grabDist = (getEntOrigin(map, map->ents[pickInfo.entIdx]) - localCamOrigin).length();
	grabStartOrigin = localCamOrigin + cameraForward * grabDist;
	grabStartEntOrigin = localCamOrigin + cameraForward * grabDist;
}

void Renderer::cutEnt()
{
	if (pickInfo.entIdx <= 0)
		return;

	if (copiedEnt)
		delete copiedEnt;

	Bsp* map = g_app->getSelectedMap();
	copiedEnt = new Entity();
	*copiedEnt = *map->ents[pickInfo.entIdx];

	DeleteEntityCommand* deleteCommand = new DeleteEntityCommand("Cut Entity", pickInfo);
	deleteCommand->execute();
	pushUndoCommand(deleteCommand);
}

void Renderer::copyEnt()
{
	if (pickInfo.entIdx <= 0)
		return;

	if (copiedEnt)
		delete copiedEnt;

	Bsp* map = g_app->getSelectedMap();
	copiedEnt = new Entity();
	*copiedEnt = *map->ents[pickInfo.entIdx];
}

void Renderer::pasteEnt(bool noModifyOrigin)
{
	if (!copiedEnt)
		return;

	Bsp* map = getSelectedMap();
	if (!map)
	{
		logf("Select a map before pasting an ent\n");
		return;
	}


	Entity insertEnt;
	insertEnt = *copiedEnt;

	if (!noModifyOrigin)
	{
// can't just set camera origin directly because solid ents can have (0,0,0) origins
		vec3 tmpOrigin = getEntOrigin(map, &insertEnt);
		vec3 modelOffset = getEntOffset(map, &insertEnt);
		vec3 mapOffset = map->getBspRender()->mapOffset;

		vec3 moveDist = (cameraOrigin + cameraForward * 100) - tmpOrigin;
		vec3 newOri = (tmpOrigin + moveDist) - (modelOffset + mapOffset);
		vec3 rounded = gridSnappingEnabled ? snapToGrid(newOri) : newOri;
		insertEnt.setOrAddKeyvalue("origin", rounded.toKeyvalueString(!gridSnappingEnabled));
	}

	CreateEntityCommand* createCommand = new CreateEntityCommand("Paste Entity", g_app->getSelectedMapId(), &insertEnt);
	createCommand->execute();
	pushUndoCommand(createCommand);

	clearSelection();
	selectMap(map);
	selectEnt(map, map->ents.size() > 1 ? ((int)map->ents.size() - 1) : 0);
}

void Renderer::deleteEnt(int entIdx)
{
	if (pickInfo.entIdx <= 0 && entIdx <= 0)
		return;
	PickInfo tmpPickInfo = pickInfo;

	if (entIdx > 0 && SelectedMap)
	{
		tmpPickInfo.entIdx = entIdx;
	}

	DeleteEntityCommand* deleteCommand = new DeleteEntityCommand("Delete Entity", pickInfo);
	deleteCommand->execute();
	pushUndoCommand(deleteCommand);
}

void Renderer::deselectObject()
{
	pickInfo.entIdx = -1;
	pickInfo.faceIdx = -1;
	pickInfo.modelIdx = -1;
	isTransformableSolid = true;
	modelUsesSharedStructures = false;
	hoverVert = -1;
	hoverEdge = -1;
	hoverAxis = -1;
	updateEntConnections();
}

void Renderer::deselectFaces()
{
	Bsp* map = getSelectedMap();
	if (!map)
		return;

	for (int i = 0; i < selectedFaces.size(); i++)
	{
		getSelectedMap()->getBspRender()->highlightFace(selectedFaces[i], false);
	}
	selectedFaces.clear();
}

void Renderer::selectEnt(Bsp* map, int entIdx)
{
	Entity* ent = NULL;
	if (pickInfo.entIdx >= 0)
	{
		ent = map->ents[pickInfo.entIdx];
	}
	pickInfo.entIdx = entIdx;
	pickInfo.modelIdx = ent ? ent->getBspModelIdx() : -1;
	updateSelectionSize();
	updateEntConnections();
	
	updateEntityState(ent);
	if (ent && ent->isBspModel())
		saveLumpState(map, 0xffffffff, true);
	pickCount++; // force transform window update
}

void Renderer::goToCoords(float x, float y, float z)
{
	cameraOrigin.x = x;
	cameraOrigin.y = y;
	cameraOrigin.z = z;
}

void Renderer::goToEnt(Bsp* map, int entIdx)
{
	if (entIdx < 0)
		return;

	Entity* ent = map->ents[entIdx];

	vec3 size;
	if (ent->isBspModel())
	{
		BSPMODEL& model = map->models[ent->getBspModelIdx()];
		size = (model.nMaxs - model.nMins) * 0.5f;
	}
	else
	{
		EntCube* cube = pointEntRenderer->getEntCube(ent);
		size = cube->maxs - cube->mins * 0.5f;
	}

	cameraOrigin = getEntOrigin(map, ent) - cameraForward * (size.length() + 64.0f);
}

void Renderer::ungrabEnt()
{
	if (!movingEnt)
	{
		return;
	}
	pushEntityUndoState("Move Entity");

	movingEnt = false;
}

void Renderer::updateEntityState(Entity* ent)
{
	if (!ent)
		return;

	if (!undoEntityState)
	{
		undoEntityState = new Entity();
	}
	*undoEntityState = *ent;
	undoEntOrigin = ent->getOrigin();
}

void Renderer::saveLumpState(Bsp* map, int targetLumps, bool deleteOldState)
{
	if (deleteOldState)
	{
		for (int i = 0; i < HEADER_LUMPS; i++)
		{
			if (undoLumpState.lumps[i])
				delete[] undoLumpState.lumps[i];
		}
	}

	undoLumpState = map->duplicate_lumps(targetLumps);
}

void Renderer::pushEntityUndoState(const std::string& actionDesc)
{
	if (!pickInfo.entIdx)
	{
		logf("Invalid entity undo state push\n");
		return;
	}

	Entity* ent = SelectedMap->ents[pickInfo.entIdx];

	if (!ent)
	{
		logf("Invalid entity undo state push 2\n");
		return;
	}

	bool anythingToUndo = true;
	if (undoEntityState->keyOrder.size() == ent->keyOrder.size())
	{
		bool keyvaluesDifferent = false;
		for (int i = 0; i < undoEntityState->keyOrder.size(); i++)
		{
			std::string oldKey = undoEntityState->keyOrder[i];
			std::string newKey = ent->keyOrder[i];
			if (oldKey != newKey)
			{
				keyvaluesDifferent = true;
				break;
			}
			std::string oldVal = undoEntityState->keyvalues[oldKey];
			std::string newVal = ent->keyvalues[oldKey];
			if (oldVal != newVal)
			{
				keyvaluesDifferent = true;
				break;
			}
		}

		anythingToUndo = keyvaluesDifferent;
	}

	if (!anythingToUndo)
	{
		return; // nothing to undo
	}

	pushUndoCommand(new EditEntityCommand(actionDesc, pickInfo, undoEntityState, ent));
	updateEntityState(ent);
}

void Renderer::pushModelUndoState(const std::string& actionDesc, int targetLumps)
{
	Bsp* map = getSelectedMap();

	if (pickInfo.modelIdx <= 0)
		pickInfo.modelIdx = 0;
	if (pickInfo.entIdx <= 0)
		pickInfo.entIdx = 0;
	if (!map)
	{
		logf("Impossible, no map, ent or model idx\n");
		return;
	}
	Entity* ent = map->ents[pickInfo.entIdx];

	LumpState newLumps = map->duplicate_lumps(targetLumps);

	bool differences[HEADER_LUMPS] = {false};

	bool anyDifference = false;
	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		if (newLumps.lumps[i] && undoLumpState.lumps[i])
		{
			if (newLumps.lumpLen[i] != undoLumpState.lumpLen[i] || memcmp(newLumps.lumps[i], undoLumpState.lumps[i], newLumps.lumpLen[i]) != 0)
			{
				anyDifference = true;
				differences[i] = true;
			}
		}
	}

	if (!anyDifference)
	{
		logf("No differences detected\n");
		return;
	}

	// delete lumps that have no differences to save space
	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		if (!differences[i])
		{
			delete[] undoLumpState.lumps[i];
			delete[] newLumps.lumps[i];
			undoLumpState.lumps[i] = newLumps.lumps[i] = NULL;
			undoLumpState.lumpLen[i] = newLumps.lumpLen[i] = 0;
		}
	}

	EditBspModelCommand* editCommand = new EditBspModelCommand(actionDesc, pickInfo, undoLumpState, newLumps, undoEntOrigin);
	pushUndoCommand(editCommand);
	saveLumpState(map, 0xffffffff, false);

	// entity origin edits also update the ent origin (TODO: this breaks when moving + scaling something)
	updateEntityState(ent);
}

void Renderer::pushUndoCommand(Command* cmd)
{
	undoHistory.push_back(cmd);
	clearRedoCommands();

	while (!undoHistory.empty() && undoHistory.size() > undoLevels)
	{
		delete undoHistory[0];
		undoHistory.erase(undoHistory.begin());
	}

	calcUndoMemoryUsage();
}

void Renderer::undo()
{
	if (undoHistory.empty())
	{
		return;
	}

	Command* undoCommand = undoHistory[undoHistory.size() - 1];
	if (!undoCommand->allowedDuringLoad && isLoading)
	{
		logf("Can't undo %s while map is loading!\n", undoCommand->desc.c_str());
		return;
	}

	undoCommand->undo();
	undoHistory.pop_back();
	redoHistory.push_back(undoCommand);
	updateEnts();
}

void Renderer::redo()
{
	if (redoHistory.empty())
	{
		return;
	}

	Command* redoCommand = redoHistory[redoHistory.size() - 1];
	if (!redoCommand->allowedDuringLoad && isLoading)
	{
		logf("Can't redo %s while map is loading!\n", redoCommand->desc.c_str());
		return;
	}

	redoCommand->execute();
	redoHistory.pop_back();
	undoHistory.push_back(redoCommand);
	updateEnts();
}

void Renderer::clearUndoCommands()
{
	for (int i = 0; i < undoHistory.size(); i++)
	{
		delete undoHistory[i];
	}

	undoHistory.clear();
	calcUndoMemoryUsage();
}

void Renderer::clearRedoCommands()
{
	for (int i = 0; i < redoHistory.size(); i++)
	{
		delete redoHistory[i];
	}

	redoHistory.clear();
	calcUndoMemoryUsage();
}

void Renderer::calcUndoMemoryUsage()
{
	undoMemoryUsage = (undoHistory.size() + redoHistory.size()) * sizeof(Command*);

	for (int i = 0; i < undoHistory.size(); i++)
	{
		undoMemoryUsage += undoHistory[i]->memoryUsage();
	}
	for (int i = 0; i < redoHistory.size(); i++)
	{
		undoMemoryUsage += redoHistory[i]->memoryUsage();
	}
}

void Renderer::updateEnts()
{
	Bsp* map = getSelectedMap();
	if (map && map->getBspRender())
	{
		map->getBspRender()->preRenderEnts();
		updateEntConnections();
		updateEntConnectionPositions();
	}
}
