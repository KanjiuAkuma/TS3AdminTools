/*
 * TeamSpeak 3 demo plugin
 *
 * Copyright (c) 2008-2017 TeamSpeak Systems GmbH
 */

#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <Windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "teamspeak/public_definitions.h"
#include "teamspeak/public_rare_definitions.h"
#include "teamspeak/clientlib_publicdefinitions.h"
#include "ts3_functions.h"
#include "plugin.h"
#include <vector>

static struct TS3Functions ts3Functions;

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); (dest)[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION 23

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128

static char* pluginID = NULL;

#ifdef _WIN32
/* Helper function to convert wchar_T to Utf-8 encoded strings on Windows */
static int wcharToUtf8(const wchar_t* str, char** result) {
	int outlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
	*result = (char*)malloc(outlen);
	if (WideCharToMultiByte(CP_UTF8, 0, str, -1, *result, outlen, 0, 0) == 0) {
		*result = NULL;
		return -1;
	}
	return 0;
}
#endif


/*********************************** My Macros ************************************/
/*
 *
 */
#ifdef AT_RELEASE
#define ASSERT(x, msg)
#define R_ASSERT(x, msg) if (!(x)) {return;}
#define CALL(x, msg) {unsigned int r = x;}
#define R_CALL(x, msg) {unsigned int r = x; if (r != ERROR_ok) {return;}}
#define RV_CALL(x, msg, rv) {unsigned int r = x; if (r != ERROR_ok) {return rv;}}
#else
#define ASSERT(x, msg) if (!(x)) printf(msg)
#define R_ASSERT(x, msg) if (!(x)) {printf(msg); return;}
#define CALL(x, msg) {unsigned int r = x; if (r != ERROR_ok) {printf("Error %d at '%s'\n", r, msg);}}
#define R_CALL(x, msg) {unsigned int r = x; if (r != ERROR_ok) {printf("Error %d at '%s'\n", r, msg); return;}}
#define RV_CALL(x, msg, rv) {unsigned int r = x; if (r != ERROR_ok) {printf("Error %d at '%s'\n", r, msg); return rv;}}
#endif


 /*********************************** UI Variables ************************************/
 /*
  *
  */
static bool channel_selected = false;
static uint64 selected_channel = 0;
static bool user_selected = false;
static anyID selected_user = 0;


/*********************************** Follow variables ************************************/
/*
 *
 */
static bool follow_enable = false;
static uint64 follow_target_db_id = 0;


/*********************************** Locked user variables ************************************/
/*
 *
 */
static std::vector<uint64> locked_users = std::vector<uint64>();
static std::vector<uint64> locked_user_channels = std::vector<uint64>();

/*********************************** Menu Item Ids ************************************/
/*
 *
 */
enum {
	MENU_ID_CHANNEL_FROM,
	MENU_ID_CHANNEL_TO,
	MENU_ID_CLIENT_FOLLOW,
	MENU_ID_CLIENT_UNFOLLOW,
	MENU_ID_CLIENT_LOCK_MOVEMENT,
	MENU_ID_CLIENT_UNLOCK_MOVEMENT,
	MENU_ID_GLOBAL_UNFOLLOW,
};

/*********************************** Required functions ************************************/
/*
 * If any of these required functions is not implemented, TS3 will refuse to load the plugin
 */

/* Unique name identifying this plugin */
const char* ts3plugin_name() {
#ifdef _WIN32
	/* TeamSpeak expects UTF-8 encoded characters. Following demonstrates a possibility how to convert UTF-16 wchar_t into UTF-8. */
	static char* result = NULL;  /* Static variable so it's allocated only once */
	if(!result) {
		const wchar_t* name = L"JAdminTools";
		if(wcharToUtf8(name, &result) == -1) {  /* Convert name into UTF-8 encoded result */
			result = "JAdminTools";  /* Conversion failed, fallback here */
		}
	}
	return result;
#else
	return "JAdminTools";
#endif
}

/* Plugin version */
const char* ts3plugin_version() {
    return "1.0";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
	return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "Joscha Vack";
}

/* Plugin description */
const char* ts3plugin_description() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "Utility functions for ts3";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

/*
 * Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
 * If the function returns 1 on failure, the plugin will be unloaded again.
 */
int ts3plugin_init() {
    char appPath[PATH_BUFSIZE];
    char resourcesPath[PATH_BUFSIZE];
    char configPath[PATH_BUFSIZE];
	char pluginPath[PATH_BUFSIZE];

    /* Your plugin init code here */
    printf("PLUGIN: init\n");

    /* Example on how to query application, resources and configuration paths from client */
    /* Note: Console client returns empty string for app and resources path */
    ts3Functions.getAppPath(appPath, PATH_BUFSIZE);
    ts3Functions.getResourcesPath(resourcesPath, PATH_BUFSIZE);
    ts3Functions.getConfigPath(configPath, PATH_BUFSIZE);
	ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE, pluginID);

	printf("PLUGIN: App path: %s\nResources path: %s\nConfig path: %s\nPlugin path: %s\n", appPath, resourcesPath, configPath, pluginPath);

    return 0;  /* 0 = success, 1 = failure, -2 = failure but client will not show a "failed to load" warning */
	/* -2 is a very special case and should only be used if a plugin displays a dialog (e.g. overlay) asking the user to disable
	 * the plugin again, avoiding the show another dialog by the client telling the user the plugin failed to load.
	 * For normal case, if a plugin really failed to load because of an error, the correct return value is 1. */
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
    /* Your plugin cleanup code here */
    printf("PLUGIN: shutdown\n");

	/*
	 * Note:
	 * If your plugin implements a settings dialog, it must be closed and deleted here, else the
	 * TeamSpeak client will most likely crash (DLL removed but dialog from DLL code still open).
	 */

	/* Free pluginID if we registered it */
	if(pluginID) {
		free(pluginID);
		pluginID = NULL;
	}
}

/****************************** Optional functions ********************************/
/*
 * Following functions are optional, if not needed you don't need to implement them.
 */

/* Tell client if plugin offers a configuration window. If this function is not implemented, it's an assumed "does not offer" (PLUGIN_OFFERS_NO_CONFIGURE). */
int ts3plugin_offersConfigure() {
	printf("PLUGIN: offersConfigure\n");
	/*
	 * Return values:
	 * PLUGIN_OFFERS_NO_CONFIGURE         - Plugin does not implement ts3plugin_configure
	 * PLUGIN_OFFERS_CONFIGURE_NEW_THREAD - Plugin does implement ts3plugin_configure and requests to run this function in an own thread
	 * PLUGIN_OFFERS_CONFIGURE_QT_THREAD  - Plugin does implement ts3plugin_configure and requests to run this function in the Qt GUI thread
	 */
	return PLUGIN_OFFERS_NO_CONFIGURE;  /* In this case ts3plugin_configure does not need to be implemented */
}

/* Plugin might offer a configuration window. If ts3plugin_offersConfigure returns 0, this function does not need to be implemented. */
void ts3plugin_configure(void* handle, void* qParentWidget) {
    printf("PLUGIN: configure\n");
}

/*
 * If the plugin wants to use error return codes, plugin commands, hotkeys or menu items, it needs to register a command ID. This function will be
 * automatically called after the plugin was initialized. This function is optional. If you don't use these features, this function can be omitted.
 * Note the passed pluginID parameter is no longer valid after calling this function, so you must copy it and store it in the plugin.
 */
void ts3plugin_registerPluginID(const char* id) {
	const size_t sz = strlen(id) + 1;
	pluginID = (char*)malloc(sz * sizeof(char));
	_strcpy(pluginID, sz, id);  /* The id buffer will invalidate after exiting this function */
	printf("PLUGIN: registerPluginID: %s\n", pluginID);
}

/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword() {
	return NULL;
}

static void print_and_free_bookmarks_list(struct PluginBookmarkList* list)
{
	ts3Functions.freeMemory(list);
}

/* Plugin processes console command. Return 0 if plugin handled the command, 1 if not handled. */
int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) {
	return 1;
}

/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {}

/*
 * Implement the following three functions when the plugin should display a line in the server/channel/client info.
 * If any of ts3plugin_infoTitle, ts3plugin_infoData or ts3plugin_freeMemory is missing, the info text will not be displayed.
 */

/* Static title shown in the left column in the info frame */
const char* ts3plugin_infoTitle() {
	return "I NEED THIS";
}

/*
 * Dynamic content shown in the right column in the info frame. Memory for the data string needs to be allocated in this
 * function. The client will call ts3plugin_freeMemory once done with the string to release the allocated memory again.
 * Check the parameter "type" if you want to implement this feature only for specific item types. Set the parameter
 * "data" to NULL to have the client ignore the info data.
 */
void ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, char** data) {
	if (type == PLUGIN_CHANNEL) {
		anyID clientID;
		R_CALL(ts3Functions.getClientID(serverConnectionHandlerID, &clientID), "Error retrieving client id!");

		uint64 clientChannelID;
		R_CALL(ts3Functions.getChannelOfClient(serverConnectionHandlerID, clientID, &clientChannelID), "Error retrieving client channel!");

		if (clientChannelID == id) { // channel is clients channel -> disable move menu items
			ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CHANNEL_FROM, 0);
			ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CHANNEL_TO, 0);
			selected_channel = 0;
			channel_selected = false;
		}
		else {
			ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CHANNEL_FROM, 1);
			ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CHANNEL_TO, 1);
			selected_channel = id;
			channel_selected = true;
		}
	}
	
	if (type == PLUGIN_CLIENT) {
		anyID myClientID;
		R_CALL(ts3Functions.getClientID(serverConnectionHandlerID, &myClientID), "Error retrieving client id!");

		if (myClientID == id) { // is myself -> disable follow menu items
			ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_FOLLOW, 0);
			ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNFOLLOW, 0);
			ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_LOCK_MOVEMENT, 0);
			ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNLOCK_MOVEMENT, 0);
			user_selected = false;
			selected_user = 0;
		}
		else {
			user_selected = true;
			selected_user = id;

			uint64 clientDBID;
			R_CALL(ts3Functions.getClientVariableAsUInt64(serverConnectionHandlerID, selected_user, CLIENT_DATABASE_ID, &clientDBID), "Error retreiving client db id!");

			if (follow_enable && follow_target_db_id == clientDBID) {
				ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_FOLLOW, 0);
				ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNFOLLOW, 1);
			}
			else {
				ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_FOLLOW, 1);
				ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNFOLLOW, 0);
			}

			if (!locked_users.empty() && std::find(locked_users.begin(), locked_users.end(), clientDBID) != locked_users.cend()) {
				ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_LOCK_MOVEMENT, 0);
				ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNLOCK_MOVEMENT, 1);
			}
			else {
				ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_LOCK_MOVEMENT, 1);
				ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNLOCK_MOVEMENT, 0);
			}
		}
		
	}
}

/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData and ts3plugin_initMenus */
void ts3plugin_freeMemory(void* data) {
	free(data);
}

/*
 * Plugin requests to be always automatically loaded by the TeamSpeak 3 client unless
 * the user manually disabled it in the plugin dialog.
 * This function is optional. If missing, no autoload is assumed.
 */
int ts3plugin_requestAutoload() {
	return 0;  /* 1 = request autoloaded, 0 = do not request autoload */
}

/* Helper function to create a menu item */
static struct PluginMenuItem* createMenuItem(enum PluginMenuType type, int id, const char* text, const char* icon) {
	struct PluginMenuItem* menuItem = (struct PluginMenuItem*)malloc(sizeof(struct PluginMenuItem));
	menuItem->type = type;
	menuItem->id = id;
	_strcpy(menuItem->text, PLUGIN_MENU_BUFSZ, text);
	_strcpy(menuItem->icon, PLUGIN_MENU_BUFSZ, icon);
	return menuItem;
}

/* Some makros to make the code to create menu items a bit more readable */
#define BEGIN_CREATE_MENUS(x) const size_t sz = x + 1; size_t n = 0; *menuItems = (struct PluginMenuItem**)malloc(sizeof(struct PluginMenuItem*) * sz);
#define CREATE_MENU_ITEM(a, b, c, d) (*menuItems)[n++] = createMenuItem(a, b, c, d);
#define END_CREATE_MENUS (*menuItems)[n++] = NULL; assert(n == sz);

/*
 * Initialize plugin menus.
 * This function is called after ts3plugin_init and ts3plugin_registerPluginID. A pluginID is required for plugin menus to work.
 * Both ts3plugin_registerPluginID and ts3plugin_freeMemory must be implemented to use menus.
 * If plugin menus are not used by a plugin, do not implement this function or return NULL.
 */
void ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon) {
	BEGIN_CREATE_MENUS(7);  /* IMPORTANT: Number of menu items must be correct! */
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CHANNEL, MENU_ID_CHANNEL_FROM, "Move all users from this channel to your channel", "1.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CHANNEL, MENU_ID_CHANNEL_TO, "Move all users from your channel to this channel", "2.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CLIENT, MENU_ID_CLIENT_FOLLOW, "Follow", "3.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CLIENT, MENU_ID_CLIENT_UNFOLLOW, "Unfollow", "4.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CLIENT, MENU_ID_CLIENT_LOCK_MOVEMENT, "Lock movement", "5.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CLIENT, MENU_ID_CLIENT_UNLOCK_MOVEMENT, "Unlock movement", "6.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_UNFOLLOW, "Unfollow", "7.png")
	END_CREATE_MENUS;  /* Includes an assert checking if the number of menu items matched */

	// disable 'reverse actions'
	ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNFOLLOW, 0);
	ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNLOCK_MOVEMENT, 0);
}

/* Helper function to create a hotkey */
static struct PluginHotkey* createHotkey(const char* keyword, const char* description) {
	struct PluginHotkey* hotkey = (struct PluginHotkey*)malloc(sizeof(struct PluginHotkey));
	_strcpy(hotkey->keyword, PLUGIN_HOTKEY_BUFSZ, keyword);
	_strcpy(hotkey->description, PLUGIN_HOTKEY_BUFSZ, description);
	return hotkey;
}

/* Some makros to make the code to create hotkeys a bit more readable */
#define BEGIN_CREATE_HOTKEYS(x) const size_t sz = x + 1; size_t n = 0; *hotkeys = (struct PluginHotkey**)malloc(sizeof(struct PluginHotkey*) * sz);
#define CREATE_HOTKEY(a, b) (*hotkeys)[n++] = createHotkey(a, b);
#define END_CREATE_HOTKEYS (*hotkeys)[n++] = NULL; assert(n == sz);

/*
 * Initialize plugin hotkeys. If your plugin does not use this feature, this function can be omitted.
 * Hotkeys require ts3plugin_registerPluginID and ts3plugin_freeMemory to be implemented.
 * This function is automatically called by the client after ts3plugin_init.
 */
void ts3plugin_initHotkeys(struct PluginHotkey*** hotkeys) {
	/* Register hotkeys giving a keyword and a description.
	 * The keyword will be later passed to ts3plugin_onHotkeyEvent to identify which hotkey was triggered.
	 * The description is shown in the clients hotkey dialog. */
	
	BEGIN_CREATE_HOTKEYS(4);  // Create hotkeys. Size must be correct for allocating memory.
	CREATE_HOTKEY("MoveToOwnChannel", "Move clients from selected channel to my channel");
	CREATE_HOTKEY("MoveToSelectedChannel", "Move clients from my channel to selected channel");
	CREATE_HOTKEY("Follow", "Follow user");
	CREATE_HOTKEY("Unfollow", "Unfollow user");
	END_CREATE_HOTKEYS;

	/* The client will call ts3plugin_freeMemory to release all allocated memory */
}

/************************** TeamSpeak callbacks ***************************/
/*
 * Following functions are optional, feel free to remove unused callbacks.
 * See the clientlib documentation for details on each function.
 */

/* Clientlib */
/*
 * Called when a plugin menu item (see ts3plugin_initMenus) is triggered. Optional function, when not using plugin menus, do not implement this.
 *
 * Parameters:
 * - serverConnectionHandlerID: ID of the current server tab
 * - type: Type of the menu (PLUGIN_MENU_TYPE_CHANNEL, PLUGIN_MENU_TYPE_CLIENT or PLUGIN_MENU_TYPE_GLOBAL)
 * - menuItemID: Id used when creating the menu item
 * - selectedItemID: Channel or Client ID in the case of PLUGIN_MENU_TYPE_CHANNEL and PLUGIN_MENU_TYPE_CLIENT. 0 for PLUGIN_MENU_TYPE_GLOBAL.
 */
void ts3plugin_onMenuItemEvent(uint64 serverConnectionHandlerID, enum PluginMenuType type, int menuItemID, uint64 selectedItemID) {
	printf("PLUGIN: onMenuItemEvent: serverConnectionHandlerID=%llu, type=%d, menuItemID=%d, selectedItemID=%llu\n", (long long unsigned int)serverConnectionHandlerID, type, menuItemID, (long long unsigned int)selectedItemID);
	switch (menuItemID) {
	case MENU_ID_CHANNEL_FROM:
		// Menu channel 1 was triggered (move users to yours)
		moveClientsToOwnChannel(serverConnectionHandlerID, selectedItemID);
		break;
	case MENU_ID_CHANNEL_TO:
		// Menu channel 2 was triggered (move users to target)
		moveClientsToSelectedChannel(serverConnectionHandlerID, selectedItemID);
		break;
	case MENU_ID_CLIENT_FOLLOW:
		ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_FOLLOW, 0);
		ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNFOLLOW, 1);
		enableFollow(serverConnectionHandlerID, selectedItemID);
		break;
	case MENU_ID_CLIENT_UNFOLLOW:
		ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_FOLLOW, 1);
		ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNFOLLOW, 0);
		disableFollow();
		break;
	case MENU_ID_CLIENT_LOCK_MOVEMENT:
		ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_LOCK_MOVEMENT, 0);
		ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNLOCK_MOVEMENT, 1);
		lockUser(serverConnectionHandlerID, selectedItemID);
		break;
	case MENU_ID_CLIENT_UNLOCK_MOVEMENT:
		ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_LOCK_MOVEMENT, 1);
		ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNLOCK_MOVEMENT, 0);
		unlockUser(serverConnectionHandlerID, selectedItemID);
		break;
	case MENU_ID_GLOBAL_UNFOLLOW:
		disableFollow();
	}
	
}

/* This function is called if a plugin hotkey was pressed. Omit if hotkeys are unused. */
void ts3plugin_onHotkeyEvent(const char* keyword) {
	printf("PLUGIN: Hotkey event: %s\n", keyword);
	/* Identify the hotkey by keyword ("keyword_1", "keyword_2" or "keyword_3" in this example) and handle here... */
	if (strncmp(keyword, "MoveToOwnChannel", strlen(keyword)) == 0 && channel_selected) {
		printf("Moving to own channel!\n");
		moveClientsToOwnChannel(ts3Functions.getCurrentServerConnectionHandlerID(), selected_channel);
	}
	else if (strncmp(keyword, "MoveToSelectedChannel", strlen(keyword)) == 0 && channel_selected) {
		printf("Moving to selected channel!\n");
		moveClientsToSelectedChannel(ts3Functions.getCurrentServerConnectionHandlerID(), selected_channel);
	}
	else if (strncmp(keyword, "Follow", strlen(keyword)) == 0 && user_selected) {
		ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_FOLLOW, 0);
		ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNFOLLOW, 1);
		enableFollow(ts3Functions.getCurrentServerConnectionHandlerID(), selected_user);
	}
	else if (strncmp(keyword, "Unfollow", strlen(keyword)) == 0 && follow_enable) {
		ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_FOLLOW, 1);
		ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_CLIENT_UNFOLLOW, 0);
		disableFollow();
	}
}

/* Called when recording a hotkey has finished after calling ts3Functions.requestHotkeyInputDialog */
void ts3plugin_onHotkeyRecordedEvent(const char* keyword, const char* key) {

}

// This function receives your key Identifier you send to notifyKeyEvent and should return
// the friendly device name of the device this hotkey originates from. Used for display in UI.
const char* ts3plugin_keyDeviceName(const char* keyIdentifier) {
	return NULL;
}

// This function translates the given key identifier to a friendly key name for display in the UI
const char* ts3plugin_displayKeyText(const char* keyIdentifier) {
	return NULL;
}

// This is used internally as a prefix for hotkeys so we can store them without collisions.
// Should be unique across plugins.
const char* ts3plugin_keyPrefix() {
	return "JAT";
}

void ts3plugin_onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage) {
	printf("Client moved (Self)! clid=%d, oCid=%llu, nCid=%llu\n", clientID, oldChannelID, newChannelID);
	onClientMoved(serverConnectionHandlerID, clientID, oldChannelID, newChannelID, false, "Changed channel");
}

void ts3plugin_onClientMoveTimeoutEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* timeoutMessage) {
	printf("Client moved (Timeout)! clid=%d, oCid=%llu, nCid=%llu\n", clientID, oldChannelID, newChannelID);
	onClientMoved(serverConnectionHandlerID, clientID, oldChannelID, newChannelID, true, "Timouted");
}

void ts3plugin_onClientMoveMovedEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID moverID, const char* moverName, const char* moverUniqueIdentifier, const char* moveMessage) {
	printf("Client moved (Got moved)! clid=%d, oCid=%llu, nCid=%llu\n", clientID, oldChannelID, newChannelID);
	onClientMoved(serverConnectionHandlerID, clientID, oldChannelID, newChannelID, true, "Got moved");
}

void ts3plugin_onClientKickFromChannelEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage) {
	printf("Client moved (Kick from channel! clid=%d, oCid=%llu, nCid=%llu\n", clientID, oldChannelID, newChannelID);
	onClientMoved(serverConnectionHandlerID, clientID, oldChannelID, newChannelID, true, "Channel kick");
}

void ts3plugin_onClientKickFromServerEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage) {
	printf("Client moved (Kick from server)! clid=%d, oCid=%llu, nCid=%llu\n", clientID, oldChannelID, newChannelID);
	onClientMoved(serverConnectionHandlerID, clientID, oldChannelID, newChannelID, true, "Server kick");
}


void moveClientsToSelectedChannel(uint64 serverConnectionHandlerID, uint64 channelID) {
	anyID clientID;
	R_CALL(ts3Functions.getClientID(serverConnectionHandlerID, &clientID), "Error retrieving client id!");

	uint64 clientChannelID;
	R_CALL(ts3Functions.getChannelOfClient(serverConnectionHandlerID, clientID, &clientChannelID), "Error retrieving client channel!");

	anyID *clients;
	R_CALL(ts3Functions.getChannelClientList(serverConnectionHandlerID, clientChannelID, &clients), "Error retrieving channel client list!");


	for (anyID* c = clients; *c != (anyID) NULL; c++) {
		printf("Moving client %hu\n", *c);
		CALL(ts3Functions.requestClientMove(serverConnectionHandlerID, *c, channelID, "", NULL), "Error moving client!");
	}
}

void moveClientsToOwnChannel(uint64 serverConnectionHandlerID, uint64 channelID) {
	anyID clientID;
	R_CALL(ts3Functions.getClientID(serverConnectionHandlerID, &clientID), "Error retrieving client id!");

	uint64 clientChannelID;
	R_CALL(ts3Functions.getChannelOfClient(serverConnectionHandlerID, clientID, &clientChannelID), "Error retrieving client channel!");

	anyID *clients;
	R_CALL(ts3Functions.getChannelClientList(serverConnectionHandlerID, channelID, &clients), "Error retrieving channel client list!");

	for (anyID* c = clients; *c != (anyID) NULL; c++) {
		printf("Moving client %hu\n", *c);
		CALL(ts3Functions.requestClientMove(serverConnectionHandlerID, *c, clientChannelID, "", NULL), "Error moving client!");
	}
	ts3Functions.freeMemory(clients);

}

struct move_data {
	uint64 serverConnectionHandlerID;
	anyID clientID;
	uint64 oldChannelID;
	uint64 newChannelID;
	bool was_moved;
};

void onClientMoved(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, bool was_moved, const char* moveType) {
	static move_data last_move = move_data{ 0, 0, 0, 0, false };
	if (newChannelID == 0) {
		// client left server
		printf("Client %d left server\n", clientID);
	}

	printf("Client move ('%s'), clid=%d, oCid=%llu, nCid=%llu, was_moved=%d\n", moveType, clientID, oldChannelID, newChannelID, was_moved);
	if (last_move.clientID == clientID && last_move.oldChannelID == oldChannelID && last_move.newChannelID == newChannelID && last_move.was_moved == was_moved) {
		printf("Repeatmove, skipping\n");
		last_move = move_data{ 0, 0, 0, 0, false };
		return;
	}
	last_move = move_data{ serverConnectionHandlerID, clientID, oldChannelID, newChannelID, was_moved };

	uint64 clientDBID;
	R_CALL(ts3Functions.getClientVariableAsUInt64(serverConnectionHandlerID, clientID, CLIENT_DATABASE_ID, &clientDBID), "Error retreiving client db id!");

	if (follow_enable) {
		if (follow_target_db_id == clientDBID) {
			follow(serverConnectionHandlerID, newChannelID);
		}
	}

	if (!locked_users.empty()) {
		const auto it = std::find(locked_users.begin(), locked_users.end(), clientDBID);
		if (it != locked_users.cend()) {
			const int ndx = std::distance(locked_users.begin(), it);
			if (!was_moved) {
				printf("Restricting user movement clid=%d\n", clientID);
				const uint64 locked_channel = locked_user_channels[ndx];
				if (newChannelID != locked_channel) {
					CALL(ts3Functions.requestClientMove(serverConnectionHandlerID, clientID, locked_channel, "", NULL), "Error moving client!");
				}
			}
			else {
				printf("Updating movement restricted user channel clid=%d, cid=%llu\n", clientID, newChannelID);
				locked_user_channels[ndx] = newChannelID;
			}
		}
	}
}

void lockUser(uint64 serverConnectionHandlerID, anyID userID) {
	uint64 userChannel;
	R_CALL(ts3Functions.getChannelOfClient(serverConnectionHandlerID, userID, &userChannel), "Error retrieving client channel!");

	uint64 clientDBID;
	R_CALL(ts3Functions.getClientVariableAsUInt64(serverConnectionHandlerID, userID, CLIENT_DATABASE_ID, &clientDBID), "Error retreiving client db id!");

	R_ASSERT(std::find(locked_users.begin(), locked_users.end(), clientDBID) == locked_users.cend(), "Error trying to lock already locked user!");
	locked_users.push_back(clientDBID);
	locked_user_channels.push_back(userChannel);
}

void unlockUser(uint64 serverConnectionHandlerID, anyID userID) {
	uint64 clientDBID;
	R_CALL(ts3Functions.getClientVariableAsUInt64(serverConnectionHandlerID, userID, CLIENT_DATABASE_ID, &clientDBID), "Error retreiving client db id!");

	R_ASSERT(!locked_users.empty(), "Error trying to unlock user but no users locked!");
	const auto it = std::find(locked_users.begin(), locked_users.end(), clientDBID);
	R_ASSERT(it != locked_users.cend(), "Error trying to unlock non-locked user!");
	const int ndx = std::distance(locked_users.begin(), it);
	locked_users.erase(it);
	locked_user_channels.erase(locked_user_channels.begin() + ndx);
}

void join(uint64 serverConnectionHandlerID, anyID targetClientID) {
	anyID myClientID;
	R_CALL(ts3Functions.getClientID(serverConnectionHandlerID, &myClientID), "Error retrieving client id!");

	uint64 myChannelID;
	R_CALL(ts3Functions.getChannelOfClient(serverConnectionHandlerID, myClientID, &myChannelID), "Error retrieving client channel!");

	uint64 targetChannelId;
	R_CALL(ts3Functions.getChannelOfClient(serverConnectionHandlerID, targetClientID, &targetChannelId), "Error retrieving target channel!");

	if (myChannelID != targetChannelId) {
		CALL(ts3Functions.requestClientMove(serverConnectionHandlerID, myClientID, targetChannelId, "", NULL), "Error moving client!");
	}
}

void enableFollow(uint64 serverConnectionHandlerID, anyID targetID) {
	uint64 clientDBID;
	R_CALL(ts3Functions.getClientVariableAsUInt64(serverConnectionHandlerID, targetID, CLIENT_DATABASE_ID, &clientDBID), "Error retreiving client db id!");
	
	follow_enable = true;
	follow_target_db_id = clientDBID;
	join(serverConnectionHandlerID, targetID);
}

void disableFollow() {
	follow_enable = false;
	follow_target_db_id = 0;
}

void follow(uint64 serverConnectionHandlerID, uint64 newChannelID) {
	anyID myClientID;
	R_CALL(ts3Functions.getClientID(serverConnectionHandlerID, &myClientID), "Error retrieving client id!");

	uint64 myChannelID;
	R_CALL(ts3Functions.getChannelOfClient(serverConnectionHandlerID, myClientID, &myChannelID), "Error retrieving client channel!");

	if (myChannelID != newChannelID) {
		CALL(ts3Functions.requestClientMove(serverConnectionHandlerID, myClientID, newChannelID, "", NULL), "Error moving client!");
	}
}