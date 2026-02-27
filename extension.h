#ifndef _INCLUDE_MODEGROUP_EXTENSION_H_
#define _INCLUDE_MODEGROUP_EXTENSION_H_

#include "smsdk_ext.h"
#include <vector>
#include <string>

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
    
    // 递归加载目录或单文件 
    void LoadPluginsRecursively(const std::string& path);
    
    // 执行配置中的 Cvars 和 Commands 
    void ExecuteCommandsAndCvars(const std::vector<std::pair<std::string, std::string>>& cvars, 
                                 const std::vector<std::string>& commands);

private:
    std::vector<IPlugin*> m_LoadedPlugins;
    std::string m_CurrentMode = "none";
};

extern ModeGroupExt g_ModeGroupExt;

#endif // _INCLUDE_MODEGROUP_EXTENSION_H_