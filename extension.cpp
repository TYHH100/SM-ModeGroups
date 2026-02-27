#include "extension.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

ModeGroupExt g_ModeGroupExt;
SMEXT_LINK(&g_ModeGroupExt);

// --- 安全的状态机解析器 ---
enum class ParserState { None, InMain, InTargetMode, InCvars, InCommands };

class ModeParser : public ITextListener_SMC {
public:
    std::string target;
    std::string pluginPath;
    std::vector<std::pair<std::string, std::string>> cvars;
    std::vector<std::string> commands;
    bool found = false;
    ParserState state = ParserState::None;

    ModeParser(const char* mode) : target(mode) {}

    SMCResult ReadSMC_NewSection(const SMCStates *states, const char *name) override {
        if (state == ParserState::None && strcmp(name, "ModeGroups") == 0) {
            state = ParserState::InMain;
        } else if (state == ParserState::InMain && strcmp(name, target.c_str()) == 0) {
            state = ParserState::InTargetMode;
            found = true;
        } else if (state == ParserState::InTargetMode) {
            if (strcmp(name, "cvars") == 0) state = ParserState::InCvars;
            else if (strcmp(name, "commands") == 0) state = ParserState::InCommands;
        }
        return SMCResult_Continue;
    }

    SMCResult ReadSMC_KeyValue(const SMCStates *states, const char *key, const char *value) override {
        if (state == ParserState::InTargetMode && strcmp(key, "plugin_directory") == 0) {
            pluginPath = value;
        } else if (state == ParserState::InCvars) {
            cvars.push_back({key, value});
        } else if (state == ParserState::InCommands) {
            commands.push_back(key);
        }
        return SMCResult_Continue;
    }

    SMCResult ReadSMC_LeavingSection(const SMCStates *states) override {
        if (state == ParserState::InCvars || state == ParserState::InCommands) state = ParserState::InTargetMode;
        else if (state == ParserState::InTargetMode) state = ParserState::InMain;
        else if (state == ParserState::InMain) state = ParserState::None;
        return SMCResult_Continue;
    }
};

// --- 指令回调 ---
void Command_SwitchMode(const CCommand &command) {
    if (command.ArgC() < 2) {
        g_pSM->LogMessage(myself, "用法: sm_mode <模式名称>");
        return;
    }
    g_ModeGroupExt.SwitchMode(command.Arg(1));
}

ConCommand sm_mode("sm_mode", Command_SwitchMode, "切换插件分组模式", FCVAR_NONE);

bool ModeGroupExt::SDK_OnLoad(char *error, size_t maxlength, bool late) {
    // 关键修复：确保接口在注册指令前已连接
    if (!g_pCVar) {
        snprintf(error, maxlength, "无法连接到 ICvar 接口，请确保 Metamod 已运行");
        return false;
    }

    SM_GET_IFACE(PLUGINSYSTEM, m_pPluginSys);
    SM_GET_IFACE(TEXTPARSERS, m_pTextParsers);
    SM_GET_IFACE(GAMEHELPERS, m_pGameHelpers);

    if (!m_pPluginSys || !m_pTextParsers) {
        snprintf(error, maxlength, "必要的 SourceMod 接口初始化失败");
        return false;
    }

    g_pCVar->RegisterConCommand(&sm_mode);
    return true;
}

void ModeGroupExt::SDK_OnUnload() {
    UnloadCurrentModePlugins();
    if (g_pCVar) g_pCVar->UnregisterConCommand(&sm_mode);
}

bool ModeGroupExt::SwitchMode(const char* modeName) {
    char configPath[PLATFORM_MAX_PATH];
    g_pSM->BuildPath(Path_SM, configPath, sizeof(configPath), "configs/modegroup.cfg");

    ModeParser parser(modeName);
    SMCError err = m_pTextParsers->ParseSMCFile(configPath, &parser, nullptr, nullptr, 0);

    if (err != SMCError_Okay || !parser.found) {
        g_pSM->LogError(myself, "模式切换失败: 配置文件中未找到模式 [%s]", modeName);
        return false;
    }

    // 1. 自动卸载上个模式加载的所有插件
    UnloadCurrentModePlugins();

    // 2. 递归扫描并加载新插件
    if (!parser.pluginPath.empty()) {
        char fullPath[PLATFORM_MAX_PATH];
        g_pSM->BuildPath(Path_SM, fullPath, sizeof(fullPath), "plugins/%s", parser.pluginPath.c_str());
        ScanAndLoadPlugins(fullPath);
    }

    // 3. 应用 Cvars
    for (const auto& cv : parser.cvars) {
        ConVar* pVar = g_pCVar->FindVar(cv.first.c_str());
        if (pVar) pVar->SetValue(cv.second.c_str());
    }

    // 4. 执行 Commands
    for (const auto& cmd : parser.commands) {
        if (m_pGameHelpers) m_pGameHelpers->ServerCommand(cmd.c_str());
    }

    g_pSM->LogMessage(myself, "已切换至模式 [%s]，加载了 %d 个插件", modeName, (int)m_LoadedPlugins.size());
    return true;
}

void ModeGroupExt::ScanAndLoadPlugins(const std::string& path) {
    if (!fs::exists(path)) return;

    auto loadFile = [this](const std::string& filePath) {
        if (fs::path(filePath).extension() == ".smx") {
            char loadErr[256];
            bool wasLoaded = false;
            // 使用 PluginType_MapUpdated 模拟自动加载行为
            IPlugin* p = m_pPluginSys->LoadPlugin(filePath.c_str(), false, PluginType_MapUpdated, loadErr, sizeof(loadErr), &wasLoaded);
            if (p && !wasLoaded) {
                m_LoadedPlugins.push_back(p);
            }
        }
    };

    if (fs::is_regular_file(path)) {
        loadFile(path);
    } else if (fs::is_directory(path)) {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                loadFile(entry.path().string());
            }
        }
    }
}

void ModeGroupExt::UnloadCurrentModePlugins() {
    for (IPlugin* p : m_LoadedPlugins) {
        if (p && p->GetStatus() <= Plugin_Paused) {
            m_pPluginSys->UnloadPlugin(p);
        }
    }
    m_LoadedPlugins.clear();
}