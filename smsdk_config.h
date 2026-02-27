#ifndef _SM_SDK_CONFIG_H_
#define _SM_SDK_CONFIG_H_

#define SMEXT_CONF_NAME			"ModeGroup"
#define SMEXT_CONF_DESCRIPTION	"Plugin group switcher based on config files"
#define SMEXT_CONF_VERSION		"1.0.0.0"
#define SMEXT_CONF_AUTHOR		"TYHH100"
#define SMEXT_CONF_URL			"https://github.com/TYHH100"
#define SMEXT_CONF_LOGTAG		"MODEGROUP"
#define SMEXT_CONF_LICENSE		""
#define SMEXT_CONF_DATESTRING	__DATE__

#define SMEXT_LINK(name) \
	extern "C" { \
		SourceMod::IExtension *GetSMExtAPI() { return (SourceMod::IExtension *)&name; } \
	}

#define SMEXT_ENABLE_FORWARDSYS
#define SMEXT_ENABLE_HANDLESYS
#define SMEXT_ENABLE_PLUGINSYS
#define SMEXT_ENABLE_CORESYS
#define SMEXT_ENABLE_SHARESYS
#define SMEXT_ENABLE_CONSOLE

#endif // _SM_SDK_CONFIG_H_