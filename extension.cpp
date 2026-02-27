#include "extension.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

ModeGroupExt g_ModeGroupExt;
SMEXT_LINK(&g_ModeGroupExt);

// --- 状态机解析器保持不变 ---
enum class ParserState { None, InMain, InTargetMode, InCvars, InCommands };

class ModeParser : public ITextListener_SMC {
public:
    std::string target;
    std::string pluginDir;
    std::vector<std::pair<std::string, std::string>> cvars;
    std::vector<std::string> commands;
    bool found = false;
    ParserState state = ParserState::None;

    ModeParser(const char* mode) : target(mode) {}

    SMCResult ReadSMC_NewSection(const SMCStates *states, const char *name) override {
        if (state == ParserState::None && strcmp(name, "ModeGroups") == 0) state = ParserState::InMain;
        else if (state == ParserState::InMain && strcmp(name, target.c_str()) == 0) { state = ParserState::InTargetMode; found = true; }
        else if (state == ParserState::InTargetMode) {
            if (strcmp(name, "cvars") == 0) state = ParserState::InCvars;
            else if (strcmp(name, "commands") == 0) state = ParserState::InCommands;
        }
        return SMCResult_Continue;
    }

    SMCResult ReadSMC_KeyValue(const SMCStates *states, const char *key, const char *value) override {
        if (state == ParserState::InTargetMode && strcmp(key, "plugin_directory") == 0) pluginDir = value;
        else if (state == ParserState::InCvars) cvars.push_back({key, value});
        else if (state == ParserState::InCommands) commands.push_back(key);
        return SMCResult_Continue;
    }

    SMCResult ReadSMC_LeavingSection(const SMCStates *states) override {
        if (state == ParserState::InCvars || state == ParserState::InCommands) state = ParserState::InTargetMode;
        else if (state == ParserState::InTargetMode) state = ParserState::InMain;
        else if (state == ParserState::InMain) state = ParserState::None;
        return SMCResult_Continue;
    }
};

void Command_SwitchMode(const CCommand &command) {
    if (command.ArgC() < 2) {
        g_pSM->LogMessage(myself, "Usage: sm_mode <mode_name>");
        return;
    }
    g_ModeGroupExt.SwitchMode(command.Arg(1));
}

ConCommand sm_mode("sm_mode", Command_SwitchMode, "Switch mode group", FCVAR_NONE);

bool ModeGroupExt::SDK_OnLoad(char *error, size_t maxlength, bool late) {
    if (!g_pCVar) {
        snprintf(error, maxlength, "Could not find ICvar interface");
        return false;
    }

    SM_GET_IFACE(PLUGINSYSTEM, m_pPluginSys);
    SM_GET_IFACE(TEXTPARSERS, m_pTextParsers);
    SM_GET_IFACE(GAMEHELPERS, m_pGameHelpers);

    if (!m_pPluginSys || !m_pTextParsers) {
        snprintf(error, maxlength, "Required interfaces missing");
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
        g_pSM->LogError(myself, "Failed to load mode '%s'", modeName);
        return false;
    }

    UnloadCurrentModePlugins();

    if (!parser.pluginDir.empty()) {
        char fullPath[PLATFORM_MAX_PATH];
        g_pSM->BuildPath(Path_SM, fullPath, sizeof(fullPath), "plugins/%s", parser.pluginDir.c_str());
        ScanAndLoadPlugins(fullPath);
    }

    for (const auto& cv : parser.cvars) {
        ConVar* pCvar = g_pCVar->FindVar(cv.first.c_str());
        if (pCvar) pCvar->SetValue(cv.second.c_str());
    }

    for (const auto& cmd : parser.commands) {
        if (m_pGameHelpers) m_pGameHelpers->ServerCommand(cmd.c_str());
    }

    return true;
}

void ModeGroupExt::ScanAndLoadPlugins(const std::string& path) {
    if (!fs::exists(path)) return;

    // 如果指定的是单文件
    if (fs::is_regular_file(path) && fs::path(path).extension() == ".smx") {
        char loadErr[256];
        bool wasloaded = false;
        IPlugin* p = m_pPluginSys->LoadPlugin(path.c_str(), false, PluginType_MapUpdated, loadErr, sizeof(loadErr), &wasloaded);
        if (p && !wasloaded) m_LoadedPlugins.push_back(p);
    } 
    // 如果指定的是目录，则递归扫描 
    else if (fs::is_directory(path)) {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".smx") {
                char loadErr[256];
                bool wasloaded = false;
                IPlugin* p = m_pPluginSys->LoadPlugin(entry.path().string().c_str(), false, PluginType_MapUpdated, loadErr, sizeof(loadErr), &wasloaded);
                if (p && !wasloaded) m_LoadedPlugins.push_back(p);
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