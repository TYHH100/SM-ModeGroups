#ifndef _INCLUDE_MODEGROUP_EXTENSION_H_
#define _INCLUDE_MODEGROUP_EXTENSION_H_

#include "smsdk_ext.h"
#include <IPluginSys.h>
#include <IGameHelpers.h>
#include <vector>
#include <string>

#if defined CVAR_INTERFACE_VERSION
#undef CVAR_INTERFACE_VERSION
#endif

#include <tier1/convar.h>

struct ModeGroup
{
    std::vector<std::string> pluginsToLoad;
    std::vector<std::string> pluginsToUnload;
    std::vector<std::pair<std::string, std::string>> cvars;
    std::vector<std::pair<std::string, std::string>> commands;
};

class ModeGroupExt : public SDKExtension
{
public:
    virtual bool SDK_OnLoad(char *error, size_t maxlength, bool late) override;
    virtual void SDK_OnUnload() override;

public:
    bool SwitchMode(const char* modeName);
    void ReloadConfig();

private:
    void UnloadCurrentModePlugins();
    void ScanAndLoadPlugins(const std::string& path);
    bool LoadConfig();
    void ScanPluginsRecursive(const std::string& path, std::vector<std::string>& files);

private:
    std::vector<IPlugin*> m_CurrentModePlugins;
    std::map<std::string, ModeGroup> m_Modes;
    IPluginManager *m_pPluginSys = nullptr;
    ITextParsers *m_pTextParsers = nullptr;
    IGameHelpers *m_pGameHelpers = nullptr;
    ICvar *m_pCVar = nullptr;
};

extern ModeGroupExt g_ModeGroupExt;

#endif // _INCLUDE_MODEGROUP_EXTENSION_H_