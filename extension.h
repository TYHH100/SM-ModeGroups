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
#include <icvar.h>

#if defined _DEBUG
#undef _DEBUG
#endif
#include <tier1/convar.h>

class ModeGroupExt : public SDKExtension
{
public:
    virtual bool SDK_OnLoad(char *error, size_t maxlength, bool late);
    virtual void SDK_OnUnload();

public:
    // 切换模式的核心函数
    bool SwitchMode(const char* modeName);

private:
    // 卸载当前组的插件 
    void UnloadCurrentModePlugins();

private:
    std::vector<IPlugin*> m_LoadedPlugins;
    std::string m_CurrentMode = "none";
    IPluginManager *m_pPluginSys = nullptr;
    ITextParsers *m_pTextParsers = nullptr;
    IGameHelpers *m_pGameHelpers = nullptr;
};

extern ModeGroupExt g_ModeGroupExt;

#endif // _INCLUDE_MODEGROUP_EXTENSION_H_