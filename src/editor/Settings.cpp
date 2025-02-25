#include "lang.h"
#include "Settings.h"
#include "Renderer.h"
#include "log.h"

std::string g_settings_path = "./bspguy.ini";
std::string g_game_dir = "/";
std::string g_working_dir = "./";
std::string g_startup_dir = "";


inih::INIReader* settings_ini = NULL;

Settings g_settings{};

void Settings::loadDefaultSettings()
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
	maximized = false;
	fontSize = 22.f;
	gamedir = std::string();
	workingdir = "./bspguy_work/";

	lastdir = "";
	selected_lang = "EN";
	languages.clear();
	languages.push_back(selected_lang);

	undoLevels = 64;
	fpslimit = 100;

	verboseLogs = false;
#ifndef NDEBUG
	verboseLogs = true;
#endif
	save_windows = false;
	debug_open = false;
	keyvalue_open = false;
	transform_open = false;
	log_open = false;
	limits_open = false;
	entreport_open = false;
	goto_open = false;

	settings_tab = 0;

	render_flags = g_render_flags = (RENDER_TEXTURES | RENDER_LIGHTMAPS | RENDER_SPECIAL
		| RENDER_ENTS | RENDER_SPECIAL_ENTS | RENDER_POINT_ENTS | RENDER_WIREFRAME | RENDER_ENT_CONNECTIONS
		| RENDER_ENT_CLIPNODES | RENDER_MODELS | RENDER_MODELS_ANIMATED | RENDER_WORLD_CLIPNODES | RENDER_ORIGIN);

	vsync = true;
	merge_verts = false;
	merge_edges = false;
	mark_unused_texinfos = false;
	start_at_entity = false;
	savebackup = true;
	save_crc = false;
	save_cam = false;
	auto_import_ent = false;
	same_dir_for_ent = false;

	moveSpeed = 500.0f;
	fov = 75.0f;
	zfar = 262144.0f;
	rotSpeed = 5.0f;

	rad_path = "hlrad.exe";
	rad_options = "\"{map_path}\"";


	default_is_empty = true;

	reload_ents_list = true;
	strip_wad_path = false;

	palette_name = "quake_1";

	unsigned char default_data[0x300] = {
	 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x1F, 0x1F, 0x1F, 0x2F, 0x2F, 0x2F, 0x3F, 0x3F, 0x3F, 0x4B,
	 0x4B, 0x4B, 0x5B, 0x5B, 0x5B, 0x6B, 0x6B, 0x6B, 0x7B, 0x7B, 0x7B, 0x8B, 0x8B, 0x8B, 0x9B, 0x9B,
	 0x9B, 0xAB, 0xAB, 0xAB, 0xBB, 0xBB, 0xBB, 0xCB, 0xCB, 0xCB, 0xDB, 0xDB, 0xDB, 0xEB, 0xEB, 0xEB,
	 0x0F, 0x0B, 0x07, 0x17, 0x0F, 0x0B, 0x1F, 0x17, 0x0B, 0x27, 0x1B, 0x0F, 0x2F, 0x23, 0x13, 0x37,
	 0x2B, 0x17, 0x3F, 0x2F, 0x17, 0x4B, 0x37, 0x1B, 0x53, 0x3B, 0x1B, 0x5B, 0x43, 0x1F, 0x63, 0x4B,
	 0x1F, 0x6B, 0x53, 0x1F, 0x73, 0x57, 0x1F, 0x7B, 0x5F, 0x23, 0x83, 0x67, 0x23, 0x8F, 0x6F, 0x23,
	 0x0B, 0x0B, 0x0F, 0x13, 0x13, 0x1B, 0x1B, 0x1B, 0x27, 0x27, 0x27, 0x33, 0x2F, 0x2F, 0x3F, 0x37,
	 0x37, 0x4B, 0x3F, 0x3F, 0x57, 0x47, 0x47, 0x67, 0x4F, 0x4F, 0x73, 0x5B, 0x5B, 0x7F, 0x63, 0x63,
	 0x8B, 0x6B, 0x6B, 0x97, 0x73, 0x73, 0xA3, 0x7B, 0x7B, 0xAF, 0x83, 0x83, 0xBB, 0x8B, 0x8B, 0xCB,
	 0x00, 0x00, 0x00, 0x07, 0x07, 0x00, 0x0B, 0x0B, 0x00, 0x13, 0x13, 0x00, 0x1B, 0x1B, 0x00, 0x23,
	 0x23, 0x00, 0x2B, 0x2B, 0x07, 0x2F, 0x2F, 0x07, 0x37, 0x37, 0x07, 0x3F, 0x3F, 0x07, 0x47, 0x47,
	 0x07, 0x4B, 0x4B, 0x0B, 0x53, 0x53, 0x0B, 0x5B, 0x5B, 0x0B, 0x63, 0x63, 0x0B, 0x6B, 0x6B, 0x0F,
	 0x07, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x17, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x27, 0x00, 0x00, 0x2F,
	 0x00, 0x00, 0x37, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x47, 0x00, 0x00, 0x4F, 0x00, 0x00, 0x57, 0x00,
	 0x00, 0x5F, 0x00, 0x00, 0x67, 0x00, 0x00, 0x6F, 0x00, 0x00, 0x77, 0x00, 0x00, 0x7F, 0x00, 0x00,
	 0x13, 0x13, 0x00, 0x1B, 0x1B, 0x00, 0x23, 0x23, 0x00, 0x2F, 0x2B, 0x00, 0x37, 0x2F, 0x00, 0x43,
	 0x37, 0x00, 0x4B, 0x3B, 0x07, 0x57, 0x43, 0x07, 0x5F, 0x47, 0x07, 0x6B, 0x4B, 0x0B, 0x77, 0x53,
	 0x0F, 0x83, 0x57, 0x13, 0x8B, 0x5B, 0x13, 0x97, 0x5F, 0x1B, 0xA3, 0x63, 0x1F, 0xAF, 0x67, 0x23,
	 0x23, 0x13, 0x07, 0x2F, 0x17, 0x0B, 0x3B, 0x1F, 0x0F, 0x4B, 0x23, 0x13, 0x57, 0x2B, 0x17, 0x63,
	 0x2F, 0x1F, 0x73, 0x37, 0x23, 0x7F, 0x3B, 0x2B, 0x8F, 0x43, 0x33, 0x9F, 0x4F, 0x33, 0xAF, 0x63,
	 0x2F, 0xBF, 0x77, 0x2F, 0xCF, 0x8F, 0x2B, 0xDF, 0xAB, 0x27, 0xEF, 0xCB, 0x1F, 0xFF, 0xF3, 0x1B,
	 0x0B, 0x07, 0x00, 0x1B, 0x13, 0x00, 0x2B, 0x23, 0x0F, 0x37, 0x2B, 0x13, 0x47, 0x33, 0x1B, 0x53,
	 0x37, 0x23, 0x63, 0x3F, 0x2B, 0x6F, 0x47, 0x33, 0x7F, 0x53, 0x3F, 0x8B, 0x5F, 0x47, 0x9B, 0x6B,
	 0x53, 0xA7, 0x7B, 0x5F, 0xB7, 0x87, 0x6B, 0xC3, 0x93, 0x7B, 0xD3, 0xA3, 0x8B, 0xE3, 0xB3, 0x97,
	 0xAB, 0x8B, 0xA3, 0x9F, 0x7F, 0x97, 0x93, 0x73, 0x87, 0x8B, 0x67, 0x7B, 0x7F, 0x5B, 0x6F, 0x77,
	 0x53, 0x63, 0x6B, 0x4B, 0x57, 0x5F, 0x3F, 0x4B, 0x57, 0x37, 0x43, 0x4B, 0x2F, 0x37, 0x43, 0x27,
	 0x2F, 0x37, 0x1F, 0x23, 0x2B, 0x17, 0x1B, 0x23, 0x13, 0x13, 0x17, 0x0B, 0x0B, 0x0F, 0x07, 0x07,
	 0xBB, 0x73, 0x9F, 0xAF, 0x6B, 0x8F, 0xA3, 0x5F, 0x83, 0x97, 0x57, 0x77, 0x8B, 0x4F, 0x6B, 0x7F,
	 0x4B, 0x5F, 0x73, 0x43, 0x53, 0x6B, 0x3B, 0x4B, 0x5F, 0x33, 0x3F, 0x53, 0x2B, 0x37, 0x47, 0x23,
	 0x2B, 0x3B, 0x1F, 0x23, 0x2F, 0x17, 0x1B, 0x23, 0x13, 0x13, 0x17, 0x0B, 0x0B, 0x0F, 0x07, 0x07,
	 0xDB, 0xC3, 0xBB, 0xCB, 0xB3, 0xA7, 0xBF, 0xA3, 0x9B, 0xAF, 0x97, 0x8B, 0xA3, 0x87, 0x7B, 0x97,
	 0x7B, 0x6F, 0x87, 0x6F, 0x5F, 0x7B, 0x63, 0x53, 0x6B, 0x57, 0x47, 0x5F, 0x4B, 0x3B, 0x53, 0x3F,
	 0x33, 0x43, 0x33, 0x27, 0x37, 0x2B, 0x1F, 0x27, 0x1F, 0x17, 0x1B, 0x13, 0x0F, 0x0F, 0x0B, 0x07,
	 0x6F, 0x83, 0x7B, 0x67, 0x7B, 0x6F, 0x5F, 0x73, 0x67, 0x57, 0x6B, 0x5F, 0x4F, 0x63, 0x57, 0x47,
	 0x5B, 0x4F, 0x3F, 0x53, 0x47, 0x37, 0x4B, 0x3F, 0x2F, 0x43, 0x37, 0x2B, 0x3B, 0x2F, 0x23, 0x33,
	 0x27, 0x1F, 0x2B, 0x1F, 0x17, 0x23, 0x17, 0x0F, 0x1B, 0x13, 0x0B, 0x13, 0x0B, 0x07, 0x0B, 0x07,
	 0xFF, 0xF3, 0x1B, 0xEF, 0xDF, 0x17, 0xDB, 0xCB, 0x13, 0xCB, 0xB7, 0x0F, 0xBB, 0xA7, 0x0F, 0xAB,
	 0x97, 0x0B, 0x9B, 0x83, 0x07, 0x8B, 0x73, 0x07, 0x7B, 0x63, 0x07, 0x6B, 0x53, 0x00, 0x5B, 0x47,
	 0x00, 0x4B, 0x37, 0x00, 0x3B, 0x2B, 0x00, 0x2B, 0x1F, 0x00, 0x1B, 0x0F, 0x00, 0x0B, 0x07, 0x00,
	 0x00, 0x00, 0xFF, 0x0B, 0x0B, 0xEF, 0x13, 0x13, 0xDF, 0x1B, 0x1B, 0xCF, 0x23, 0x23, 0xBF, 0x2B,
	 0x2B, 0xAF, 0x2F, 0x2F, 0x9F, 0x2F, 0x2F, 0x8F, 0x2F, 0x2F, 0x7F, 0x2F, 0x2F, 0x6F, 0x2F, 0x2F,
	 0x5F, 0x2B, 0x2B, 0x4F, 0x23, 0x23, 0x3F, 0x1B, 0x1B, 0x2F, 0x13, 0x13, 0x1F, 0x0B, 0x0B, 0x0F,
	 0x2B, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x4B, 0x07, 0x00, 0x5F, 0x07, 0x00, 0x6F, 0x0F, 0x00, 0x7F,
	 0x17, 0x07, 0x93, 0x1F, 0x07, 0xA3, 0x27, 0x0B, 0xB7, 0x33, 0x0F, 0xC3, 0x4B, 0x1B, 0xCF, 0x63,
	 0x2B, 0xDB, 0x7F, 0x3B, 0xE3, 0x97, 0x4F, 0xE7, 0xAB, 0x5F, 0xEF, 0xBF, 0x77, 0xF7, 0xD3, 0x8B,
	 0xA7, 0x7B, 0x3B, 0xB7, 0x9B, 0x37, 0xC7, 0xC3, 0x37, 0xE7, 0xE3, 0x57, 0x7F, 0xBF, 0xFF, 0xAB,
	 0xE7, 0xFF, 0xD7, 0xFF, 0xFF, 0x67, 0x00, 0x00, 0x8B, 0x00, 0x00, 0xB3, 0x00, 0x00, 0xD7, 0x00,
	 0x00, 0xFF, 0x00, 0x00, 0xFF, 0xF3, 0x93, 0xFF, 0xF7, 0xC7, 0xFF, 0xFF, 0xFF, 0x9F, 0x5B, 0x53,
	};

	pal_id = -1;
	memcpy(palette_default, default_data, 0x300);

	ResetBspLimits();

	fgdPaths.clear();
	fgdPaths.emplace_back("/moddir/GameDefinitionFile.fgd", true);

	resPaths.clear();
	resPaths.emplace_back("/moddir/", true);
	resPaths.emplace_back("/moddir_addon/", true);

	conditionalPointEntTriggers.clear();
	conditionalPointEntTriggers.emplace_back("trigger_once");
	conditionalPointEntTriggers.emplace_back("trigger_multiple");
	conditionalPointEntTriggers.emplace_back("trigger_counter");
	conditionalPointEntTriggers.emplace_back("trigger_gravity");
	conditionalPointEntTriggers.emplace_back("trigger_teleport");

	entsThatNeverNeedAnyHulls.clear();
	entsThatNeverNeedAnyHulls.emplace_back("env_bubbles");
	entsThatNeverNeedAnyHulls.emplace_back("func_tankcontrols");
	entsThatNeverNeedAnyHulls.emplace_back("func_traincontrols");
	entsThatNeverNeedAnyHulls.emplace_back("func_vehiclecontrols");
	entsThatNeverNeedAnyHulls.emplace_back("trigger_autosave"); // obsolete in sven
	entsThatNeverNeedAnyHulls.emplace_back("trigger_endsection"); // obsolete in sven

	entsThatNeverNeedCollision.clear();
	entsThatNeverNeedCollision.emplace_back("func_illusionary");
	entsThatNeverNeedCollision.emplace_back("func_mortar_field");

	passableEnts.clear();
	passableEnts.emplace_back("func_door");
	passableEnts.emplace_back("func_door_rotating");
	passableEnts.emplace_back("func_pendulum");
	passableEnts.emplace_back("func_tracktrain");
	passableEnts.emplace_back("func_train");
	passableEnts.emplace_back("func_water");
	passableEnts.emplace_back("momentary_door");

	playerOnlyTriggers.clear();
	playerOnlyTriggers.emplace_back("func_ladder");
	playerOnlyTriggers.emplace_back("game_zone_player");
	playerOnlyTriggers.emplace_back("player_respawn_zone");
	playerOnlyTriggers.emplace_back("trigger_cdaudio");
	playerOnlyTriggers.emplace_back("trigger_changelevel");
	playerOnlyTriggers.emplace_back("trigger_transition");

	monsterOnlyTriggers.clear();
	monsterOnlyTriggers.emplace_back("func_monsterclip");
	monsterOnlyTriggers.emplace_back("trigger_monsterjump");

	entsNegativePitchPrefix.clear();

	entsNegativePitchPrefix.emplace_back("ammo_");
	entsNegativePitchPrefix.emplace_back("env_sprite");
	entsNegativePitchPrefix.emplace_back("cycler");
	entsNegativePitchPrefix.emplace_back("item_");
	entsNegativePitchPrefix.emplace_back("monster_");
	entsNegativePitchPrefix.emplace_back("weaponbox");
	entsNegativePitchPrefix.emplace_back("worlditems");
	entsNegativePitchPrefix.emplace_back("xen_");

	transparentTextures.clear();
	transparentTextures.emplace_back("AAATRIGGER");

	transparentEntities.clear();
	transparentEntities.emplace_back("func_buyzone");
}

void  Settings::fillLanguages(const std::string& folderPath)
{
	languages.clear();
	languages.emplace_back("EN");

	std::error_code err;
	for (const auto& entry : fs::directory_iterator(folderPath, err))
	{
		if (!entry.is_directory()) {
			std::string filename = entry.path().filename().string();
			if (starts_with(filename, "language_") && ends_with(filename, ".ini"))
			{
				std::string language = filename.substr(9);
				language.erase(language.size() - 4);
				language = toUpperCase(language);
				if (std::find(languages.begin(), languages.end(), language) == languages.end())
					languages.push_back(language);
			}
		}
	}
}

void Settings::fillPalettes(const std::string& folderPath)
{
	palettes.clear();

	std::error_code err;
	for (const auto& entry : fs::directory_iterator(folderPath, err))
	{
		if (!entry.is_directory()) {
			std::string filename = entry.path().filename().string();
			if (ends_with(filename, ".pal"))
			{
				int len;
				char* data = loadFile(entry.path().string(), len);
				if (data)
				{
					if (len > 256 * sizeof(COLOR3))
					{
						print_log(PRINT_RED, "Bad palette \"{}\" size : {} bytes!", entry.path().string(), len);
					}
					else
					{
						filename.pop_back(); filename.pop_back(); filename.pop_back(); filename.pop_back();
						palettes.push_back({ toUpperCase(filename), (unsigned int)(len / sizeof(COLOR3)), NULL });
						memcpy(palettes[palettes.size() - 1].data, data, len);
						delete[] data;
					}
				}
			}
		}
	}
}

void Settings::AddRecentFile(const std::string& file)
{
	if (fileExists(file) && std::find(g_settings.lastOpened.begin(), g_settings.lastOpened.end(), file) == g_settings.lastOpened.end())
	{
		g_settings.lastOpened.push_back(file);
	}
}

void Settings::loadSettings()
{
	set_localize_lang("EN");
	fillLanguages("./languages/");
	loadDefaultSettings();

	if (fileExists(g_settings_path))
	{
		try
		{
			settings_ini = new inih::INIReader(g_settings_path);
			int errorLine = settings_ini->ParseError();
			if (errorLine < 0)
			{
				throw std::runtime_error("Error parse! Can't open file !" + g_settings_path);
			}
			else if (errorLine > 0)
			{
				throw std::runtime_error(("Error parse ini at line:" + std::to_string(errorLine)).c_str());
			}
		}
		catch (std::runtime_error& runtime)
		{
			print_log(PRINT_RED | PRINT_INTENSITY, "Settings parse from {} fatal error: {}\n", g_settings_path, runtime.what());
			delete settings_ini;
			settings_ini = NULL;
		}
	}
	else
	{
		print_log(PRINT_RED | PRINT_INTENSITY, "No setting file {} found! Create new...\n", g_settings_path);
		saveSettings(g_settings_path);
		try
		{
			settings_ini = new inih::INIReader(g_settings_path);
			int errorLine = settings_ini->ParseError();
			if (errorLine < 0)
			{
				throw std::runtime_error("Error parse! Can't open file " + g_settings_path);
			}
			else if (errorLine > 0)
			{
				throw std::runtime_error(("Error parse ini at line:" + std::to_string(errorLine)).c_str());
			}
		}
		catch (std::runtime_error& runtime)
		{
			print_log(PRINT_RED | PRINT_INTENSITY, "Settings parse from {} fatal error: {}\n", g_settings_path, runtime.what());
			delete settings_ini;
			settings_ini = NULL;
		}
	}

	if (!settings_ini)
		return;

	fillPalettes("./palettes/");

	palette_name = "quake_1";

	if (settings_ini->ParseError() != 0) {
		print_log(PRINT_RED, "Can't load {}\n", g_settings_path);
		return;
	}

	selected_lang = settings_ini->Get<std::string>("GENERAL", "lang", "EN");

	set_localize_lang(selected_lang);

	g_settings.windowWidth = settings_ini->Get<int>("GENERAL", "window_width", 800);
	g_settings.windowHeight = settings_ini->Get<int>("GENERAL", "window_height", 600);
	g_settings.windowX = settings_ini->Get<int>("GENERAL", "window_x", 0);
	g_settings.windowY = settings_ini->Get<int>("GENERAL", "window_y", 0);
	g_settings.maximized = settings_ini->Get<int>("GENERAL", "window_maximized", 0) != 0;
	g_settings.start_at_entity = settings_ini->Get<int>("GENERAL", "start_at_entity", 0) != 0;
	g_settings.savebackup = settings_ini->Get<int>("GENERAL", "savebackup", 0) != 0;
	g_settings.save_crc = settings_ini->Get<int>("GENERAL", "save_crc", 1) != 0;
	g_settings.save_cam = settings_ini->Get<int>("GENERAL", "save_cam", 0) != 0;
	g_settings.auto_import_ent = settings_ini->Get<int>("GENERAL", "auto_import_ent", 0) != 0;
	g_settings.same_dir_for_ent = settings_ini->Get<int>("GENERAL", "same_dir_for_ent", 0) != 0;
	g_settings.reload_ents_list = settings_ini->Get<int>("GENERAL", "reload_ents_list", 0) != 0;
	g_settings.strip_wad_path = settings_ini->Get<int>("GENERAL", "strip_wad_path", 0) != 0;
	g_settings.default_is_empty = settings_ini->Get<int>("GENERAL", "default_is_empty", 0) != 0;
	g_settings.undoLevels = settings_ini->Get<int>("GENERAL", "undo_levels", 100);

	g_settings.save_windows = settings_ini->Get<int>("WIDGETS", "save_windows", 1) != 0;
	g_settings.debug_open = settings_ini->Get<int>("WIDGETS", "debug_open", 0) != 0 && g_settings.save_windows;
	g_settings.keyvalue_open = settings_ini->Get<int>("WIDGETS", "keyvalue_open", 0) != 0 && g_settings.save_windows;
	g_settings.transform_open = settings_ini->Get<int>("WIDGETS", "transform_open", 0) != 0 && g_settings.save_windows;
	g_settings.log_open = settings_ini->Get<int>("WIDGETS", "log_open", 0) != 0 && g_settings.save_windows;
	g_settings.limits_open = settings_ini->Get<int>("WIDGETS", "limits_open", 0) != 0 && g_settings.save_windows;
	g_settings.entreport_open = settings_ini->Get<int>("WIDGETS", "entreport_open", 0) != 0 && g_settings.save_windows;
	g_settings.texbrowser_open = settings_ini->Get<int>("WIDGETS", "texbrowser_open", 0) != 0 && g_settings.save_windows;
	g_settings.goto_open = settings_ini->Get<int>("WIDGETS", "goto_open", 0) != 0 && g_settings.save_windows;
	g_settings.settings_tab = settings_ini->Get<int>("WIDGETS", "settings_tab", 0);

	g_settings.vsync = settings_ini->Get<int>("GRAPHICS", "vsync", 1) != 0;
	g_settings.fov = settings_ini->Get<float>("GRAPHICS", "fov", 60.0f);
	g_settings.zfar = settings_ini->Get<float>("GRAPHICS", "zfar", 1000.0f);
	g_settings.render_flags = settings_ini->Get<int>("GRAPHICS", "renders_flags", 0);
	g_settings.fontSize = settings_ini->Get<float>("GRAPHICS", "font_size", 22.0f);
	g_settings.fpslimit = settings_ini->Get<int>("GRAPHICS", "fpslimit", 60);


	g_settings.mark_unused_texinfos = settings_ini->Get<int>("OPTIMIZE", "mark_unused_texinfos", 0) != 0;
	g_settings.merge_verts = settings_ini->Get<int>("OPTIMIZE", "merge_verts", 0) != 0;
	g_settings.merge_edges = settings_ini->Get<int>("OPTIMIZE", "merge_edges", 0) != 0;

	if (g_settings.fpslimit < 30) {
		g_settings.fpslimit = 30;
	}
	if (g_settings.fpslimit > 1000) {
		g_settings.fpslimit = 1000;
	}

	g_settings.moveSpeed = settings_ini->Get<float>("INPUT", "move_speed", 500.0f);

	if (g_settings.moveSpeed < 100) {
		print_log(get_localized_string(LANG_0927));
		g_settings.moveSpeed = 500;
	}
	g_settings.rotSpeed = settings_ini->Get<float>("INPUT", "rot_speed", 1.0f);

	g_settings.gamedir = settings_ini->Get<std::string>("PATHS", "gamedir", "");
	g_settings.workingdir = settings_ini->Get<std::string>("PATHS", "workingdir", "");
	g_settings.lastdir = settings_ini->Get<std::string>("PATHS", "lastdir", "");
	g_settings.rad_path = settings_ini->Get<std::string>("PATHS", "hlrad_path", "");
	g_settings.palette_name = settings_ini->Get<std::string>("PATHS", "palette", "");

	int fgdCount = settings_ini->Get<int>("FGD", "count", 0);
	if (fgdCount)
		g_settings.fgdPaths.clear();

	for (int i = 1; i <= fgdCount; ++i) {
		std::string item = settings_ini->Get<std::string>("FGD", std::to_string(i), "");
		if (!item.empty() && (starts_with(item, "enabled?") || starts_with(item, "disabled?"))) {
			g_settings.fgdPaths.emplace_back(item.substr(item.find('?') + 1), starts_with(item, "enabled?"));
		}
	}

	int resCount = settings_ini->Get<int>("RES", "count", 0);
	if (resCount)
		g_settings.resPaths.clear();

	for (int i = 1; i <= resCount; ++i) {
		std::string item = settings_ini->Get<std::string>("RES", std::to_string(i), "");
		if (!item.empty() && (starts_with(item, "enabled?") || starts_with(item, "disabled?"))) {
			g_settings.resPaths.emplace_back(item.substr(item.find('?') + 1), starts_with(item, "enabled?"));
		}
	}

	g_settings.verboseLogs = settings_ini->Get<int>("DEBUG", "verbose_logs", 0) != 0;
#ifndef NDEBUG
	g_settings.verboseLogs = true;
#endif

	int lastCount = settings_ini->Get<int>("LAST_OPENED", "count", 0);
	lastOpened.clear();

	for (int i = 0; i < lastCount; i++)
	{
		std::string file = settings_ini->Get<std::string>("LAST_OPENED", "file_" + std::to_string(i + 1), "");
		if (fileExists(file))
		{
			AddRecentFile(file);
		}
	}

	int limitEngines = settings_ini->Get<int>("ENGINE", "count", 0);
	if (limitEngines > 0)
	{
		limitsMap.clear();
		for (int i = 0; i < limitEngines; i++)
		{
			std::string engineName = settings_ini->Get<std::string>("ENGINE", "name" + std::to_string(i+1), "half-life1");

			limitsMap[engineName].engineName = engineName;
			limitsMap[engineName].fltMaxCoord = settings_ini->Get<float>(engineName, "FLT_MAX_COORD", 16384.0f);
			limitsMap[engineName].maxMapModels = settings_ini->Get<int>(engineName, "MAX_MAP_MODELS", 1024);
			limitsMap[engineName].maxSurfaceExtent = settings_ini->Get<int>(engineName, "MAX_SURFACE_EXTENT", 64);
			limitsMap[engineName].maxMapNodes = settings_ini->Get<int>(engineName, "MAX_MAP_NODES", 32767);
			limitsMap[engineName].maxMapClipnodes = settings_ini->Get<int>(engineName, "MAX_MAP_CLIPNODES", 32767);
			limitsMap[engineName].maxMapLeaves = settings_ini->Get<int>(engineName, "MAX_MAP_LEAVES", 8192);
			limitsMap[engineName].maxMapVisdata = settings_ini->Get<int>(engineName, "MAX_MAP_VISDATA", 64) * 1024 * 1024;
			limitsMap[engineName].maxMapEnts = settings_ini->Get<int>(engineName, "MAX_MAP_ENTS", 1024);
			limitsMap[engineName].maxMapSurfedges = settings_ini->Get<int>(engineName, "MAX_MAP_SURFEDGES", 128000);
			limitsMap[engineName].maxMapEdges = settings_ini->Get<int>(engineName, "MAX_MAP_EDGES", 256000);
			limitsMap[engineName].maxMapTextures = settings_ini->Get<int>(engineName, "MAX_MAP_TEXTURES", 512);
			limitsMap[engineName].maxMapLightdata = settings_ini->Get<int>(engineName, "MAX_MAP_LIGHTDATA", 64) * 1024 * 1024;
			limitsMap[engineName].maxTextureDimension = settings_ini->Get<int>(engineName, "MAX_TEXTURE_DIMENSION", 4096);
			limitsMap[engineName].maxTextureSize = ((g_limits.maxTextureDimension * g_limits.maxTextureDimension * 2 * 3) / 2);
			limitsMap[engineName].maxMapBoundary = settings_ini->Get<float>(engineName, "MAX_MAP_BOUNDARY", 4096.0f);
			limitsMap[engineName].textureStep = settings_ini->Get<int>(engineName, "TEXTURE_STEP", 16);

		}

		try
		{
			std::string selectedEngine = settings_ini->Get<std::string>("ENGINE", "selected", "half-life1");
			g_limits = limitsMap[selectedEngine];
		}
		catch (...)
		{
			if (limitsMap.size())
			{
				g_limits = limitsMap.begin()->second;
			}
		}
	}

	conditionalPointEntTriggers.clear();
	entsThatNeverNeedAnyHulls.clear();
	entsThatNeverNeedCollision.clear();
	passableEnts.clear();
	playerOnlyTriggers.clear();
	monsterOnlyTriggers.clear();
	entsNegativePitchPrefix.clear();
	transparentTextures.clear();
	transparentEntities.clear();

	for (int i = 1; i <= settings_ini->Get<int>("OPTIMIZER_COND_ENTS", "count", 0); ++i) {
		std::string item = settings_ini->Get<std::string>("OPTIMIZER_COND_ENTS", std::to_string(i), "");
		if (!item.empty()) {
			conditionalPointEntTriggers.push_back(item);
		}
	}
	for (int i = 1; i <= settings_ini->Get<int>("OPTIMIZER_NO_HULLS_ENTS", "count", 0); ++i) {
		std::string item = settings_ini->Get<std::string>("OPTIMIZER_NO_HULLS_ENTS", std::to_string(i), "");
		if (!item.empty()) {
			entsThatNeverNeedAnyHulls.push_back(item);
		}
	}
	for (int i = 1; i <= settings_ini->Get<int>("OPTIMIZER_NO_COLLISION_ENTS", "count", 0); ++i) {
		std::string item = settings_ini->Get<std::string>("OPTIMIZER_NO_COLLISION_ENTS", std::to_string(i), "");
		if (!item.empty()) {
			entsThatNeverNeedCollision.push_back(item);
		}
	}
	for (int i = 1; i <= settings_ini->Get<int>("OPTIMIZER_PASSABLE_ENTS", "count", 0); ++i) {
		std::string item = settings_ini->Get<std::string>("OPTIMIZER_PASSABLE_ENTS", std::to_string(i), "");
		if (!item.empty()) {
			passableEnts.push_back(item);
		}
	}
	for (int i = 1; i <= settings_ini->Get<int>("OPTIMIZER_PLAYER_HULL_ENTS", "count", 0); ++i) {
		std::string item = settings_ini->Get<std::string>("OPTIMIZER_PLAYER_HULL_ENTS", std::to_string(i), "");
		if (!item.empty()) {
			playerOnlyTriggers.push_back(item);
		}
	}
	for (int i = 1; i <= settings_ini->Get<int>("OPTIMIZER_MONSTER_HULL_ENTS", "count", 0); ++i) {
		std::string item = settings_ini->Get<std::string>("OPTIMIZER_MONSTER_HULL_ENTS", std::to_string(i), "");
		if (!item.empty()) {
			monsterOnlyTriggers.push_back(item);
		}
	}
	for (int i = 1; i <= settings_ini->Get<int>("NEGATIVE_PITCH_ENTS", "count", 0); ++i) {
		std::string item = settings_ini->Get<std::string>("NEGATIVE_PITCH_ENTS", std::to_string(i), "");
		if (!item.empty()) {
			entsNegativePitchPrefix.push_back(item);
		}
	}
	for (int i = 1; i <= settings_ini->Get<int>("TRANSPARENT_TEXTURES", "count", 0); ++i) {
		std::string item = settings_ini->Get<std::string>("TRANSPARENT_TEXTURES", std::to_string(i), "");
		if (!item.empty()) {
			transparentTextures.push_back(item);
		}
	}
	for (int i = 1; i <= settings_ini->Get<int>("TRANSPARENT_ENTITIES", "count", 0); ++i) {
		std::string item = settings_ini->Get<std::string>("TRANSPARENT_ENTITIES", std::to_string(i), "");
		if (!item.empty()) {
			transparentEntities.push_back(item);
		}
	}


	if (g_settings.windowY == -32000 &&
		g_settings.windowX == -32000)
	{
		g_settings.windowY = 200;
		g_settings.windowX = 200;
	}

#ifdef WIN32
	// Fix invisible window header for primary screen.
	if (g_settings.windowY >= 0 && g_settings.windowY < 30)
	{
		g_settings.windowY = 30;
	}
	if (g_settings.windowX == 0 && g_settings.windowY > 0)
	{
		g_settings.windowX = 200;
	}
#endif

	// Restore default window height if invalid.
	if (windowHeight <= 100 || windowWidth <= 100)
	{
		windowHeight = 600;
		windowWidth = 800;
	}

	if (default_is_empty && fgdPaths.empty())
	{
		fgdPaths.emplace_back("/moddir/GameDefinitionFile.fgd", true);
	}

	if (default_is_empty && resPaths.empty())
	{
		resPaths.emplace_back("/moddir/", true);
		resPaths.emplace_back("/moddir_addon/", true);
	}

	if (default_is_empty)
	{
		if (default_is_empty && conditionalPointEntTriggers.empty())
		{
			conditionalPointEntTriggers.clear();
			conditionalPointEntTriggers.push_back("trigger_once");
			conditionalPointEntTriggers.push_back("trigger_multiple");
			conditionalPointEntTriggers.push_back("trigger_counter");
			conditionalPointEntTriggers.push_back("trigger_gravity");
			conditionalPointEntTriggers.push_back("trigger_teleport");
		}
		if (default_is_empty && entsThatNeverNeedAnyHulls.empty())
		{
			entsThatNeverNeedAnyHulls.clear();
			entsThatNeverNeedAnyHulls.push_back("env_bubbles");
			entsThatNeverNeedAnyHulls.push_back("func_tankcontrols");
			entsThatNeverNeedAnyHulls.push_back("func_traincontrols");
			entsThatNeverNeedAnyHulls.push_back("func_vehiclecontrols");
			entsThatNeverNeedAnyHulls.push_back("trigger_autosave"); // obsolete in sven
			entsThatNeverNeedAnyHulls.push_back("trigger_endsection"); // obsolete in sven
		}
		if (default_is_empty && entsThatNeverNeedCollision.empty())
		{
			entsThatNeverNeedCollision.clear();
			entsThatNeverNeedCollision.push_back("func_illusionary");
			entsThatNeverNeedCollision.push_back("func_mortar_field");
		}
		if (default_is_empty && passableEnts.empty())
		{
			passableEnts.clear();
			passableEnts.push_back("func_door");
			passableEnts.push_back("func_door_rotating");
			passableEnts.push_back("func_pendulum");
			passableEnts.push_back("func_tracktrain");
			passableEnts.push_back("func_train");
			passableEnts.push_back("func_water");
			passableEnts.push_back("momentary_door");
		}
		if (default_is_empty && playerOnlyTriggers.empty())
		{
			playerOnlyTriggers.clear();
			playerOnlyTriggers.push_back("func_ladder");
			playerOnlyTriggers.push_back("game_zone_player");
			playerOnlyTriggers.push_back("player_respawn_zone");
			playerOnlyTriggers.push_back("trigger_cdaudio");
			playerOnlyTriggers.push_back("trigger_changelevel");
			playerOnlyTriggers.push_back("trigger_transition");
		}
		if (default_is_empty && monsterOnlyTriggers.empty())
		{
			monsterOnlyTriggers.clear();
			monsterOnlyTriggers.push_back("func_monsterclip");
			monsterOnlyTriggers.push_back("trigger_monsterjump");
		}
		if (default_is_empty && entsNegativePitchPrefix.empty())
		{
			entsNegativePitchPrefix.clear();
			entsNegativePitchPrefix.push_back("ammo_");
			entsNegativePitchPrefix.push_back("cycler");
			entsNegativePitchPrefix.push_back("item_");
			entsNegativePitchPrefix.push_back("monster_");
			entsNegativePitchPrefix.push_back("weaponbox");
			entsNegativePitchPrefix.push_back("worlditems");
			entsNegativePitchPrefix.push_back("xen_");
		}
	}

	if (default_is_empty && transparentTextures.empty())
	{
		transparentTextures.push_back("AAATRIGGER");
	}

	if (default_is_empty && transparentEntities.empty())
	{
		transparentEntities.push_back("func_buyzone");
	}


	FixupAllSystemPaths();
	reload_ents_list = false;
}

void Settings::saveSettings(std::string path) 
{
	std::lock_guard<std::mutex> lock(g_mutex_list[7]);

	removeFile(path);

	std::ofstream iniFile(path, std::ios::binary);

	if (!iniFile.is_open()) 
	{
		print_log(PRINT_RED, "Can't open " + path + " for writing\n");
		return;
	}

	std::ostringstream iniData{};
	iniData << "[GENERAL]\n";
	iniData << "lang=" << selected_lang << "\n";
	iniData << "window_width=" << g_settings.windowWidth << "\n";
	iniData << "window_height=" << g_settings.windowHeight << "\n";
	iniData << "window_x=" << g_settings.windowX << "\n";
	iniData << "window_y=" << g_settings.windowY << "\n";
	iniData << "window_maximized=" << g_settings.maximized << "\n";
	iniData << "start_at_entity=" << g_settings.start_at_entity << "\n";
	iniData << "savebackup=" << g_settings.savebackup << "\n";
	iniData << "save_crc=" << g_settings.save_crc << "\n";
	iniData << "save_cam=" << g_settings.save_cam << "\n";
	iniData << "auto_import_ent=" << g_settings.auto_import_ent << "\n";
	iniData << "same_dir_for_ent=" << g_settings.same_dir_for_ent << "\n";
	iniData << "reload_ents_list=" << g_settings.reload_ents_list << "\n";
	iniData << "strip_wad_path=" << g_settings.strip_wad_path << "\n";
	iniData << "default_is_empty=" << g_settings.default_is_empty << "\n";
	iniData << "undo_levels=" << g_settings.undoLevels << "\n\n";

	iniData << "[WIDGETS]\n";
	iniData << "save_windows=" << g_settings.save_windows << "\n";
	iniData << "debug_open=" << g_settings.debug_open << "\n";
	iniData << "keyvalue_open=" << g_settings.keyvalue_open << "\n";
	iniData << "transform_open=" << g_settings.transform_open << "\n";
	iniData << "log_open=" << g_settings.log_open << "\n";
	iniData << "limits_open=" << g_settings.limits_open << "\n";
	iniData << "entreport_open=" << g_settings.entreport_open << "\n";
	iniData << "texbrowser_open=" << g_settings.texbrowser_open << "\n";
	iniData << "goto_open=" << g_settings.goto_open << "\n";
	iniData << "settings_tab=" << g_settings.settings_tab << "\n\n";

	iniData << "[GRAPHICS]\n";
	iniData << "vsync=" << g_settings.vsync << "\n";
	iniData << "fov=" << g_settings.fov << "\n";
	iniData << "zfar=" << g_settings.zfar << "\n";
	iniData << "renders_flags=" << g_settings.render_flags << "\n";
	iniData << "font_size=" << g_settings.fontSize << "\n";
	iniData << "fpslimit=" << g_settings.fpslimit << "\n\n";

	iniData << "[OPTIMIZE]\n";
	iniData << "mark_unused_texinfos=" << g_settings.mark_unused_texinfos << "\n";
	iniData << "merge_verts=" << g_settings.merge_verts << "\n";
	iniData << "merge_edges=" << g_settings.merge_edges << "\n\n";

	iniData << "[INPUT]\n";
	iniData << "move_speed=" << g_settings.moveSpeed << "\n";
	iniData << "rot_speed=" << g_settings.rotSpeed << "\n\n";

	iniData << "[PATHS]\n";
	iniData << "gamedir=" << g_settings.gamedir << "\n";
	iniData << "workingdir=" << g_settings.workingdir << "\n";
	iniData << "lastdir=" << g_settings.lastdir << "\n";
	iniData << "hlrad_path=" << g_settings.rad_path << "\n";
	iniData << "palette=" << g_settings.palette_name << "\n\n";

	iniData << "[FGD]\n";
	iniData << "count=" << g_settings.fgdPaths.size() << "\n";
	for (size_t i = 0; i < g_settings.fgdPaths.size(); ++i) {
		iniData << (i + 1) << "=" << (g_settings.fgdPaths[i].enabled ? "enabled" : "disabled") << "?" << g_settings.fgdPaths[i].path << "\n";
	}
	iniData << "\n";

	iniData << "[RES]\n";
	iniData << "count=" << g_settings.resPaths.size() << "\n";
	for (size_t i = 0; i < g_settings.resPaths.size(); ++i) {
		iniData << (i + 1) << "=" << (g_settings.resPaths[i].enabled ? "enabled" : "disabled") << "?" << g_settings.resPaths[i].path << "\n";
	}
	iniData << "\n";

	iniData << "[DEBUG]\n";
	iniData << "verbose_logs=" << (g_settings.verboseLogs ? 1 : 0) << "\n";

	iniData << "\n\n";

	iniData << "[LAST_OPENED]\n";

	int openCount = 0;

	if (lastOpened.size())
		std::reverse(lastOpened.begin(), lastOpened.end());
	
	for (auto & file : lastOpened)
	{
		if (fileExists(file))
		{
			openCount++;
			if (openCount == 10)
				break;
		}
	}

	int id = 1;
	iniData << "count=" << openCount << "\n";

	for (auto& file : lastOpened)
	{
		if (fileExists(file))
		{
			iniData << "file_" + std::to_string(id) << "=" << file << "\n";

			if (id == 10)
				break;

			id++;
		}
	}
	iniData << "\n\n";

	if (lastOpened.size())
		std::reverse(lastOpened.begin(), lastOpened.end());

	id = 1;

	iniData << "[ENGINE]\n";
	iniData << "count=" << limitsMap.size() << "\n";

	for (const auto& pair : limitsMap) {
		iniData << "name" + std::to_string(id) << "=" << pair.first << "\n";
		id++;
	}

	iniData << "selected=" << g_limits.engineName << "\n\n";

	id = 1;
	for (const auto& pair : limitsMap) {
		const std::string engineName = pair.first;
		const BSPLimits& limits = pair.second;

		iniData << "[" << engineName << "]\n";
		iniData << "FLT_MAX_COORD=" << limits.fltMaxCoord << "\n";
		iniData << "MAX_MAP_MODELS=" << limits.maxMapModels << "\n";
		iniData << "MAX_SURFACE_EXTENT=" << limits.maxSurfaceExtent << "\n";
		iniData << "MAX_MAP_NODES=" << limits.maxMapNodes << "\n";
		iniData << "MAX_MAP_CLIPNODES=" << limits.maxMapClipnodes << "\n";
		iniData << "MAX_MAP_LEAVES=" << limits.maxMapLeaves << "\n";
		iniData << "MAX_MAP_VISDATA=" << (limits.maxMapVisdata / (1024 * 1024)) << "\n";
		iniData << "MAX_MAP_ENTS=" << limits.maxMapEnts << "\n";
		iniData << "MAX_MAP_SURFEDGES=" << limits.maxMapSurfedges << "\n";
		iniData << "MAX_MAP_EDGES=" << limits.maxMapEdges << "\n";
		iniData << "MAX_MAP_TEXTURES=" << limits.maxMapTextures << "\n";
		iniData << "MAX_MAP_LIGHTDATA=" << (limits.maxMapLightdata / (1024 * 1024)) << "\n";
		iniData << "MAX_TEXTURE_DIMENSION=" << limits.maxTextureDimension << "\n";
		iniData << "MAX_MAP_BOUNDARY=" << limits.maxMapBoundary << "\n";
		iniData << "TEXTURE_STEP=" << limits.textureStep << "\n";
		iniData << "\n";
		id ++;
	}

	auto writeListSection = [&iniData](const std::string& section, const std::vector<std::string>& list) {
		iniData << "[" << section << "]\n";
		iniData << "count=" << list.size() << "\n";
		for (size_t i = 0; i < list.size(); ++i) {
			iniData << (i + 1) << "=" << list[i] << "\n";
		}
		iniData << "\n";
		};

	writeListSection("OPTIMIZER_COND_ENTS", conditionalPointEntTriggers);
	writeListSection("OPTIMIZER_NO_HULLS_ENTS", entsThatNeverNeedAnyHulls);
	writeListSection("OPTIMIZER_NO_COLLISION_ENTS", entsThatNeverNeedCollision);
	writeListSection("OPTIMIZER_PASSABLE_ENTS", passableEnts);
	writeListSection("OPTIMIZER_PLAYER_HULL_ENTS", playerOnlyTriggers);
	writeListSection("OPTIMIZER_MONSTER_HULL_ENTS", monsterOnlyTriggers);
	writeListSection("NEGATIVE_PITCH_ENTS", entsNegativePitchPrefix);
	writeListSection("TRANSPARENT_TEXTURES", transparentTextures);
	writeListSection("TRANSPARENT_ENTITIES", transparentEntities);

	iniData.flush();

	iniFile.write(iniData.str().c_str(), iniData.str().size());
	iniFile.flush();
	iniFile.close();

	using namespace std::chrono_literals;
	std::this_thread::sleep_for(100ms);
}

void Settings::saveSettings()
{
	FixupAllSystemPaths();
	g_app->saveGuiSettings();
	saveSettings(g_settings_path);
}

std::string convertToSection(std::string& key) {
	if (key == "window_width" || key == "window_height" || key == "window_x" ||
		key == "window_y" || key == "window_maximized" || key == "save_windows" ||
		key == "debug_open" || key == "keyvalue_open" || key == "transform_open" ||
		key == "log_open" || key == "limits_open" || key == "entreport_open" ||
		key == "texbrowser_open" || key == "goto_open" || key == "settings_tab" ||
		key == "start_at_entity" || key == "savebackup" || key == "save_crc" ||
		key == "save_cam" || key == "auto_import_ent" || key == "same_dir_for_ent" ||
		key == "reload_ents_list" || key == "strip_wad_path" || key == "default_is_empty" ||
		key == "undo_levels" || key == "language") {
		if (key == "language")
			key = "lang";
		return "GENERAL";
	}
	if (key == "vsync" || key == "fov" || key == "zfar" || key == "renders_flags" ||
		key == "font_size" || key == "fpslimit" || key == "mark_unused_texinfos" ||
		key == "merge_verts" || key == "merge_edges") {
		return "GRAPHICS";
	}
	if (key == "move_speed" || key == "rot_speed") {
		return "INPUT";
	}
	if (key == "gamedir" || key == "workingdir" || key == "lastdir" ||
		key == "hlrad_path" || key == "palette") {
		return "PATHS";
	}
	if (key == "fgd" || key == "res") {
		return toUpperCase(key);
	}
	if (key == "g_limits.fltMaxCoord" || key == "g_limits.maxMapModels" || key == "g_limits.maxSurfaceExtent" ||
		key == "g_limits.maxMapNodes" || key == "g_limits.maxMapClipnodes" || key == "g_limits.maxMapLeaves" ||
		key == "g_limits.maxMapVisdata" || key == "g_limits.maxMapEnts" || key == "g_limits.maxMapSurfedges" ||
		key == "g_limits.maxMapEdges" || key == "g_limits.maxMapTextures" || key == "g_limits.maxMapLightdata" ||
		key == "g_limits.maxTextureDimension" || key == "g_limits.maxTextureSize" || key == "g_limits.maxMapBoundary" ||
		key == "g_limits.textureStep") {
		return "LIMITS";
	}
	if (key == "optimizer_cond_ents" || key == "optimizer_no_hulls_ents" || key == "optimizer_no_collision_ents" ||
		key == "optimizer_passable_ents" || key == "optimizer_player_hull_ents" || key == "optimizer_monster_hull_ents" ||
		key == "negative_pitch_ents" || key == "transparent_textures" || key == "transparent_entities") {
		return toUpperCase(key);
	}
	if (key == "verbose_logs") {
		return "DEBUG";
	}
	return "GENERAL";
}

std::string ConvertFromCFGtoINI(const std::string& cfgData) {
	std::istringstream cfgStream(cfgData);
	std::string line;
	std::map<std::string, std::vector<std::string>> cfgMap;

	while (getline(cfgStream, line)) {
		if (line.empty() || line.find('=') == std::string::npos) {
			continue;
		}

		size_t eqPos = line.find('=');
		std::string key = trimSpaces(line.substr(0, eqPos));
		std::string value = trimSpaces(line.substr(eqPos + 1));

		cfgMap[key].push_back(value);
	}

	std::ostringstream iniStream;
	std::map<std::string, std::ostringstream> sections;

	for (const auto& entry : cfgMap) {
		std::string key = entry.first;
		std::string section = convertToSection(key);
		if (sections.find(section) == sections.end()) {
			sections[section] << "[" << section << "]\n";
		}
		if (entry.second.size() > 1 || section == toUpperCase(key)) {
			sections[section] << "count = " << entry.second.size() << "\n";
			for (size_t i = 0; i < entry.second.size(); ++i) {
				sections[section] << (i + 1) << " = " << entry.second[i] << "\n";
			}
		}
		else {
			sections[section] << key << " = " << entry.second[0] << "\n";
		}
	}

	std::vector<std::string> order = { "GENERAL", "GRAPHICS", "INPUT", "LIMITS", "PATHS" };
	for (const auto& section : order) {
		if (sections.find(section) != sections.end()) {
			iniStream << sections[section].str() << "\n";
			sections.erase(section);
		}
	}

	for (const auto& section : sections) {
		iniStream << section.second.str() << "\n";
	}

	return iniStream.str();
}
