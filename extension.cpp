#include "extension.h"
#include <filesystem>
#include <itextparsers.h>
#include <string>

// To fix the CVAR_INTERFACE_VERSION redefinition error:
#if defined CVAR_INTERFACE_VERSION
#undef CVAR_INTERFACE_VERSION
#endif
#include <eiface.h>
#include <icvar.h>

namespace fs = std::filesystem;

ModeGroupExt g_ModeGroupExt;
SMEXT_LINK(&g_ModeGroupExt);

// Define global interface pointers if they aren't automatically linked
IGameConfig *g_pGameConfig = nullptr;
IVEngineServer *engine = nullptr; 

// --- SMC 配置文件解析器 ---
class ModeConfigParser : public ITextListener_SMC
{
public:
    std::string targetMode;
    std::string pluginDir;
    std::vector<std::pair<std::string, std::string>> cvars;
    std::vector<std::string> commands;
    int state = 0; 
    bool foundMode = false;

    ModeConfigParser(const char* mode) : targetMode(mode) {}

    // These now return SMCResult correctly
    SMCResult ReadSMC_NewSection(const SMCStates *states, const char *name) override {
        if (state == 0 && strcmp(name, "ModeGroups") == 0) {
            state = 1;
        } else if (state == 1 && strcmp(name, targetMode.c_str()) == 0) {
            state = 2;
            foundMode = true;
        } else if (state == 2 && strcmp(name, "cvars") == 0) {
            state = 3;
        } else if (state == 2 && strcmp(name, "commands") == 0) {
            state = 4;
        }
        return SMCResult_Continue;
    }

    SMCResult ReadSMC_KeyValue(const SMCStates *states, const char *key, const char *value) override {
        if (state == 2 && strcmp(key, "plugin_directory") == 0) {
            pluginDir = value;
        } else if (state == 3) {
            cvars.push_back({key, value});
        } else if (state == 4) {
            commands.push_back(key);
        }
        return SMCResult_Continue;
    }

    SMCResult ReadSMC_LeavingSection(const SMCStates *states) override {
        if (state == 4 || state == 3) state = 2;
        else if (state == 2) state = 1;
        else if (state == 1) state = 0;
        return SMCResult_Continue;
    }
};

// --- Command Callback ---
void Command_SwitchMode(const CCommand &command)
{
    if (command.ArgC() < 2) {
        smutils->LogMessage(myself, "Usage: sm_mode <mode_name>");
        return;
    }
    g_ModeGroupExt.SwitchMode(command.Arg(1));
}

ConCommand sm_mode("sm_mode", Command_SwitchMode, "Switch plugin mode group", FCVAR_NONE);

// --- Extension Lifecycle ---
bool ModeGroupExt::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
    // Initialize Engine pointer
    engine = iserver->GetEngine();
    
    if (!g_pCVar) {
        libsys->GetInterfaceFactory(engine); // Ensure CVar is available
    }

    g_pCVar->RegisterConCommand(&sm_mode);
    return true;
}

void ModeGroupExt::SDK_OnUnload()
{
    UnloadCurrentModePlugins();
    g_pCVar->UnregisterConCommand(&sm_mode);
}

bool ModeGroupExt::SwitchMode(const char* modeName)
{
    char configPath[PLATFORM_MAX_PATH];
    g_pSM->BuildPath(Path_SM, configPath, sizeof(configPath), "configs/modegroup.cfg");

    ModeConfigParser parser(modeName);
    SMCError err = textparsers->ParseSMCFile(configPath, &parser);

    if (err != SMCError_Okay || !parser.foundMode) {
        smutils->LogError(myself, "Failed to load mode '%s' (Error code: %d)", modeName, err);
        return false;
    }

    UnloadCurrentModePlugins();

    if (!parser.pluginDir.empty()) {
        char pluginPath[PLATFORM_MAX_PATH];
        g_pSM->BuildPath(Path_SM, pluginPath, sizeof(pluginPath), "plugins/%s", parser.pluginDir.c_str());
        LoadPluginsRecursively(pluginPath);
    }

    ExecuteCommandsAndCvars(parser.cvars, parser.commands);
    m_CurrentMode = modeName;
    return true;
}

void ModeGroupExt::UnloadCurrentModePlugins()
{
    for (IPlugin* plugin : m_LoadedPlugins) {
        if (plugin && plugin->GetStatus() <= Plugin_Paused) {
            pluginsys->UnloadPlugin(plugin);
        }
    }
    m_LoadedPlugins.clear();
}

void ModeGroupExt::LoadPluginsRecursively(const std::string& path)
{
    if (!fs::exists(path)) return;

    if (fs::is_directory(path)) {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".smx") {
                char error[256];
                bool wasloaded = false;
                IPlugin* p = pluginsys->LoadPlugin(entry.path().string().c_str(), false, nullptr, error, sizeof(error), &wasloaded);
                if (p && !wasloaded) m_LoadedPlugins.push_back(p);
            }
        }
    }
}

void ModeGroupExt::ExecuteCommandsAndCvars(const std::vector<std::pair<std::string, std::string>>& cvars, 
                                           const std::vector<std::string>& commands)
{
    for (const auto& cvar : cvars) {
        ConVar* pCvar = g_pCVar->FindVar(cvar.first.c_str());
        if (pCvar) pCvar->SetValue(cvar.second.c_str());
    }

    for (const auto& cmd : commands) {
        engine->ServerCommand(cmd.c_str());
    }
    engine->ServerExecute();
}