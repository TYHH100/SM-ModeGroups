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

#ifndef _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_

/**
 * @file extension.h
 * @brief Mode Groups extension code header.
 */

#include "smsdk_ext.h"
#include <vector>
#include <string>
#include <map>

struct ModeGroup
{
	std::string name;
	std::string plugin_directory;
	std::vector<std::string> plugin_files;
	std::map<std::string, std::string> cvars;
	std::map<std::string, std::string> commands;
};

class ModeGroupExtension : public SDKExtension, public IRootConsoleCommand
{
public:
	virtual bool SDK_OnLoad(char *error, size_t maxlen, bool late);
	virtual void SDK_OnUnload();
	virtual void SDK_OnAllLoaded();
	virtual bool QueryRunning(char *error, size_t maxlen);

public:
	bool LoadConfig(char *error, size_t maxlen);
	bool SwitchModeGroup(const char *groupName);
	void UnloadCurrentModeGroup();
	void LoadModeGroup(const ModeGroup &group);
	void ScanDirectoryForPlugins(const char *path, std::vector<std::string> &plugins);
	bool LoadPlugin(const char *path);
	void UnloadPlugin(const char *path);
	void ReloadConfig();
	void ListModeGroups();
	const char *GetCurrentModeGroupName();
	void CurrentModeGroup();

public:
	void OnRootConsoleCommand(const char *cmdname, const ICommandArgs *args) override;

private:
	std::map<std::string, ModeGroup> m_ModeGroups;
	std::string m_CurrentModeGroup;
	std::vector<std::string> m_LoadedPlugins;
	IForward *m_pModeGroupChangedForward;
};

extern ModeGroupExtension g_ModeGroupExtension;

#endif // _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_