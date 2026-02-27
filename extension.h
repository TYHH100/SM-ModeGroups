#include "smsdk_ext.h"
#include <vector>
#include <string>

class ModeManager : public SDKExtension {
public:
    virtual bool SDK_OnLoad(char *error, size_t maxlength, bool late);
    virtual void SDK_OnUnload();
private:
    void SwitchMode(const char *modeName);
    void LoadPluginsRecursive(const char *basePath, const char *relPath);
    void UnloadActivePlugins();
private:
    std::vector<IPlugin *> m_ActivePlugins;
};