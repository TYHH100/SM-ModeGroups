#ifndef _INCLUDE_MODEGROUP_EXTENSION_H_
#define _INCLUDE_MODEGROUP_EXTENSION_H_

// 1. 首先包含 SourceMod 扩展 SDK 
#include "smsdk_ext.h"
#include <IPluginSys.h>
#include <IGameHelpers.h>
#include <vector>
#include <string>

/**
 * 2. 解决 CVAR_INTERFACE_VERSION 冲突
 * tier1/convar.h 会包含 icvar.h，导致宏重定义。
 * 我们在这里取消定义，让 HL2SDK 使用它自己的定义，或者让 SM 覆盖它。
 */
#if defined CVAR_INTERFACE_VERSION
#undef CVAR_INTERFACE_VERSION
#endif

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