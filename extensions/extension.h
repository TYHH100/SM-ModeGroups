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
	std::vector<std::string> load_plugins;
	std::vector<std::string> unload_plugins;
	bool use_sm_cvar;
	std::map<std::string, std::string> cvars;
	std::map<std::string, std::string> commands;
};

class ModeGroupExtension : public SDKExtension, public IRootConsoleCommand
{
public:
	virtual bool SDK_OnLoad(char *error, size_t maxlen, bool late) override;
	virtual void SDK_OnUnload() override;
	virtual void SDK_OnAllLoaded() override;
	virtual bool QueryRunning(char *error, size_t maxlen) override;

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