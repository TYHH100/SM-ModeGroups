#include "extension.h"
#include <filesystem>
#include <string>
#include <vector>

#if defined CVAR_INTERFACE_VERSION
#undef CVAR_INTERFACE_VERSION
#endif
#include <icvar.h>

namespace fs = std::filesystem;

ModeGroupExt g_ModeGroupExt;
SMEXT_LINK(&g_ModeGroupExt);

// --- SMC 解析监听器 ---
class ModeParser : public ITextListener_SMC {
public:
    std::string targetMode;
    std::string pluginDir;
    std::vector<std::pair<std::string, std::string>> cvars;
    std::vector<std::string> commands;
    int state = 0; 
    bool found = false;
    std::string currentSection;

    ModeParser(const char* mode) : targetMode(mode) {}

    // 使用 SMCResult 替代手动包含头文件，SM 会自动识别这些枚举
    SMCResult ReadSMC_NewSection(const SMCStates *states, const char *name) override {
        if (state == 0 && strcmp(name, "ModeGroups") == 0) state = 1;
        else if (state == 1 && strcmp(name, targetMode.c_str()) == 0) { state = 2; found = true; }
        else if (state == 2 && (strcmp(name, "cvars") == 0 || strcmp(name, "commands") == 0)) { 
            state = 3;
            currentSection = name;
        }
        return SMCResult_Continue;
    }

    SMCResult ReadSMC_KeyValue(const SMCStates *states, const char *key, const char *value) override {
        if (state == 2 && strcmp(key, "plugin_directory") == 0) pluginDir = value;
        else if (state == 3) {
            if (currentSection == "cvars") cvars.push_back({key, value});
            else if (currentSection == "commands") commands.push_back(key);
        }
        return SMCResult_Continue;
    }

    SMCResult ReadSMC_LeavingSection(const SMCStates *states) override {
        if (state > 0) state--;
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
    // Check if the global ICvar interface provided by Metamod/SourceSDK is valid
    if (!g_pCVar) {
        snprintf(error, maxlength, "Could not find ICvar interface");
        return false;
    }

    SM_GET_IFACE(PLUGINSYSTEM, m_pPluginSys);
    SM_GET_IFACE(TEXTPARSERS, m_pTextParsers);
    SM_GET_IFACE(GAMEHELPERS, m_pGameHelpers);

    if (!m_pPluginSys || !m_pTextParsers) {
        snprintf(error, maxlength, "Required interfaces (PluginSys/TextParsers) missing");
        return false;
    }
    
    g_pCVar->RegisterConCommand(&sm_mode);
    return true;
}

void ModeGroupExt::SDK_OnUnload() {
    UnloadCurrentModePlugins();
    g_pCVar->UnregisterConCommand(&sm_mode);
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
        
        if (fs::exists(fullPath)) {
            for (const auto& entry : fs::recursive_directory_iterator(fullPath)) {
                if (entry.is_regular_file() && entry.path().extension() == ".smx") {
                    char loadErr[256];
                    bool wasloaded = false;
                    IPlugin* p = m_pPluginSys->LoadPlugin(entry.path().string().c_str(), false, PluginType_MapUpdated, loadErr, sizeof(loadErr), &wasloaded);
                    if (p && !wasloaded) m_LoadedPlugins.push_back(p);
                }
            }
        }
    }

    for (const auto& cv : parser.cvars) {
        ConVar* pCvar = g_pCVar->FindVar(cv.first.c_str());
        if (pCvar) pCvar->SetValue(cv.second.c_str());
    }

    for (const auto& cmd : parser.commands) {
        if (m_pGameHelpers) {
            m_pGameHelpers->ServerCommand(cmd.c_str());
        }
    }

    return true;
}

void ModeGroupExt::UnloadCurrentModePlugins() {
    for (IPlugin* p : m_LoadedPlugins) {
        if (p && p->GetStatus() <= Plugin_Paused) m_pPluginSys->UnloadPlugin(p);
    }
    m_LoadedPlugins.clear();
}