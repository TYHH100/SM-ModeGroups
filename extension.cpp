#include "extension.h"

ModeGroupExtension g_ModeGroup;
SMEXT_LINK(g_ModeGroup);

ModeGroupExtension::ModeGroupExtension()
    : m_IsSwitching(false), m_pModeCommand(nullptr)
{
}

ModeGroupExtension::~ModeGroupExtension()
{
}

bool ModeGroupExtension::SDK_OnLoad(char *error, size_t maxlen, bool late)
{
    sharesys->AddInterface(myself, this);
    sharesys->RegisterLibrary(myself, "modegroup");

    if (!LoadConfig())
    {
        snprintf(error, maxlen, "Failed to load config file");
        return false;
    }

    IPlayerHelpers *playerhelpers = (IPlayerHelpers *)sharesys->RequestInterface(IPlayerHelpers_Name);
    if (playerhelpers)
    {
        playerhelpers->GetPluginManager()->AddPluginsListener(this);
    }

    m_pModeCommand = new ConCommand("sm_mode", OnModeCommand, "Switch game mode (see modegroup.cfg)", FCVAR_SPONLY | FCVAR_GAMEDLL);
    g_pCVar->RegisterConCommand(m_pModeCommand);

    return true;
}

void ModeGroupExtension::SDK_OnAllLoaded()
{
}

void ModeGroupExtension::SDK_OnUnload()
{
    UnloadCurrentModePlugins();

    IPlayerHelpers *playerhelpers = (IPlayerHelpers *)sharesys->RequestInterface(IPlayerHelpers_Name);
    if (playerhelpers)
    {
        playerhelpers->GetPluginManager()->RemovePluginsListener(this);
    }

    if (m_pModeCommand)
    {
        g_pCVar->UnregisterConCommand(m_pModeCommand);
        delete m_pModeCommand;
        m_pModeCommand = nullptr;
    }
}

void ModeGroupExtension::SDK_OnPauseChange(bool paused)
{
}

void ModeGroupExtension::OnPluginLoaded(IPlugin *plugin)
{
    if (m_IsSwitching)
    {
        m_LoadedPlugins.push_back(plugin);
    }
}

void ModeGroupExtension::OnPluginUnloaded(IPlugin *plugin)
{
    for (auto it = m_LoadedPlugins.begin(); it != m_LoadedPlugins.end(); ++it)
    {
        if (*it == plugin)
        {
            m_LoadedPlugins.erase(it);
            break;
        }
    }
}

bool ModeGroupExtension::LoadConfig()
{
    char path[PLATFORM_MAX_PATH];
    g_pSM->BuildPath(Path_SM, path, sizeof(path), "configs/modegroup.cfg");

    KeyValues *kv = new KeyValues("ModeGroups");
    if (!kv->LoadFromFile(g_pSM->GetFileSystem(), path))
    {
        kv->deleteThis();
        return false;
    }

    if (!kv->JumpToKey("ModeGroups"))
    {
        kv->deleteThis();
        return false;
    }

    for (KeyValues *sub = kv->GetFirstSubKey(); sub; sub = sub->GetNextKey())
    {
        ModeInfo info;
        info.name = sub->GetName();

        // plugin_directory
        const char *pluginDir = sub->GetString("plugin_directory", nullptr);
        if (pluginDir && pluginDir[0])
        {
            CollectPluginsFromDir(pluginDir, info.pluginFiles, true);
        }

        // plugins list
        if (sub->JumpToKey("plugins"))
        {
            for (KeyValues *p = sub->GetFirstSubKey(); p; p = p->GetNextKey())
            {
                const char *fileName = p->GetString();
                if (fileName && fileName[0])
                {
                    info.pluginFiles.push_back(fileName);
                }
            }
            sub->GoBack();
        }

        // cvars
        if (sub->JumpToKey("cvars"))
        {
            for (KeyValues *p = sub->GetFirstSubKey(); p; p = p->GetNextKey())
            {
                info.cvars[p->GetName()] = p->GetString();
            }
            sub->GoBack();
        }

        // commands
        if (sub->JumpToKey("commands"))
        {
            for (KeyValues *p = sub->GetFirstSubKey(); p; p = p->GetNextKey())
            {
                const char *cmd = p->GetString();
                if (cmd && cmd[0])
                {
                    info.commands.push_back(cmd);
                }
            }
            sub->GoBack();
        }

        m_Modes.push_back(info);
    }

    kv->deleteThis();
    return true;
}

void ModeGroupExtension::CollectPluginsFromDir(const char *relativeDir, std::vector<std::string> &outFiles, bool recursive)
{
    char fullPath[PLATFORM_MAX_PATH];
    g_pSM->BuildPath(Path_SM, fullPath, sizeof(fullPath), "plugins/%s", relativeDir);

    ILibrarySys *libsys = (ILibrarySys *)sharesys->RequestInterface(ILibrarySys_Name);
    if (!libsys)
        return;

    IDirectory *dir = libsys->OpenDirectory(fullPath);
    if (!dir)
        return;

    while (dir->MoreFiles())
    {
        if (dir->IsEntryDirectory())
        {
            if (recursive && strcmp(dir->GetEntryName(), ".") != 0 && strcmp(dir->GetEntryName(), "..") != 0)
            {
                char subDir[PLATFORM_MAX_PATH];
                g_pSM->Format(subDir, sizeof(subDir), "%s/%s", relativeDir, dir->GetEntryName());
                CollectPluginsFromDir(subDir, outFiles, recursive);
            }
        }
        else
        {
            const char *name = dir->GetEntryName();
            if (strstr(name, ".smx") || strstr(name, ".SMX"))
            {
                char relativePath[PLATFORM_MAX_PATH];
                g_pSM->Format(relativePath, sizeof(relativePath), "%s/%s", relativeDir, name);
                outFiles.push_back(relativePath);
            }
        }
        dir->NextEntry();
    }
    dir->Release();
}

void ModeGroupExtension::UnloadCurrentModePlugins()
{
    IPlayerHelpers *playerhelpers = (IPlayerHelpers *)sharesys->RequestInterface(IPlayerHelpers_Name);
    if (playerhelpers)
    {
        IPluginManager *mgr = playerhelpers->GetPluginManager();
        for (IPlugin *pl : m_LoadedPlugins)
        {
            mgr->UnloadPlugin(pl);
        }
    }
    m_LoadedPlugins.clear();
}

void ModeGroupExtension::LoadModePlugins(const std::vector<std::string> &files)
{
    IPlayerHelpers *playerhelpers = (IPlayerHelpers *)sharesys->RequestInterface(IPlayerHelpers_Name);
    if (playerhelpers)
    {
        IPluginManager *mgr = playerhelpers->GetPluginManager();
        for (const std::string &file : files)
        {
            char error[255];
            bool wasLoaded = false;

            IPlugin *pl = mgr->LoadPlugin(file.c_str(), false, SourceMod::PluginType_Private, error, sizeof(error), &wasLoaded);
            if (!pl)
            {
                g_pSM->LogError(myself, "Failed to load %s: %s", file.c_str(), error);
            }
        }
    }
}

void ModeGroupExtension::ApplyModeSettings(const ModeInfo &mode)
{
    for (const auto &it : mode.cvars)
    {
        ConVar *cvar = g_pCVar->FindVar(it.first.c_str());
        if (cvar)
        {
            cvar->SetValue(it.second.c_str());
        }
        else
        {
            g_pSM->LogError(myself, "Cvar %s not found", it.first.c_str());
        }
    }

    for (const std::string &cmd : mode.commands)
    {
        g_pSM->InsertServerCommand(cmd.c_str());
    }
}

bool ModeGroupExtension::SwitchToMode(const char *modeName)
{
    const ModeInfo *newMode = nullptr;
    for (const auto &mode : m_Modes)
    {
        if (mode.name == modeName)
        {
            newMode = &mode;
            break;
        }
    }
    if (!newMode)
        return false;

    UnloadCurrentModePlugins();

    m_IsSwitching = true;
    LoadModePlugins(newMode->pluginFiles);
    ApplyModeSettings(*newMode);
    m_IsSwitching = false;

    m_CurrentMode = modeName;
    return true;
}

void ModeGroupExtension::OnModeCommand(const CCommand &command)
{
    if (command.ArgC() < 2)
    {
        g_pSM->LogMessage(myself, "Usage: sm_mode <modename>");
        return;
    }

    const char *modeName = command.Arg(1);
    if (g_ModeGroup.SwitchToMode(modeName))
    {
        g_pSM->LogMessage(myself, "Switched to mode '%s'", modeName);
    }
    else
    {
        g_pSM->LogMessage(myself, "Unknown mode '%s'", modeName);
    }
}