#ifndef _INCLUDE_SOURCEMOD_EXTENSION_CONFIG_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_CONFIG_H_

#define SMEXT_CONF_NAME         "Mode Group Manager"
#define SMEXT_CONF_DESCRIPTION  "Manages plugin loading/unloading by mode groups"
#define SMEXT_CONF_VERSION      "1.0.2"
#define SMEXT_CONF_AUTHOR       "TYHH100"
#define SMEXT_CONF_URL          "https://github.com/TYHH100/SM-ModeGroups"
#define SMEXT_CONF_LOGTAG       "MODEGROUP"
#define SMEXT_CONF_LICENSE      "MIT"
#define SMEXT_CONF_DATESTRING   __DATE__

#define SMEXT_LINK(name) SDKExtension *g_pExtensionIface = name;

#define SMEXT_CONF_METAMOD
#define SMEXT_ENABLE_FORWARDSYS
#define SMEXT_ENABLE_PLUGINSYS
#define SMEXT_ENABLE_TEXTPARSERS
#define SMEXT_ENABLE_GAMEHELPERS
#define SMEXT_ENABLE_LIBSYS
#define SMEXT_ENABLE_ROOTCONSOLEMENU

#endif // _INCLUDE_SOURCEMOD_EXTENSION_CONFIG_H_