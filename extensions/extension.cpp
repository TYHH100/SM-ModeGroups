#include "extension.h"
#include <sh_string.h>
#include <ITextParsers.h>
#include <IGameHelpers.h>

ModeGroupExtension g_ModeGroupExtension;

SMEXT_LINK(&g_ModeGroupExtension);

extern sp_nativeinfo_t g_Natives[];

class ModeGroupConfigParser : public ITextListener_SMC
{
public:
	ModeGroupConfigParser(std::map<std::string, ModeGroup> &groups) 
		: m_Groups(groups), m_InModeGroups(false), m_InCvars(false), m_InCommands(false), m_InLoadPlugins(false), m_InUnloadPlugins(false)
	{
	}

	void ReadSMC_ParseStart()
	{
		m_CurrentGroup.name.clear();
		m_CurrentGroup.plugin_directory.clear();
		m_CurrentGroup.plugin_files.clear();
		m_CurrentGroup.load_plugins.clear();
		m_CurrentGroup.unload_plugins.clear();
		m_CurrentGroup.use_sm_cvar = true;
		m_CurrentGroup.cvars.clear();
		m_CurrentGroup.commands.clear();
		m_InModeGroups = false;
		m_InCvars = false;
		m_InCommands = false;
		m_InLoadPlugins = false;
		m_InUnloadPlugins = false;
	}

	SMCResult ReadSMC_NewSection(const SMCStates *states, const char *name)
	{
		if (strcmp(name, "ModeGroups") == 0)
		{
			m_InModeGroups = true;
			return SMCResult_Continue;
		}

		if (m_InModeGroups && !m_CurrentGroup.name.empty() && strcmp(name, "cvars") == 0)
		{
			m_InCvars = true;
			return SMCResult_Continue;
		}

		if (m_InModeGroups && !m_CurrentGroup.name.empty() && strcmp(name, "commands") == 0)
		{
			m_InCommands = true;
			return SMCResult_Continue;
		}

		if (m_InModeGroups && !m_CurrentGroup.name.empty() && strcmp(name, "load_plugins") == 0)
		{
			m_InLoadPlugins = true;
			return SMCResult_Continue;
		}

		if (m_InModeGroups && !m_CurrentGroup.name.empty() && strcmp(name, "unload_plugins") == 0)
		{
			m_InUnloadPlugins = true;
			return SMCResult_Continue;
		}

		if (m_InModeGroups)
		{
			m_CurrentGroup.name = name;
			return SMCResult_Continue;
		}

		return SMCResult_Continue;
	}

	SMCResult ReadSMC_KeyValue(const SMCStates *states, const char *key, const char *value)
	{
		if (m_CurrentGroup.name.empty())
			return SMCResult_Continue;

		if (m_InCvars)
		{
			m_CurrentGroup.cvars[key] = value;
		}
		else if (m_InCommands)
		{
			m_CurrentGroup.commands[key] = value;
		}
		else if (m_InLoadPlugins)
		{
			m_CurrentGroup.load_plugins.push_back(value);
		}
		else if (m_InUnloadPlugins)
		{
			m_CurrentGroup.unload_plugins.push_back(value);
		}
		else if (strcmp(key, "plugin_directory") == 0)
		{
			m_CurrentGroup.plugin_directory = value;
		}
		else if (strcmp(key, "use_sm_cvar") == 0)
		{
			m_CurrentGroup.use_sm_cvar = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
		}

		return SMCResult_Continue;
	}

	SMCResult ReadSMC_LeavingSection(const SMCStates *states)
	{
		if (m_InCvars)
		{
			m_InCvars = false;
		}
		else if (m_InCommands)
		{
			m_InCommands = false;
		}
		else if (m_InLoadPlugins)
		{
			m_InLoadPlugins = false;
		}
		else if (m_InUnloadPlugins)
		{
			m_InUnloadPlugins = false;
		}
		else if (!m_CurrentGroup.name.empty())
		{
			m_Groups[m_CurrentGroup.name] = m_CurrentGroup;
			m_CurrentGroup.name.clear();
			m_CurrentGroup.plugin_directory.clear();
			m_CurrentGroup.plugin_files.clear();
			m_CurrentGroup.load_plugins.clear();
			m_CurrentGroup.unload_plugins.clear();
			m_CurrentGroup.use_sm_cvar = true; // 重置为默认值
			m_CurrentGroup.cvars.clear();
			m_CurrentGroup.commands.clear();
		}
		else if (m_InModeGroups)
		{
			m_InModeGroups = false;
		}

		return SMCResult_Continue;
	}

	void ReadSMC_ParseEnd(bool halted, bool failed)
	{
	}

private:
	std::map<std::string, ModeGroup> &m_Groups;
	ModeGroup m_CurrentGroup;
	bool m_InModeGroups;
	bool m_InCvars;
	bool m_InCommands;
	bool m_InLoadPlugins;
	bool m_InUnloadPlugins;
};

bool ModeGroupExtension::SDK_OnLoad(char *error, size_t maxlen, bool late)
{
	if (!LoadConfig(error, maxlen))
	{
		return false;
	}

	sharesys->AddNatives(myself, g_Natives);

	m_pModeGroupChangedForward = forwards->CreateForward("OnModeGroupChanged", ET_Ignore, 2, NULL, Param_String, Param_String);

	rootconsole->AddRootConsoleCommand3("modegroup", "Manage Mode Groups", this);

	g_pSM->LogMessage(myself, "Mode Group Manager loaded successfully");

	return true;
}

void ModeGroupExtension::SDK_OnUnload()
{
	UnloadCurrentModeGroup();

	if (m_pModeGroupChangedForward)
	{
		forwards->ReleaseForward(m_pModeGroupChangedForward);
		m_pModeGroupChangedForward = NULL;
	}

	rootconsole->RemoveRootConsoleCommand("modegroup", this);

	g_pSM->LogMessage(myself, "Mode Group Manager unloaded");
}

void ModeGroupExtension::SDK_OnAllLoaded()
{
}

bool ModeGroupExtension::QueryRunning(char *error, size_t maxlen)
{
	return true;
}

bool ModeGroupExtension::LoadConfig(char *error, size_t maxlen)
{
	char path[PLATFORM_MAX_PATH];
	g_pSM->BuildPath(Path_SM, path, sizeof(path), "configs/modegroup.cfg");

	ModeGroupConfigParser parser(m_ModeGroups);
	SMCStates states;
	char smcError[256];

	SMCError err = textparsers->ParseSMCFile(path, &parser, &states, smcError, sizeof(smcError));
	if (err != SMCError_Okay)
	{
		ke::SafeSprintf(error, maxlen, "Failed to parse modegroup.cfg: %s (line %u, col %u)", 
			smcError, states.line, states.col);
		return false;
	}

	g_pSM->LogMessage(myself, "Loaded %zu mode groups", m_ModeGroups.size());

	return true;
}

bool ModeGroupExtension::SwitchModeGroup(const char *groupName)
{
	std::string oldGroup = m_CurrentModeGroup;

	UnloadCurrentModeGroup();

	std::map<std::string, ModeGroup>::iterator it = m_ModeGroups.find(groupName);
	if (it == m_ModeGroups.end())
	{
		g_pSM->LogError(myself, "Mode group '%s' not found", groupName);
		return false;
	}

	LoadModeGroup(it->second);

	m_CurrentModeGroup = groupName;

	if (m_pModeGroupChangedForward)
	{
		m_pModeGroupChangedForward->PushString(oldGroup.c_str());
		m_pModeGroupChangedForward->PushString(groupName);
		m_pModeGroupChangedForward->Execute(NULL);
	}

	g_pSM->LogMessage(myself, "Switched to mode group: %s", groupName);

	return true;
}

void ModeGroupExtension::UnloadCurrentModeGroup()
{
	if (m_CurrentModeGroup.empty())
		return;

	for (size_t i = 0; i < m_LoadedPlugins.size(); i++)
	{
		UnloadPlugin(m_LoadedPlugins[i].c_str());
	}

	m_LoadedPlugins.clear();
	m_CurrentModeGroup.clear();
}

void ModeGroupExtension::LoadModeGroup(const ModeGroup &group)
{
	if (!group.plugin_directory.empty())
	{
		std::vector<std::string> plugins;
		ScanDirectoryForPlugins(group.plugin_directory.c_str(), plugins);

		for (size_t i = 0; i < plugins.size(); i++)
		{
			if (LoadPlugin(plugins[i].c_str()))
			{
				m_LoadedPlugins.push_back(plugins[i]);
			}
		}
	}

	// 加载手动指定的插件
	for (size_t i = 0; i < group.load_plugins.size(); i++)
	{
		if (LoadPlugin(group.load_plugins[i].c_str()))
		{
			m_LoadedPlugins.push_back(group.load_plugins[i]);
		}
	}

	// 卸载手动指定的插件
	for (size_t i = 0; i < group.unload_plugins.size(); i++)
	{
		UnloadPlugin(group.unload_plugins[i].c_str());
	}

	for (std::map<std::string, std::string>::const_iterator it = group.cvars.begin();
		it != group.cvars.end(); ++it)
	{
		char cmd[256];
		if (group.use_sm_cvar)
		{
			ke::SafeSprintf(cmd, sizeof(cmd), "sm_cvar %s %s\n", it->first.c_str(), it->second.c_str());
		}
		else
		{
			ke::SafeSprintf(cmd, sizeof(cmd), "%s %s\n", it->first.c_str(), it->second.c_str());
		}
		gamehelpers->ServerCommand(cmd);
		g_pSM->LogMessage(myself, "Set Cvar %s to %s", it->first.c_str(), it->second.c_str());
	}

	for (std::map<std::string, std::string>::const_iterator it = group.commands.begin();
		it != group.commands.end(); ++it)
	{
		if (strcmp(it->first.c_str(), "command") == 0)
		{
			char cmd[256];
			ke::SafeSprintf(cmd, sizeof(cmd), "%s\n", it->second.c_str());
			gamehelpers->ServerCommand(cmd);
			g_pSM->LogMessage(myself, "Executed Command: %s", it->second.c_str());
		}
		else
		{
			char cmd[256];
			ke::SafeSprintf(cmd, sizeof(cmd), "%s %s\n", it->first.c_str(), it->second.c_str());
			gamehelpers->ServerCommand(cmd);
			g_pSM->LogMessage(myself, "Executed Command: %s", cmd);
		}
	}
}

void ModeGroupExtension::ScanDirectoryForPlugins(const char *path, std::vector<std::string> &plugins)
{
	char fullPath[PLATFORM_MAX_PATH];
	g_pSM->BuildPath(Path_SM, fullPath, sizeof(fullPath), "plugins/%s", path);

	IDirectory *dir = libsys->OpenDirectory(fullPath);
	if (!dir)
	{
		g_pSM->LogError(myself, "Could not open directory: %s", fullPath);
		return;
	}

	while (dir->MoreFiles())
	{
		const char *name = dir->GetEntryName();
		bool isDir = dir->IsEntryDirectory();

		if (isDir && strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
		{
			char subPath[PLATFORM_MAX_PATH];
			ke::SafeSprintf(subPath, sizeof(subPath), "%s/%s", path, name);
			ScanDirectoryForPlugins(subPath, plugins);
		}
		else if (!isDir)
		{
			size_t len = strlen(name);
			if (len > 4 && strcasecmp(name + len - 4, ".smx") == 0)
			{
				char pluginPath[PLATFORM_MAX_PATH];
				ke::SafeSprintf(pluginPath, sizeof(pluginPath), "%s/%s", path, name);
				plugins.push_back(pluginPath);
			}
		}

		dir->NextEntry();
	}

	libsys->CloseDirectory(dir);
}

bool ModeGroupExtension::LoadPlugin(const char *path)
{
	char fullPath[PLATFORM_MAX_PATH];
	g_pSM->BuildPath(Path_SM, fullPath, sizeof(fullPath), "plugins/%s", path);

	char error[256];
	bool wasloaded;
	IPlugin *pPlugin = plsys->LoadPlugin(path, false, PluginType_MapUpdated, error, sizeof(error), &wasloaded);
	if (!pPlugin)
	{
		g_pSM->LogError(myself, "Failed to load plugin %s: %s", path, error);
		return false;
	}

	g_pSM->LogMessage(myself, "Loaded plugin: %s", path);
	return true;
}

void ModeGroupExtension::UnloadPlugin(const char *path)
{
	char fullPath[PLATFORM_MAX_PATH];
	g_pSM->BuildPath(Path_SM, fullPath, sizeof(fullPath), "plugins/%s", path);

	IPlugin *pPlugin = NULL;
	IPluginIterator *iter = plsys->GetPluginIterator();
	while (iter->MorePlugins())
	{
		IPlugin *p = iter->GetPlugin();
		if (strcmp(p->GetFilename(), path) == 0)
		{
			pPlugin = p;
			break;
		}
		iter->NextPlugin();
	}
	iter->Release();

	if (!pPlugin)
	{
		return;
	}

	if (pPlugin->GetStatus() != Plugin_Running)
	{
		return;
	}

	if (!plsys->UnloadPlugin(pPlugin))
	{
		g_pSM->LogError(myself, "Failed to unload plugin %s", path);
		return;
	}

	g_pSM->LogMessage(myself, "Unloaded plugin: %s", path);
}

void ModeGroupExtension::ReloadConfig()
{
	UnloadCurrentModeGroup();
	m_ModeGroups.clear();

	char error[256];
	if (LoadConfig(error, sizeof(error)))
	{
		g_pSM->LogMessage(myself, "Configuration reloaded successfully");
	}
	else
	{
		g_pSM->LogError(myself, "Failed to reload configuration: %s", error);
	}
}

void ModeGroupExtension::ListModeGroups()
{
	rootconsole->ConsolePrint("Available mode groups:");
	for (std::map<std::string, ModeGroup>::iterator it = m_ModeGroups.begin();
		it != m_ModeGroups.end(); ++it)
	{
		rootconsole->ConsolePrint("  - %s", it->first.c_str());
	}
}

void ModeGroupExtension::CurrentModeGroup()
{
	if (m_CurrentModeGroup.empty())
	{
		rootconsole->ConsolePrint("No mode group currently active");
	}
	else
	{
		rootconsole->ConsolePrint("Current mode group: %s", m_CurrentModeGroup.c_str());
	}
}

const char *ModeGroupExtension::GetCurrentModeGroupName()
{
	return m_CurrentModeGroup.c_str();
}

void ModeGroupExtension::OnRootConsoleCommand(const char *cmdname, const ICommandArgs *args)
{
	if (args->ArgC() == 2)
	{
		rootconsole->ConsolePrint("Mode Group Manager Menu:");
		rootconsole->ConsolePrint("Usage: sm modegroup [arguments]");
		rootconsole->ConsolePrint("    switch              - Switch to a mode group");
		rootconsole->ConsolePrint("    reload              - Reload mode group configuration");
		rootconsole->ConsolePrint("    list                - List available mode groups");
		rootconsole->ConsolePrint("    current             - Show current mode group");
		return;
	}
	else if (args->ArgC() >= 3)
	{
		const char *subcmd = args->Arg(2);
		if (strcmp(subcmd, "switch") == 0)
		{
			if (args->ArgC() < 4)
			{
				rootconsole->ConsolePrint("Usage: sm modegroup switch <groupname>");
				return;
			}
			
			SwitchModeGroup(args->Arg(3));
		}
		else if (strcmp(subcmd, "reload") == 0)
		{
			ReloadConfig();
		}
		else if (strcmp(subcmd, "list") == 0)
		{
			ListModeGroups();
		}
		else if (strcmp(subcmd, "current") == 0)
		{
			CurrentModeGroup();
		}
	}
}

cell_t Native_SwitchModeGroup(IPluginContext *pContext, const cell_t *params)
{
	char *groupName;
	pContext->LocalToString(params[1], &groupName);

	return g_ModeGroupExtension.SwitchModeGroup(groupName) ? 1 : 0;
}

cell_t Native_GetCurrentModeGroup(IPluginContext *pContext, const cell_t *params)
{
	char *buffer;
	pContext->LocalToString(params[1], &buffer);
	ke::SafeStrcpy(buffer, params[2], g_ModeGroupExtension.GetCurrentModeGroupName());
	return 1;
}

cell_t Native_ReloadConfig(IPluginContext *pContext, const cell_t *params)
{
	g_ModeGroupExtension.ReloadConfig();
	return 1;
}

sp_nativeinfo_t g_Natives[] = 
{
	{"ModeGroup_Switch",			Native_SwitchModeGroup},
	{"ModeGroup_GetCurrent",		Native_GetCurrentModeGroup},
	{"ModeGroup_ReloadConfig",		Native_ReloadConfig},
	{NULL,							NULL}
};
