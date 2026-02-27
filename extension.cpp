#include "extension.h"
#include "filesystem.h"

ModeManager g_ModeManager;
SMEXT_LINK(&g_ModeManager);

void ModeManager::SwitchMode(const char *modeName) {

    UnloadActivePlugins();

    KeyValues *pKV = new KeyValues("modegroups");
    char path[PLATFORM_MAX_PATH];
    g_pSM->BuildPath(Path_SM, path, sizeof(path), "configs/modegroup.cfg");

    if (!pKV->LoadFromFile(filesystem, path, "GAME")) {
        delete pKV;
        return;
    }


    KeyValues *pMode = pKV->FindKey(modeName);
    if (pMode) {

        KeyValues *pSettings = pMode->FindKey("cvars&cmds");
        if (pSettings) {
            for (KeyValues *pCur = pSettings->GetFirstValue(); pCur; pCur = pCur->GetNextKey()) {
                ConVar *pConVar = icvar->FindVar(pCur->GetName());
                if (pConVar) {
                    pConVar->SetValue(pCur->GetString());
                }
            }
        }

        const char *dir = pMode->GetString("plugin_directory", "");
        if (dir[0] != '\0') {
            LoadPluginsRecursive("addons/sourcemod/plugins", dir);
        }
    }

    delete pKV;
}

void ModeManager::LoadPluginsRecursive(const char *basePath, const char *relPath) {
    char searchPath[PLATFORM_MAX_PATH];
    char currentFullDir[PLATFORM_MAX_PATH];
    
    g_LibSys->PathFormat(currentFullDir, sizeof(currentFullDir), "%s/%s", basePath, relPath);
    g_LibSys->PathFormat(searchPath, sizeof(searchPath), "%s/*", currentFullDir);

    FileFindHandle_t hFind;
    const char *fileName = filesystem->FindFirst(searchPath, &hFind);

    while (fileName) {
        if (strcmp(fileName, ".") != 0 && strcmp(fileName, "..") != 0) {
            char nextRelPath[PLATFORM_MAX_PATH];
            g_LibSys->PathFormat(nextRelPath, sizeof(nextRelPath), "%s/%s", relPath, fileName);

            if (filesystem->FindIsDirectory(hFind)) {

                LoadPluginsRecursive(basePath, nextRelPath);
            } else {

                size_t len = strlen(fileName);
                if (len > 4 && strcmp(fileName + len - 4, ".smx") == 0) {
                    char error[256];
                    bool already;

                    IPlugin *pPlugin = plugins->LoadPlugin(nextRelPath, false, PluginType_MapUpdated, error, sizeof(error), &already);
                    if (pPlugin) {
                        m_ActivePlugins.push_back(pPlugin);
                    }
                }
            }
        }
        fileName = filesystem->FindNext(hFind);
    }
    filesystem->FindClose(hFind);
}

void ModeManager::UnloadActivePlugins() {
    for (auto it = m_ActivePlugins.begin(); it != m_ActivePlugins.end(); ++it) {
        if (*it && (*it)->GetStatus() <= PluginStatus_Paused) {
            plugins->UnloadPlugin(*it);
        }
    }
    m_ActivePlugins.clear();
}