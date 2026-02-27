/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Mode Groups Extension
 * Copyright (C) 2024.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"
#include <sh_string.h>
#include <ITextParsers.h>

ModeGroupExtension g_ModeGroupExtension;

SMEXT_LINK(&g_ModeGroupExtension);

sp_nativeinfo_t g_Natives[];
void Cmd_SwitchModeGroup(const CCommand &args);
void Cmd_ReloadConfig(const CCommand &args);
void Cmd_ListModeGroups(const CCommand &args);
void Cmd_CurrentModeGroup(const CCommand &args);

class ModeGroupConfigParser : public ITextListener_SMC
{
public:
	ModeGroupConfigParser(std::map<std::string, ModeGroup> &groups) 
		: m_Groups(groups), m_InModeGroups(false), m_InCvars(false), m_InCommands(false)
	{
	}

	void ReadSMC_ParseStart()
	{
		m_CurrentGroup.name.clear();
		m_CurrentGroup.plugin_directory.clear();
		m_CurrentGroup.plugin_files.clear();
		m_CurrentGroup.cvars.clear();
		m_CurrentGroup.commands.clear();
		m_InModeGroups = false;
		m_InCvars = false;
		m_InCommands = false;
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
		else if (strcmp(key, "plugin_directory") == 0)
		{
			m_CurrentGroup.plugin_directory = value;
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
		else if (!m_CurrentGroup.name.empty())
		{
			m_Groups[m_CurrentGroup.name] = m_CurrentGroup;
			m_CurrentGroup.name.clear();
			m_CurrentGroup.plugin_directory.clear();
			m_CurrentGroup.plugin_files.clear();
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
};

bool ModeGroupExtension::SDK_OnLoad(char *error, size_t maxlen, bool late)
{
	if (!LoadConfig(error, maxlen))
	{
		return false;
	}

	sharesys->AddNatives(myself, g_Natives);

	m_pModeGroupChangedForward = forwards->CreateForward("OnModeGroupChanged", ET_Ignore, 2, NULL, Param_String, Param_String);

	ConCmdBase *pCmd;
	pCmd = new ConCommand("modegroup_switch", Cmd_SwitchModeGroup, "Switch to a mode group", FCVAR_NONE);
	pCmd->AddCommand();
	pCmd = new ConCommand("modegroup_reload", Cmd_ReloadConfig, "Reload mode group configuration", FCVAR_NONE);
	pCmd->AddCommand();
	pCmd = new ConCommand("modegroup_list", Cmd_ListModeGroups, "List available mode groups", FCVAR_NONE);
	pCmd->AddCommand();
	pCmd = new ConCommand("modegroup_current", Cmd_CurrentModeGroup, "Show current mode group", FCVAR_NONE);
	pCmd->AddCommand();

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

	if (!textparsers->ParseFile_SMC(path, &parser, &states))
	{
		ke::SafeStrcpy(error, maxlen, "Failed to parse modegroup.cfg");
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

	for (std::map<std::string, std::string>::const_iterator it = group.cvars.begin();
		it != group.cvars.end(); ++it)
	{
		ConVar *pCvar = icvar->FindVar(it->first.c_str());
		if (pCvar)
		{
			pCvar->SetValue(it->second.c_str());
			g_pSM->LogMessage(myself, "Set cvar %s to %s", it->first.c_str(), it->second.c_str());
		}
		else
		{
			g_pSM->LogError(myself, "Cvar %s not found", it->first.c_str());
		}
	}

	for (std::map<std::string, std::string>::const_iterator it = group.commands.begin();
		it != group.commands.end(); ++it)
	{
		char cmd[256];
		ke::SafeSprintf(cmd, sizeof(cmd), "%s %s", it->first.c_str(), it->second.c_str());
		engine->ServerCommand(cmd);
		g_pSM->LogMessage(myself, "Executed command: %s", cmd);
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

		dir->NextFile();
	}

	libsys->CloseDirectory(dir);
}

bool ModeGroupExtension::LoadPlugin(const char *path)
{
	char fullPath[PLATFORM_MAX_PATH];
	g_pSM->BuildPath(Path_SM, fullPath, sizeof(fullPath), "plugins/%s", path);

	IPlugin *pPlugin = pluginsys->FindPluginByFile(fullPath);
	if (pPlugin)
	{
		if (pPlugin->GetStatus() == Plugin_Running)
		{
			g_pSM->LogMessage(myself, "Plugin already loaded: %s", path);
			return true;
		}
	}

	char error[256];
	PluginId id;
	if (!pluginsys->LoadPlugin(fullPath, NULL, error, sizeof(error), &id))
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

	IPlugin *pPlugin = pluginsys->FindPluginByFile(fullPath);
	if (!pPlugin)
	{
		return;
	}

	if (pPlugin->GetStatus() != Plugin_Running)
	{
		return;
	}

	PluginId id = pPlugin->GetId();
	char error[256];
	if (!pluginsys->UnloadPlugin(id, error, sizeof(error)))
	{
		g_pSM->LogError(myself, "Failed to unload plugin %s: %s", path, error);
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
	g_pSM->LogMessage(myself, "Available mode groups:");
	for (std::map<std::string, ModeGroup>::iterator it = m_ModeGroups.begin();
		it != m_ModeGroups.end(); ++it)
	{
		g_pSM->LogMessage(myself, "  - %s", it->first.c_str());
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
	ke::SafeStrcpy(buffer, params[2], g_ModeGroupExtension.m_CurrentModeGroup.c_str());
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

void Cmd_SwitchModeGroup(const CCommand &args)
{
	if (args.ArgC() < 2)
	{
		Msg("Usage: modegroup_switch <groupname>\n");
		return;
	}

	g_ModeGroupExtension.SwitchModeGroup(args.Arg(1));
}

void Cmd_ReloadConfig(const CCommand &args)
{
	g_ModeGroupExtension.ReloadConfig();
}

void Cmd_ListModeGroups(const CCommand &args)
{
	g_ModeGroupExtension.ListModeGroups();
}

void Cmd_CurrentModeGroup(const CCommand &args)
{
	if (g_ModeGroupExtension.m_CurrentModeGroup.empty())
	{
		Msg("No mode group currently active\n");
	}
	else
	{
		Msg("Current mode group: %s\n", g_ModeGroupExtension.m_CurrentModeGroup.c_str());
	}
}
