#include "extension.h"
#include <filesystem>
#include <algorithm>
#include <map>

namespace fs = std::filesystem;

ModeGroupExt g_ModeGroupExt;
SMEXT_LINK(&g_ModeGroupExt);

// --- 指令回调 ---
void Command_SwitchMode(const CCommand &command) {
    if (command.ArgC() < 2) {
        g_pSM->LogMessage(myself, "用法: sm_mode <模式名称>");
        return;
    }
    g_ModeGroupExt.SwitchMode(command.Arg(1));
}

void Command_ReloadConfig(const CCommand &command) {
    g_ModeGroupExt.ReloadConfig();
    g_pSM->LogMessage(myself, "ModeGroups 配置文件已重载");
}

ConCommand sm_mode("sm_mode", Command_SwitchMode, "切换插件分组模式", FCVAR_NONE);
ConCommand sm_mode_reload("sm_mode_reload", Command_ReloadConfig, "重新加载 ModeGroups 配置文件", FCVAR_NONE);

bool ModeGroupExt::SDK_OnLoad(char *error, size_t maxlength, bool late) {
    SM_GET_IFACE(PLUGINSYSTEM, m_pPluginSys);
    SM_GET_IFACE(TEXTPARSERS, m_pTextParsers);
    SM_GET_IFACE(GAMEHELPERS, m_pGameHelpers);
    
    // 我们需要更通用的 ICvar 接口
    m_pCVar = (ICvar *)g_pSM->GetEngineFactory()("VEngineCvar007", nullptr);
    if (!m_pCVar) {
        m_pCVar = (ICvar *)g_pSM->GetEngineFactory()("VEngineCvar004", nullptr);
    }

    if (!m_pPluginSys || !m_pTextParsers || !m_pCVar) {
        snprintf(error, maxlength, "必要的 SourceMod 接口初始化失败");
        return false;
    }

    LoadConfig();

    m_pCVar->RegisterConCommand(&sm_mode);
    m_pCVar->RegisterConCommand(&sm_mode_reload);
    return true;
}

void ModeGroupExt::SDK_OnUnload() {
    UnloadCurrentModePlugins();
    if (m_pCVar) {
        m_pCVar->UnregisterConCommand(&sm_mode);
        m_pCVar->UnregisterConCommand(&sm_mode_reload);
    }
}

bool ModeGroupExt::LoadConfig() {
    char configPath[PLATFORM_MAX_PATH];
    g_pSM->BuildPath(Path_SM, configPath, sizeof(configPath), "configs/modegroup.cfg");

    KeyValues *kv = new KeyValues("ModeGroups");
    if (!kv->LoadFromFile(g_pFullFileSystem, configPath)) {
        delete kv;
        g_pSM->LogError(myself, "无法加载配置文件: %s", configPath);
        return false;
    }

    m_Modes.clear();

    KeyValues *pMode = kv->GetFirstTrueSubKey();
    while (pMode) {
        const char *modeName = pMode->GetName();
        ModeGroup node;

        // 解析 load 部分
        KeyValues *pLoad = pMode->FindKey("load");
        if (pLoad) {
            KeyValues *pValue = pLoad->GetFirstValue();
            while (pValue) {
                node.pluginsToLoad.push_back(pValue->GetString());
                pValue = pValue->GetNextValue();
            }
        }

        // 解析 unload 部分
        KeyValues *pUnload = pMode->FindKey("unload");
        if (pUnload) {
            KeyValues *pValue = pUnload->GetFirstValue();
            while (pValue) {
                node.pluginsToUnload.push_back(pValue->GetString());
                pValue = pValue->GetNextValue();
            }
        }

        // 解析 cvars 部分
        KeyValues *pCvars = pMode->FindKey("cvars");
        if (pCvars) {
            KeyValues *pValue = pCvars->GetFirstValue();
            while (pValue) {
                node.cvars.push_back({pValue->GetName(), pValue->GetString()});
                pValue = pValue->GetNextValue();
            }
        }

        // 解析 commands 部分
        KeyValues *pCmds = pMode->FindKey("commands");
        if (pCmds) {
            KeyValues *pValue = pCmds->GetFirstValue();
            while (pValue) {
                node.commands.push_back({pValue->GetName(), pValue->GetString()});
                pValue = pValue->GetNextValue();
            }
        }

        m_Modes[modeName] = node;
        pMode = pMode->GetNextTrueSubKey();
    }

    delete kv;
    return true;
}

void ModeGroupExt::ReloadConfig() {
    LoadConfig();
}

bool ModeGroupExt::SwitchMode(const char* modeName) {
    auto it = m_Modes.find(modeName);
    if (it == m_Modes.end()) {
        g_pSM->LogError(myself, "模式切换失败: 未找到模式 [%s]", modeName);
        return false;
    }

    const ModeGroup& mode = it->second;

    // 1. 卸载该模式额外加载的插件
    UnloadCurrentModePlugins();

    // 2. 处理指定的卸载列表 (从全局插件列表中查找)
    for (const auto& target : mode.pluginsToUnload) {
        char fullPath[PLATFORM_MAX_PATH];
        g_pSM->BuildPath(Path_SM, fullPath, sizeof(fullPath), "plugins/%s", target.c_str());
        std::string targetPath = fullPath;

        IPluginIterator *pIter = m_pPluginSys->GetPluginIterator();
        while (pIter->MorePlugins()) {
            IPlugin *p = pIter->GetPlugin();
            if (p->GetStatus() <= Plugin_Paused) {
                std::string pluginFile = p->GetFilename();
                // 检查是否匹配文件或位于目录下
                if (pluginFile.find(targetPath) == 0) {
                    m_pPluginSys->UnloadPlugin(p);
                }
            }
            pIter->NextPlugin();
        }
        pIter->Release();
    }

    // 3. 处理加载列表
    for (const auto& target : mode.pluginsToLoad) {
        char fullPath[PLATFORM_MAX_PATH];
        g_pSM->BuildPath(Path_SM, fullPath, sizeof(fullPath), "plugins/%s", target.c_str());
        ScanAndLoadPlugins(fullPath);
    }

    // 4. 应用 Cvars
    for (const auto& cv : mode.cvars) {
        ConVar *pVar = m_pCVar->FindVar(cv.first.c_str());
        if (pVar) {
            pVar->SetValue(cv.second.c_str());
        }
    }

    // 5. 执行 Commands
    for (const auto& cmd : mode.commands) {
        if (m_pGameHelpers) {
            std::string fullCmd = cmd.first;
            if (!cmd.second.empty()) {
                fullCmd += " ";
                fullCmd += cmd.second;
            }
            m_pGameHelpers->ServerCommand(fullCmd.c_str());
        }
    }

    g_pSM->LogMessage(myself, "已切换至模式 [%s], 加载了 %d 个专属插件", modeName, (int)m_CurrentModePlugins.size());
    return true;
}

void ModeGroupExt::ScanAndLoadPlugins(const std::string& path) {
    std::vector<std::string> files;
    ScanPluginsRecursive(path, files);

    for (const auto& file : files) {
        char loadErr[256];
        bool wasLoaded = false;
        // 模式加载的插件标记为 PluginType_MapUpdated 以便在需要时管理
        IPlugin *p = m_pPluginSys->LoadPlugin(file.c_str(), false, PluginType_MapUpdated, loadErr, sizeof(loadErr), &wasLoaded);
        if (p && !wasLoaded) {
            m_CurrentModePlugins.push_back(p);
        }
    }
}

void ModeGroupExt::ScanPluginsRecursive(const std::string& path, std::vector<std::string>& files) {
    if (!fs::exists(path)) return;

    if (fs::is_regular_file(path)) {
        if (fs::path(path).extension() == ".smx") {
            files.push_back(path);
        }
    } else if (fs::is_directory(path)) {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".smx") {
                files.push_back(entry.path().string());
            }
        }
    }
}

void ModeGroupExt::UnloadCurrentModePlugins() {
    for (IPlugin* p : m_CurrentModePlugins) {
        if (p && p->GetStatus() <= Plugin_Paused) {
            m_pPluginSys->UnloadPlugin(p);
        }
    }
    m_CurrentModePlugins.clear();
}