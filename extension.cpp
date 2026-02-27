#include "extension.h"
#include <filesystem.h>
#include <sh_vector.h>

ModeGroupExtension g_ModeGroup;
SMEXT_LINK(&g_ModeGroup);

// Handle types for modes (if needed)
HandleType_t g_ModeType = 0;

ModeGroupExtension::ModeGroupExtension()
	: m_IsSwitching(false)
{
}

ModeGroupExtension::~ModeGroupExtension()
{
}

bool ModeGroupExtension::SDK_OnLoad(char *error, size_t maxlen, bool late)
{
	// Initialize necessary interfaces
	sharesys->AddInterface(myself, this);
	sharesys->RegisterLibrary(myself, "modegroup");

	// Load configuration on startup
	if (!LoadConfig())
	{
		snprintf(error, maxlen, "Failed to load config file");
		return false;
	}

	// Register plugin listener
	playerhelpers->GetPluginManager()->AddPluginsListener(this);

	// Register console command
	RegAdminCmd("sm_mode", OnModeCommand, ADMFLAG_ROOT, "Switch game mode (see modegroup.cfg)");

	return true;
}

void ModeGroupExtension::SDK_OnAllLoaded()
{
	// If late loaded, we might need to initialize current mode
	// For simplicity, we don't set any default mode here.
}

void ModeGroupExtension::SDK_OnUnload()
{
	// Unload all plugins loaded by this extension
	UnloadCurrentModePlugins();

	// Remove listener
	playerhelpers->GetPluginManager()->RemovePluginsListener(this);
}

void ModeGroupExtension::SDK_OnPauseChange(bool paused)
{
	// Not used
}

// ------------------------------------------------------------
// IPluginsListener implementation
// ------------------------------------------------------------
void ModeGroupExtension::OnPluginLoaded(IPlugin *plugin)
{
	if (m_IsSwitching)
	{
		// This plugin is loaded by our switching process
		m_LoadedPlugins.push_back(plugin);
	}
}

void ModeGroupExtension::OnPluginUnloaded(IPlugin *plugin)
{
	// Remove from our list if present (e.g., if manually unloaded)
	for (auto it = m_LoadedPlugins.begin(); it != m_LoadedPlugins.end(); ++it)
	{
		if (*it == plugin)
		{
			m_LoadedPlugins.erase(it);
			break;
		}
	}
}

// ------------------------------------------------------------
// Config loading
// ------------------------------------------------------------
bool ModeGroupExtension::LoadConfig()
{
	char path[PLATFORM_MAX_PATH];
	g_pSM->BuildPath(Path_SM, path, sizeof(path), "configs/modegroup.cfg");

	IKeyValues *kv = new CKeyValues("ModeGroups");
	if (!kv->LoadFromFile(g_pSM->GetFileSystem(), path))
	{
		delete kv;
		return false;
	}

	// The root key must be "ModeGroups"
	if (!kv->JumpToKey("ModeGroups"))
	{
		delete kv;
		return false;
	}

	// Iterate through all subkeys (mode names)
	for (IKeyValues *sub = kv->GetFirstSubKey(); sub; sub = sub->GetNextKey())
	{
		ModeInfo info;
		info.name = sub->GetName();

		// plugin_directory
		const char *pluginDir = sub->GetString("plugin_directory", NULL);
		if (pluginDir && pluginDir[0])
		{
			CollectPluginsFromDir(pluginDir, info.pluginFiles, true);
		}

		// plugins list
		if (sub->JumpToKey("plugins"))
		{
			for (IKeyValues *p = sub->GetFirstSubKey(); p; p = p->GetNextKey())
			{
				const char *fileName = p->GetString();
				if (fileName && fileName[0])
				{
					info.pluginFiles.push_back(fileName);
				}
			}
			sub->GoBack();
		}

		// cvars
		if (sub->JumpToKey("cvars"))
		{
			for (IKeyValues *p = sub->GetFirstSubKey(); p; p = p->GetNextKey())
			{
				info.cvars[p->GetName()] = p->GetString();
			}
			sub->GoBack();
		}

		// commands
		if (sub->JumpToKey("commands"))
		{
			for (IKeyValues *p = sub->GetFirstSubKey(); p; p = p->GetNextKey())
			{
				const char *cmd = p->GetString();
				if (cmd && cmd[0])
				{
					info.commands.push_back(cmd);
				}
			}
			sub->GoBack();
		}

		m_Modes.push_back(info);
	}

	delete kv;
	return true;
}

// ------------------------------------------------------------
// Recursive plugin collection
// ------------------------------------------------------------
void ModeGroupExtension::CollectPluginsFromDir(const char *relativeDir, std::vector<std::string> &outFiles, bool recursive)
{
	char fullPath[PLATFORM_MAX_PATH];
	g_pSM->BuildPath(Path_SM, fullPath, sizeof(fullPath), "plugins/%s", relativeDir);

	IDirectory *dir = libsys->OpenDirectory(fullPath);
	if (!dir)
		return;

	while (dir->MoreFiles())
	{
		if (dir->IsEntryDirectory())
		{
			if (recursive && strcmp(dir->GetEntryName(), ".") != 0 && strcmp(dir->GetEntryName(), "..") != 0)
			{
				char subDir[PLATFORM_MAX_PATH];
				g_pSM->Format(subDir, sizeof(subDir), "%s/%s", relativeDir, dir->GetEntryName());
				CollectPluginsFromDir(subDir, outFiles, recursive);
			}
		}
		else
		{
			const char *name = dir->GetEntryName();
			if (strstr(name, ".smx") || strstr(name, ".SMX"))
			{
				char relativePath[PLATFORM_MAX_PATH];
				g_pSM->Format(relativePath, sizeof(relativePath), "%s/%s", relativeDir, name);
				outFiles.push_back(relativePath);
			}
		}
		dir->NextEntry();
	}
	dir->Close();
	delete dir;
}

// ------------------------------------------------------------
// Mode switching
// ------------------------------------------------------------
bool ModeGroupExtension::SwitchToMode(const char *modeName)
{
	// Find mode
	const ModeInfo *newMode = nullptr;
	for (const auto &mode : m_Modes)
	{
		if (mode.name == modeName)
		{
			newMode = &mode;
			break;
		}
	}
	if (!newMode)
		return false;

	// Unload previous mode plugins
	UnloadCurrentModePlugins();

	// Set flag to capture plugin loads
	m_IsSwitching = true;

	// Load new mode plugins
	LoadModePlugins(newMode->pluginFiles);

	// Apply cvars/commands
	ApplyModeSettings(*newMode);

	// Done switching
	m_IsSwitching = false;
	m_CurrentMode = modeName;

	return true;
}

void ModeGroupExtension::UnloadCurrentModePlugins()
{
	IPluginManager *mgr = playerhelpers->GetPluginManager();
	for (IPlugin *pl : m_LoadedPlugins)
	{
		mgr->UnloadPlugin(pl);
	}
	m_LoadedPlugins.clear();
}

void ModeGroupExtension::LoadModePlugins(const std::vector<std::string> &files)
{
	IPluginManager *mgr = playerhelpers->GetPluginManager();
	for (const std::string &file : files)
	{
		char error[255];
		bool wasLoaded = false;
		IPlugin *pl = mgr->LoadPlugin(file.c_str(), false, PluginType_Private, error, sizeof(error), &wasLoaded);
		if (!pl)
		{
			g_pSM->LogError(myself, "Failed to load %s: %s", file.c_str(), error);
		}
		// Note: OnPluginLoaded will add to m_LoadedPlugins automatically if m_IsSwitching is true
	}
}

void ModeGroupExtension::ApplyModeSettings(const ModeInfo &mode)
{
	// Set cvars
	for (const auto &it : mode.cvars)
	{
		ConVar *cvar = g_pCVar->FindConVar(it.first.c_str());
		if (cvar)
		{
			cvar->SetString(it.second.c_str());
		}
		else
		{
			g_pSM->LogError(myself, "Cvar %s not found", it.first.c_str());
		}
	}

	// Execute commands
	for (const std::string &cmd : mode.commands)
	{
		g_pSM->InsertServerCommand(cmd.c_str());
	}
}

void ModeGroupExtension::ClearPluginList()
{
	m_LoadedPlugins.clear();
}

// ------------------------------------------------------------
// Console command handler
// ------------------------------------------------------------
static cell_t OnModeCommand(IPluginContext *pContext, const cell_t *params)
{
	char modeName[64];
	pContext->LocalToString(params[1], modeName, sizeof(modeName));

	if (g_ModeGroup.SwitchToMode(modeName))
	{
		pContext->ReportError("Switched to mode '%s'", modeName);
		return 1;
	}
	else
	{
		pContext->ReportError("Unknown mode or switch failed");
		return 0;
	}
}