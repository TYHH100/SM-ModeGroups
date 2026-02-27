#include "extension.h"
#include <filesystem>
#include <eiface.h>
#include <icvar.h>

namespace fs = std::filesystem;

ModeGroupExt g_ModeGroupExt;
SMEXT_LINK(&g_ModeGroupExt);

// --- SMC 配置文件解析器 ---
// 用于解析 modegroup.cfg 
class ModeConfigParser : public ITextListener_SMC
{
public:
    std::string targetMode;
    std::string pluginDir;
    std::vector<std::pair<std::string, std::string>> cvars;
    std::vector<std::string> commands;

    int state = 0; // 0: Root, 1: ModeGroups, 2: TargetMode, 3: cvars, 4: commands
    bool foundMode = false;

    ModeConfigParser(const char* mode) : targetMode(mode) {}

    virtual void ReadSMC_ParseStart() {}
    
    virtual SMCResult ReadSMC_NewSection(const SMCStates *states, const char *name) {
        if (state == 0 && strcmp(name, "ModeGroups") == 0) {
            state = 1; // 进入 ModeGroups 
        } else if (state == 1 && strcmp(name, targetMode.c_str()) == 0) {
            state = 2; // 找到目标模式 (如 "none") 
            foundMode = true;
        } else if (state == 2 && strcmp(name, "cvars") == 0) {
            state = 3; // 进入 cvars 
        } else if (state == 2 && strcmp(name, "commands") == 0) {
            state = 4; // 进入 commands 
        }
        return SMCResult_Continue;
    }

    virtual SMCResult ReadSMC_KeyValue(const SMCStates *states, const char *key, const char *value) {
        if (state == 2 && strcmp(key, "plugin_directory") == 0) {
            pluginDir = value; // 获取路径 
        } else if (state == 3) {
            cvars.push_back({key, value});
        } else if (state == 4) {
            commands.push_back(key); // 命令本体在 key 中 
        }
        return SMCResult_Continue;
    }

    virtual SMCResult ReadSMC_LeavingSection(const SMCStates *states) {
        if (state == 4 || state == 3) state = 2;
        else if (state == 2) state = 1;
        else if (state == 1) state = 0;
        return SMCResult_Continue;
    }
};

// --- 控制台命令回调 ---
void Command_SwitchMode(const CCommand &command)
{
    if (command.ArgC() < 2) {
        META_CONPRINTF("[ModeGroup] Usage: sm_mode <mode_name>\n");
        return;
    }
    
    const char* modeName = command.Arg(1);
    g_ModeGroupExt.SwitchMode(modeName);
}

ConCommand sm_mode("sm_mode", Command_SwitchMode, "Switch plugin mode group", FCVAR_NONE);

// --- 扩展生命周期 ---
bool ModeGroupExt::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
    g_pCVar->RegisterConCommand(&sm_mode);
    return true;
}

void ModeGroupExt::SDK_OnUnload()
{
    UnloadCurrentModePlugins();
    g_pCVar->UnregisterConCommand(&sm_mode);
}

// --- 核心逻辑 ---
bool ModeGroupExt::SwitchMode(const char* modeName)
{
    char configPath[PLATFORM_MAX_PATH];
    g_pSM->BuildPath(Path_SM, configPath, sizeof(configPath), "configs/modegroup.cfg");

    ModeConfigParser parser(modeName);
    SMCError err = textparsers->ParseSMCFile(configPath, &parser);

    if (err != SMCError_Okay || !parser.foundMode) {
        smutils->LogError(myself, "Failed to load mode '%s' or parse error in modegroup.cfg", modeName);
        return false;
    }

    META_CONPRINTF("[ModeGroup] Switching mode to: %s\n", modeName);

    // 1. 自动卸载上个模式的所有插件
    UnloadCurrentModePlugins();

    // 2. 递归加载新模式指定的目录或文件 
    if (!parser.pluginDir.empty()) {
        char pluginPath[PLATFORM_MAX_PATH];
        g_pSM->BuildPath(Path_SM, pluginPath, sizeof(pluginPath), "plugins/%s", parser.pluginDir.c_str());
        LoadPluginsRecursively(pluginPath);
    }

    // 3. 执行 Cvars 和 Commands 
    ExecuteCommandsAndCvars(parser.cvars, parser.commands);

    m_CurrentMode = modeName;
    return true;
}

void ModeGroupExt::UnloadCurrentModePlugins()
{
    for (IPlugin* plugin : m_LoadedPlugins) {
        // 只有当插件还在加载状态时才卸载
        if (plugin && plugin->GetStatus() == Plugin_Running) {
            pluginsys->UnloadPlugin(plugin);
        }
    }
    m_LoadedPlugins.clear();
}

void ModeGroupExt::LoadPluginsRecursively(const std::string& path)
{
    if (!fs::exists(path)) {
        smutils->LogError(myself, "Path does not exist: %s", path.c_str());
        return;
    }

    std::vector<std::string> targetFiles;

    // 支持直接指定单文件，或指定文件夹名递归所有文件
    if (fs::is_regular_file(path) && fs::path(path).extension() == ".smx") {
        targetFiles.push_back(path);
    } else if (fs::is_directory(path)) {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".smx") {
                targetFiles.push_back(entry.path().string());
            }
        }
    }

    // 调用 IPluginManager 加载插件
    for (const auto& file : targetFiles) {
        char error[256];
        bool wasloaded = false;
        
        // 使用绝对路径加载
        IPlugin* plugin = pluginsys->LoadPlugin(file.c_str(), false, nullptr, error, sizeof(error), &wasloaded);
        
        if (plugin) {
            // 如果插件是之前手动或其他方式加载的，我们不纳入 ModeGroup 的自动卸载追踪
            if (!wasloaded) {
                m_LoadedPlugins.push_back(plugin);
                META_CONPRINTF("[ModeGroup] Loaded: %s\n", file.c_str());
            }
        } else {
            smutils->LogError(myself, "Failed to load %s: %s", file.c_str(), error);
        }
    }
}

void ModeGroupExt::ExecuteCommandsAndCvars(const std::vector<std::pair<std::string, std::string>>& cvars, 
                                           const std::vector<std::string>& commands)
{
    // 设置 Cvars 
    for (const auto& cvar : cvars) {
        ConVar* pCvar = g_pCVar->FindVar(cvar.first.c_str());
        if (pCvar) {
            pCvar->SetValue(cvar.second.c_str());
        }
    }

    // 执行 Commands 
    for (const auto& cmd : commands) {
        engine->ServerCommand(cmd.c_str());
        engine->ServerCommand("\n"); // 换行确保命令被推入
    }
    engine->ServerExecute();
}