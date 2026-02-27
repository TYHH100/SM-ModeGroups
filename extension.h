#ifndef _INCLUDE_MODEGROUP_EXTENSION_H_
#define _INCLUDE_MODEGROUP_EXTENSION_H_

#include "smsdk_ext.h"
#include <IPluginSys.h>
#include <IGameHelpers.h>
#include <vector>
#include <string>
#include <tier1/convar.h>

class ModeGroupExt : public SDKExtension
{
public:
    virtual bool SDK_OnLoad(char *error, size_t maxlength, bool late) override;
    virtual void SDK_OnUnload() override;

public:
    bool SwitchMode(const char* modeName);

private:
    void UnloadCurrentModePlugins();
    void ScanAndLoadPlugins(const std::string& path);

private:
    std::vector<IPlugin*> m_LoadedPlugins;
    IPluginManager *m_pPluginSys = nullptr;
    ITextParsers *m_pTextParsers = nullptr;
    IGameHelpers *m_pGameHelpers = nullptr;
};

extern ModeGroupExt g_ModeGroupExt;

#endif // _INCLUDE_MODEGROUP_EXTENSION_H_