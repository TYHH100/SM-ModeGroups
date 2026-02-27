#ifndef _INCLUDE_SOURCEMOD_EXTENSION_CONFIG_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_CONFIG_H_

#define SMEXT_CONF_NAME         "Mode Group Manager"
#define SMEXT_CONF_DESCRIPTION  "Manages plugin loading/unloading by mode groups"
#define SMEXT_CONF_VERSION      "1.0.0"
#define SMEXT_CONF_AUTHOR       ""
#define SMEXT_CONF_URL          ""
#define SMEXT_CONF_LOGTAG       "MODEGROUP"
#define SMEXT_CONF_LICENSE      ""
#define SMEXT_CONF_DATESTRING   __DATE__

#define SMEXT_LINK(name) SDKExtension *g_pExtensionIface = name;

#define SMEXT_ENABLE_FORWARDSYS
#define SMEXT_ENABLE_PLUGINCOMPAT
#define SMEXT_ENABLE_PLUGINSYS
#define SMEXT_ENABLE_TEXTPARSERS

#endif // _INCLUDE_SOURCEMOD_EXTENSION_CONFIG_H_