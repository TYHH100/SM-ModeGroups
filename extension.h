#ifndef _MODEGROUP_EXTENSION_H_
#define _MODEGROUP_EXTENSION_H_

#include "smsdk_config.h"
#include <extension.h>
#include <filesystem.h>
#include <convar.h>
#include <IPluginSys.h>
#include <ILibrarySys.h>
#include <IPlayerHelpers.h>
#include <ISourceMod.h>
#include <ICvar.h>
#include <vector>
#include <string>
#include <map>



using namespace SourceMod;

struct ModeInfo
{
    std::string name;
    std::vector<std::string> pluginFiles;
    std::map<std::string, std::string> cvars;
    std::vector<std::string> commands;
};

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

private:
    void CollectPluginsFromDir(const char *relativeDir, std::vector<std::string> &outFiles, bool recursive);
    void UnloadCurrentModePlugins();
    void LoadModePlugins(const std::vector<std::string> &files);
    void ApplyModeSettings(const ModeInfo &mode);

    std::vector<ModeInfo> m_Modes;
    std::string m_CurrentMode;
    std::vector<IPlugin *> m_LoadedPlugins;
    bool m_IsSwitching;

    // 控制台命令处理
    static void OnModeCommand(const CCommand &command);
    ConCommand *m_pModeCommand;
};

extern ModeGroupExtension g_ModeGroup;

#endif // _MODEGROUP_EXTENSION_H_