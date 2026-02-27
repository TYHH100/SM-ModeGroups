#ifndef _MODEGROUP_EXTENSION_H_
#define _MODEGROUP_EXTENSION_H_

#include "smsdk_config.h"
#include <IShareSys.h>
#include <IPluginSys.h>
#include <IForwardSys.h>
#include <IHandleSys.h>
#include <icvar.h>
#include <KeyValues.h>
#include <vector>
#include <string>

using namespace SourceMod;

/**
 * @brief Mode information structure
 */
struct ModeInfo
{
	std::string name;
	std::vector<std::string> pluginFiles;      // Relative paths from plugins folder
	std::map<std::string, std::string> cvars;
	std::vector<std::string> commands;
};

/**
 * @brief ModeGroup extension class
 */
class ModeGroupExtension : public SDKExtension, public IPluginsListener
{
public:
	ModeGroupExtension();
	virtual ~ModeGroupExtension();

	// SDKExtension overrides
	virtual bool SDK_OnLoad(char *error, size_t maxlen, bool late);
	virtual void SDK_OnAllLoaded();
	virtual void SDK_OnUnload();
	virtual void SDK_OnPauseChange(bool paused);

	// IPluginsListener overrides
	virtual void OnPluginLoaded(IPlugin *plugin);
	virtual void OnPluginUnloaded(IPlugin *plugin);

	// Public methods
	bool LoadConfig();
	bool SwitchToMode(const char *modeName);
	const char *GetCurrentMode() const { return m_CurrentMode.c_str(); }
	const std::vector<ModeInfo> &GetModes() const { return m_Modes; }

private:
	void CollectPluginsFromDir(const char *relativeDir, std::vector<std::string> &outFiles, bool recursive);
	void UnloadCurrentModePlugins();
	void LoadModePlugins(const std::vector<std::string> &files);
	void ApplyModeSettings(const ModeInfo &mode);
	void ClearPluginList();

	std::vector<ModeInfo> m_Modes;
	std::string m_CurrentMode;
	std::vector<IPlugin *> m_LoadedPlugins;   // Plugins loaded by current mode
	bool m_IsSwitching;                         // Flag to distinguish our own loads
};

extern ModeGroupExtension g_ModeGroup;

#endif // _MODEGROUP_EXTENSION_H_