#if defined _modegroup_included
 #endinput
#endif
#define _modegroup_included

/**
 * Switches to a specified mode group.
 *
 * @param groupName         Name of the mode group to switch to.
 * @return                True if successful, false otherwise.
 */
native bool ModeGroup_Switch(const char[] groupName);

/**
 * Gets the name of the currently active mode group.
 *
 * @param buffer           Buffer to store the group name.
 * @param maxlen           Maximum length of the buffer.
 */
native void ModeGroup_GetCurrent(char[] buffer, int maxlen);

/**
 * Reloads the mode group configuration file.
 */
native void ModeGroup_ReloadConfig();

/**
 * Called when the mode group changes.
 *
 * @param oldGroup         Name of the previous mode group (empty string if none).
 * @param newGroup         Name of the new mode group.
 */
forward void OnModeGroupChanged(const char[] oldGroup, const char[] newGroup);

#if !defined REQUIRE_EXTENSIONS
public void __pl_modegroup_SetNTVOptional()
{
	MarkNativeAsOptional("ModeGroup_Switch");
	MarkNativeAsOptional("ModeGroup_GetCurrent");
	MarkNativeAsOptional("ModeGroup_ReloadConfig");
}
#endif

public Extension __ext_modegroup = 
{
	name = "modegroup",
	file = "modegroup.ext",
#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};