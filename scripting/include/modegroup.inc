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

public SharedPlugin __pl_modegroup = 
{
	name = "modegroup",
	file = "modegroup.ext",
#if defined REQUIRE_PLUGIN
	required = 1
#else
	required = 0
#endif
};

#if !defined REQUIRE_PLUGIN
public void __pl_modegroup_SetNTVOptional()
{
	MarkNativeAsOptional("ModeGroup_Switch");
	MarkNativeAsOptional("ModeGroup_GetCurrent");
	MarkNativeAsOptional("ModeGroup_ReloadConfig");
}
#endif
