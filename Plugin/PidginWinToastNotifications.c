#include "PidginWinToastNotifications.h"

ConnectionNode ** connection_root = NULL;
int num_connection_nodes = 0;

// for add_connection_node_to_delete_array
ConnectionNode ** connection_nodes_to_delete = NULL;
int current_connection_node_id = 0;

int compare_connection_nodes (const void *a, const void *b)
{
  const ConnectionNode *ca = (const ConnectionNode *) a;
  const ConnectionNode *cb = (const ConnectionNode *) b;

  return (ca->account > cb->account) - (ca->account < cb->account);
}

static void ensure_prefs_path(const char *path) {
	char *subPath;
	int i = 0;
	gboolean exists;
	while (path[i] != '\0') {
		if (i > 0 && path[i] == '/') {
			subPath = (char *) malloc((i+1) * sizeof(char));
			memcpy(subPath, path, i); // copy everything except '/'
			subPath[i] = '\0';
			exists = purple_prefs_exists(subPath);
			if (!exists) {
				purple_prefs_add_none(subPath);
			}
			free(subPath);
		}
		i++;
	}
}

static const char* get_prefs_sub_path(Setting setting) {
	switch (setting) {
		case SETTING_ENABLED:
			return "/enabled";
		case SETTING_FOR_IM:
			return "/for_im";
		case SETTING_FOR_CHAT:
			return "/for_chat";
		case SETTING_FOR_CHAT_MENTIONED:
			return "/for_chat_mentioned";
		case SETTING_FOR_FOCUS:
			return "/for_focus";
		case SETTING_BUDDY_SIGNED_ON:
			return "/BUDDY_ONLINE";
		case SETTING_BUDDY_SIGNED_OFF:
			return "/BUDDY_OFFLINE";
		default:
			return "";
	}
}

static const char* get_buddy_type_sub_path(Buddy_type buddy_type) {
	switch (buddy_type) {
		case BUDDY_TYPE_GLOBAL:
			return "/global";
		case BUDDY_TYPE_GROUP:
			return "/group";
		case BUDDY_TYPE_BUDDY:
			return "/buddy";
		case BUDDY_TYPE_CHAT:
			return "/chat";
		default:
			return "/unknown";
	}
}

static char* get_old_prefs_path(PurpleStatusPrimitive status, Setting setting) {
	char *ret = 0;
	const char *statusStr = 0;
	const char *settingStr = 0;

	int size = 0;
	int basePathSize = 0;
	int statusSize = 0;
	int settingSize = 0;

	int targetPos = 0;


	if (status != PURPLE_STATUS_UNSET) {
		statusStr = purple_primitive_get_id_from_type(status);
		statusSize = strlen(statusStr) + 1; // +1 for '/'
	}
	settingStr = get_prefs_sub_path(setting);

	basePathSize = strlen(base_settings_path);
	settingSize = strlen(settingStr);

	size = basePathSize + statusSize + settingSize + 1;
	ret = (char *)malloc(size);
	memcpy(ret + targetPos, base_settings_path, basePathSize);
	targetPos += basePathSize;
	if (status != PURPLE_STATUS_UNSET) {
		ret[targetPos] = '/';
		targetPos++;
		memcpy(ret + targetPos, statusStr, statusSize-1);
		targetPos += statusSize-1;
	}
	memcpy(ret + targetPos, settingStr, settingSize);
	targetPos += settingSize;
	ret[targetPos] = '\0';
	return ret;
}

static char* get_prefs_path(
	PurpleStatusPrimitive status,
	Setting setting,
	Buddy_type buddy_type,
	const char *group_name,
	const char *protocol_id,
	const char *account_name,
	const char *buddy_name) {
	// global example: "/plugins/gtk/gallax-win_toast_notifications/global/available/enabled"
	// group example: "/plugins/gtk/gallax-win_toast_notifications/group/<group_name>/available/enabled"
	// buddy example: "/plugins/gtk/gallax-win_toast_notifications/buddy/<protocol_id>/<account_name>/<buddy_name>/available/enabled"
	// chat example: "/plugins/gtk/gallax-win_toast_notifications/chat/<protocol_id>/<account_name>/<chat_name>/available/enabled"
	char *ret = 0;
	const char *statusStr = 0;
	const char *settingStr = 0;
	const char *buddyTypeStr = 0;

	int size = 0;
	int basePathSize = 0;
	int groupSize = 0;
	int protocolSize = 0;
	int accountSize = 0;
	int statusSize = 0;
	int settingSize = 0;
	int buddyTypeSize = 0;
	int buddyNameSize = 0;

	char * pch = 0;
	int targetPos = 0;

	statusStr = purple_primitive_get_id_from_type(status);
	settingStr = get_prefs_sub_path(setting);
	buddyTypeStr = get_buddy_type_sub_path(buddy_type);

	basePathSize = strlen(base_settings_path);
	buddyTypeSize = strlen(buddyTypeStr);
	statusSize = strlen(statusStr) + 1; // +1 for '/'
	settingSize = strlen(settingStr);
	if (buddy_type == BUDDY_TYPE_BUDDY || buddy_type == BUDDY_TYPE_CHAT) {
		protocolSize = strlen(protocol_id) + 1;
		// ignore anything behind a '/', like a XMPP ressource
		accountSize = strlen(account_name);
		pch=strchr(account_name,'/');
		if (pch != NULL) {
			accountSize = pch - account_name;
		}
		accountSize++;
		buddyNameSize = strlen(buddy_name);
		pch=strchr(buddy_name,'/');
		if (pch != NULL) {
			buddyNameSize = pch - buddy_name;
		}
		buddyNameSize++;
	} else if (buddy_type == BUDDY_TYPE_GROUP) {
		groupSize = strlen(group_name) + 1;
	}

	size = basePathSize + statusSize + settingSize + groupSize + protocolSize + buddyTypeSize + accountSize + buddyNameSize + 1;
	ret = (char *)malloc(size);
	memcpy(ret + targetPos, base_settings_path, basePathSize);
	targetPos += basePathSize;
	memcpy(ret + targetPos, buddyTypeStr, buddyTypeSize);
	targetPos += buddyTypeSize;
	if (buddy_type == BUDDY_TYPE_BUDDY || buddy_type == BUDDY_TYPE_CHAT) {
		ret[targetPos] = '/';
		targetPos++;
		memcpy(ret + targetPos, protocol_id, protocolSize-1);
		targetPos += protocolSize-1;
		ret[targetPos] = '/';
		targetPos++;
		memcpy(ret + targetPos, account_name, accountSize-1);
		targetPos += accountSize-1;
		ret[targetPos] = '/';
		targetPos++;
		memcpy(ret + targetPos, buddy_name, buddyNameSize-1);
		targetPos += buddyNameSize-1;
	} else if (buddy_type == BUDDY_TYPE_GROUP) {
		ret[targetPos] = '/';
		targetPos++;
		memcpy(ret + targetPos, group_name, groupSize-1);
		targetPos += groupSize-1;
	}
	ret[targetPos] = '/';
	targetPos++;
	memcpy(ret + targetPos, statusStr, statusSize-1);
	targetPos += statusSize-1;
	memcpy(ret + targetPos, settingStr, settingSize);
	targetPos += settingSize;
	ret[targetPos] = '\0';
	return ret;
}

GtkWidget * get_config_frame(PurplePlugin *plugin) {
	struct localSettingsData *data;
	GtkWidget *vbox;

	data = malloc(sizeof(struct localSettingsData));
	data->paths = 0;

	vbox = gtk_vbox_new(FALSE, 5);

	add_setting_groups(
		BUDDY_TYPE_GLOBAL,
		NULL,
		NULL,
		NULL,
		NULL,
		vbox,
		data
	);
	
	g_signal_connect(G_OBJECT(vbox), "destroy",
	    G_CALLBACK(settings_dialog_destroy_cb), data);

	gtk_widget_show_all(vbox);
	return vbox;
}

static PidginPluginUiInfo pidgin_plugin_info = {
	get_config_frame,
	0,
	NULL,
	NULL,
	NULL,
	NULL
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	PIDGIN_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,

	"gtk-win32-gallax-win_toast_notifications",
	"Windows Toast Notifications",
	"1.6.1",

	"Native Windows Toast Notifications.",
	"Displays native Windows Toast Notifications.",
	"Christian Galla <ChristianGalla@users.noreply.github.com>",
	"https://github.com/ChristianGalla/PidginWinToastNotifications",

	plugin_load,
	plugin_unload,
	NULL,

	&pidgin_plugin_info,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void output_toast_error(int errorNumber, const char *message)
{
	const char * errorMessage = NULL;
	switch (errorNumber)
	{
	case 0:
		errorMessage = "No error. The process was executed correctly";
		break;
	case 1:
		errorMessage = "The library has not been initialized";
		break;
	case 2:
		errorMessage = "The OS does not support WinToast";
		break;
	case 3:
		errorMessage = "The library was not able to create a Shell Link for the app";
		break;
	case 4:
		errorMessage = "The AUMI is not a valid one";
		break;
	case 5:
		errorMessage = "The parameters used to configure the library are not valid normally because an invalid AUMI or App Name";
		break;
	case 6:
		errorMessage = "The toast was created correctly but WinToast was not able to display the toast";
		break;
	case 7:
		errorMessage = "Unknown error";
		break;
	default:
		errorMessage = "Unknown exception";
		break;
	}
	purple_debug_error("win_toast_notifications", "%s: %s\n",
					   message, errorMessage);
}

static char *get_attr_text(const char *protocolName, const char *userName, const char *chatName)
{
	const char *startText = "Via ";
	const char *chatText = "In chat ";
	const char *formatText = " ()";
	const char *formatChat = "\r\n";
	int startTextSize = strlen(startText);
	int formatTextSize = strlen(formatText);
	int protocolNameSize = strlen(protocolName);
	int userNameSize = strlen(userName);
	int targetPos = 0;
	int chatTextSize = 0;
	int chatNameSize = 0;
	int formatChatSize = 0;
	int size;
	char *ret = NULL;
	if (chatName != NULL)
	{
		chatNameSize = strlen(chatName);
		chatTextSize = strlen(chatText);
		formatChatSize = strlen(formatChat);
	}
	size = startTextSize + formatTextSize + protocolNameSize + userNameSize + chatNameSize + chatTextSize + formatChatSize + 1;
	ret = (char *)malloc(size);
	memcpy(ret + targetPos, startText, startTextSize);
	targetPos += startTextSize;
	memcpy(ret + targetPos, protocolName, protocolNameSize);
	targetPos += protocolNameSize;
	ret[targetPos] = ' ';
	targetPos++;
	ret[targetPos] = '(';
	targetPos++;
	memcpy(ret + targetPos, userName, userNameSize);
	targetPos += userNameSize;
	ret[targetPos] = ')';
	targetPos++;
	if (chatNameSize > 0)
	{
		ret[targetPos] = '\r';
		targetPos++;
		ret[targetPos] = '\n';
		targetPos++;
		memcpy(ret + targetPos, chatText, chatTextSize);
		targetPos += chatTextSize;
		memcpy(ret + targetPos, chatName, chatNameSize);
		targetPos += chatNameSize;
	}
	ret[targetPos] = '\0';
	return ret;
}

static void toast_clicked_cb(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	purple_debug_misc("win_toast_notifications", "toast clicked\n");
	if (conv != NULL) {
		pidgin_conv_attach_to_conversation(conv);
		gtkconv = PIDGIN_CONVERSATION(conv);
		pidgin_conv_switch_active_conversation(conv);
		pidgin_conv_window_switch_gtkconv(gtkconv->win, gtkconv);
		gtk_window_present(GTK_WINDOW(gtkconv->win->window));
	}
}

static gboolean get_effective_setting(PurpleStatusPrimitive status, Setting setting, Buddy_type buddy_type, const char *group_name, const char *protocol_id, const char *account_name, const char *buddy_name) {
	char *path;
	gboolean exists;
	gboolean value;

	purple_debug_misc("win_toast_notifications", "Checking effective settings\n");

	if (protocol_id != NULL && account_name != NULL && buddy_name != NULL) {
		// check buddy setting for status
		path = get_prefs_path(status, SETTING_ENABLED, buddy_type, NULL, protocol_id, account_name, buddy_name);
		exists = purple_prefs_exists(path);
		if (exists) {
			if (purple_prefs_get_bool(path)) {
				purple_debug_misc("win_toast_notifications", "Found enabled buddy setting for status: %s\n", path);
				free(path);
				path = get_prefs_path(status, setting, buddy_type, NULL, protocol_id, account_name, buddy_name);
				exists = purple_prefs_exists(path);
				if (exists) {
					value = purple_prefs_get_bool(path);
					purple_debug_misc("win_toast_notifications", "Found setting for status %i: %i, path: %s\n", setting, value, path);
					free(path);
					return value;
				}
			}
		}
		free(path);

		// check buddy setting for unset status
		path = get_prefs_path(PURPLE_STATUS_UNSET, SETTING_ENABLED, buddy_type, NULL, protocol_id, account_name, buddy_name);
		exists = purple_prefs_exists(path);
		if (exists) {
			if (purple_prefs_get_bool(path)) {
				purple_debug_misc("win_toast_notifications", "Found enabled buddy setting for unset status: %s\n", path);
				free(path);
				path = get_prefs_path(PURPLE_STATUS_UNSET, setting, buddy_type, NULL, protocol_id, account_name, buddy_name);
				exists = purple_prefs_exists(path);
				if (exists) {
					value = purple_prefs_get_bool(path);
					purple_debug_misc("win_toast_notifications", "Found setting for status %i: %i, path: %s\n", setting, value, path);
					free(path);
					return value;
				}
			}
		}
		free(path);
	}

	if (group_name != NULL) {
		// check group setting for status
		path = get_prefs_path(status, SETTING_ENABLED, BUDDY_TYPE_GROUP, group_name, NULL, NULL, NULL);
		exists = purple_prefs_exists(path);
		if (exists) {
			if (purple_prefs_get_bool(path)) {
				purple_debug_misc("win_toast_notifications", "Found enabled group setting for status: %s\n", path);
				free(path);
				path = get_prefs_path(status, setting, BUDDY_TYPE_GROUP, group_name, NULL, NULL, NULL);
				exists = purple_prefs_exists(path);
				if (exists) {
					value = purple_prefs_get_bool(path);
					purple_debug_misc("win_toast_notifications", "Found setting for status %i: %i, path: %s\n", setting, value, path);
					free(path);
					return value;
				}
			}
		}
		free(path);

		// check group setting for unset status
		path = get_prefs_path(PURPLE_STATUS_UNSET, SETTING_ENABLED, BUDDY_TYPE_GROUP, group_name, NULL, NULL, NULL);
		exists = purple_prefs_exists(path);
		if (exists) {
			if (purple_prefs_get_bool(path)) {
				purple_debug_misc("win_toast_notifications", "Found enabled group for unset status: %s\n", path);
				free(path);
				path = get_prefs_path(PURPLE_STATUS_UNSET, setting, BUDDY_TYPE_GROUP, group_name, NULL, NULL, NULL);
				exists = purple_prefs_exists(path);
				if (exists) {
					value = purple_prefs_get_bool(path);
					purple_debug_misc("win_toast_notifications", "Found setting for status %i: %i, path: %s\n", setting, value, path);
					free(path);
					return value;
				}
			}
		}
		free(path);
	}

	// check global setting for status
	path = get_prefs_path(status, SETTING_ENABLED, BUDDY_TYPE_GLOBAL, NULL, NULL, NULL, NULL);
	exists = purple_prefs_exists(path);
	if (exists) {
		if (purple_prefs_get_bool(path)) {
			purple_debug_misc("win_toast_notifications", "Found enabled global setting for status: %s\n", path);
			free(path);
			path = get_prefs_path(status, setting, BUDDY_TYPE_GLOBAL, NULL, NULL, NULL, NULL);
			exists = purple_prefs_exists(path);
			if (exists) {
				value = purple_prefs_get_bool(path);
				purple_debug_misc("win_toast_notifications", "Found setting for status %i: %i, path: %s\n", setting, value, path);
				free(path);
				return value;
			}
		}
	}
	free(path);

	// check global setting for unset status
	path = get_prefs_path(PURPLE_STATUS_UNSET, SETTING_ENABLED, BUDDY_TYPE_GLOBAL, NULL, NULL, NULL, NULL);
	exists = purple_prefs_exists(path);
	if (exists) {
		if (purple_prefs_get_bool(path)) {
			purple_debug_misc("win_toast_notifications", "Found enabled global setting for unset status: %s\n", path);
			free(path);
			path = get_prefs_path(PURPLE_STATUS_UNSET, setting, BUDDY_TYPE_GLOBAL, NULL, NULL, NULL, NULL);
			exists = purple_prefs_exists(path);
			if (exists) {
				value = purple_prefs_get_bool(path);
				purple_debug_misc("win_toast_notifications", "Found setting for status %i: %i, path: %s\n", setting, value, path);
				free(path);
				return value;
			}
		}
	}
	free(path);

	return FALSE;	
}

static gboolean should_show_message(PurpleAccount *account, PurpleConversation *conv, PurpleConversationType convType, PurpleMessageFlags flags, const char *sender) {
	PurpleStatus* purpleStatus = NULL;
	PurpleStatusType* statusType = NULL;
	PurpleStatusPrimitive primStatus = 0;
	const char* protocol_id = NULL;
	const char* account_name = NULL;
	PurpleBuddy* buddy = NULL;
	const char* chatName = NULL;
	PurpleChat* chat = NULL;
	PurpleGroup* group = NULL;
	const char* group_name = NULL;

	if (!(flags & PURPLE_MESSAGE_RECV)) {
		return FALSE;
	}
	if (flags & PURPLE_MESSAGE_SYSTEM) {
		return FALSE;
	}

	protocol_id = purple_account_get_protocol_id(account);
	account_name = purple_account_get_username(account);

	purpleStatus = purple_account_get_active_status(account);
	statusType = purple_status_get_type(purpleStatus);
	primStatus = purple_status_type_get_primitive(statusType);

	if (convType == PURPLE_CONV_TYPE_IM) {
		purple_debug_misc("win_toast_notifications", "Checking im settings\n");
		buddy = purple_find_buddy(account, sender);
		group = purple_buddy_get_group(buddy);
		group_name = purple_group_get_name(group);
		if (!get_effective_setting(primStatus, SETTING_FOR_IM, BUDDY_TYPE_BUDDY, group_name, protocol_id, account_name, sender)) {
			return FALSE;
		}
		if (conv != NULL && purple_conversation_has_focus(conv) && !get_effective_setting(primStatus, SETTING_FOR_FOCUS, BUDDY_TYPE_BUDDY, group_name, protocol_id, account_name, sender))
		{
			return FALSE;
		}
	} else if (convType == PURPLE_CONV_TYPE_CHAT) {
		if (conv != NULL) {
			chatName = purple_conversation_get_name(conv);
			chat = purple_blist_find_chat(account, chatName);
			group = purple_chat_get_group(chat);
			group_name = purple_group_get_name(group);
			purple_debug_misc("win_toast_notifications", "Checking chat settings\n");
			if (!get_effective_setting(primStatus, SETTING_FOR_CHAT, BUDDY_TYPE_CHAT, group_name, protocol_id, account_name, chatName)) {
				if (!(flags & PURPLE_MESSAGE_NICK && get_effective_setting(primStatus, SETTING_FOR_CHAT_MENTIONED, BUDDY_TYPE_CHAT, group_name, protocol_id, account_name, chatName))) {
					return FALSE;
				}
			}
			if (purple_conversation_has_focus(conv) && !get_effective_setting(primStatus, SETTING_FOR_FOCUS, BUDDY_TYPE_CHAT, group_name, protocol_id, account_name, chatName))
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

static void displayed_msg_cb(
	PurpleAccount *account,
	const char *sender,
	const char *buffer,
	PurpleConversation *conv,
	PurpleMessageFlags flags
) {
	const char *chatName = NULL;
	const char *protocolName = NULL;
	const char *userName = NULL;
	char *attrText = NULL;
	PurpleBuddy *buddy = NULL;
	int callResult;
	const char *senderName = NULL;
	PurpleBuddyIcon *icon = NULL;
	const char *iconPath = NULL;
	PurpleConversationType convType = purple_conversation_get_type(conv);

	if (should_show_message(account, conv, convType, flags, sender))
	{
		buddy = purple_find_buddy(account, sender);
		if (buddy != NULL)
		{
			purple_debug_misc("win_toast_notifications", "Received a message from a buddy\n");
			senderName = purple_buddy_get_alias(buddy);
			if (senderName == NULL)
			{
				senderName = purple_buddy_get_name(buddy);
				if (senderName == NULL)
				{
					senderName = sender;
				}
			}
			icon = purple_buddy_get_icon(buddy);
			if (icon != NULL)
			{
				iconPath = purple_buddy_icon_get_full_path(icon);
			}
		}
		else
		{
			purple_debug_misc("win_toast_notifications", "Received a message from someone who is not a buddy\n");
			senderName = sender;
		}

		if (conv != NULL && convType == PURPLE_CONV_TYPE_CHAT)
		{
			chatName = purple_conversation_get_title(conv);
		}

		userName = purple_account_get_username(account);
		protocolName = purple_account_get_protocol_name(account);
		attrText = get_attr_text(protocolName, userName, chatName);

		purple_debug_misc("win_toast_notifications", "displayed_msg_cb (%s, %s, %s, %s, %s, %d)\n",
							protocolName, userName, sender, buffer,
							chatName, flags);
		callResult = (showToastProcAdd)(senderName, buffer, iconPath, attrText, conv);
		if (callResult)
		{
			output_toast_error(callResult, "Failed to show Toast Notification");
		}
		else
		{
			purple_debug_misc("win_toast_notifications", "Showed Toast Notification\n");
		}
		free(attrText);
	}
}

static void account_signed_on(
	PurpleAccount *account
) {
	ConnectionNode **tree_node = NULL;
	ConnectionNode *node = malloc(sizeof(ConnectionNode));
	if (node == NULL) {
		return;
	}
	node->account = account;
	node->connect_time = time(NULL);
	if (node->connect_time == (time_t) -1) {
		free(node);
		return;
	}
	tree_node = (ConnectionNode **)tsearch((void*)node, (void*)&connection_root, compare_connection_nodes);
	if (tree_node == NULL) {
		free(node);
		return;
	}
	if (*tree_node != node) {
		(*tree_node)->connect_time = node->connect_time;
		free(node);
		return;
	}
	num_connection_nodes++;
}

static void buddy_sign_cb(
	PurpleBuddy *buddy,
	BOOL online
) {
	PurpleStatus * purpleStatus = NULL;
	PurpleStatusType * statusType = NULL;
	PurpleStatusPrimitive primStatus = 0;
	const char * protocol_id = NULL;
	const char * account_name = NULL;
	PurpleGroup * group = NULL;
	const char * group_name = NULL;
	const char *protocolName = NULL;
	char *attrText = NULL;
	int callResult;
	const char *senderName = NULL;
	PurpleBuddyIcon *icon = NULL;
	const char *iconPath = NULL;
	PurpleAccount * account = NULL;
	char * message = NULL;
	Setting setting;
	ConnectionNode *node = NULL;
	ConnectionNode **tree_node = NULL;
	time_t current_time;
	int notificationDelayAfterSignOnInS = 5;

	if (online) {
		purple_debug_misc("win_toast_notifications", "Buddy signed on\n");
		setting = SETTING_BUDDY_SIGNED_ON;
		message = "Signed on";
	} else {
		purple_debug_misc("win_toast_notifications", "Buddy signed off\n");
		setting = SETTING_BUDDY_SIGNED_OFF;
		message = "Signed off";
	}

	account = purple_buddy_get_account(buddy);

	node = malloc(sizeof(ConnectionNode));
	if (node == NULL) {
		return;
	}
	node->account = account;
	if (node->connect_time == (time_t) -1) {
		free(node);
		return;
	}
	tree_node = (ConnectionNode **)tfind((void*)node, (void*)&connection_root, compare_connection_nodes);
	free(node);
	if (tree_node == NULL) {
		return;
	}
	current_time = time(NULL);
	if (((current_time - (*tree_node)->connect_time)) < notificationDelayAfterSignOnInS) {
		// do not show sign on notifications of a buddy in the first seconds after signed on
		// because this can produce many notifications
		// todo: make this customizable by the user?
		return;
	}


	protocol_id = purple_account_get_protocol_id(account);
	account_name = purple_account_get_username(account);
	purpleStatus = purple_account_get_active_status(account);
	statusType = purple_status_get_type(purpleStatus);
	primStatus = purple_status_type_get_primitive(statusType);
	group = purple_buddy_get_group(buddy);
	group_name = purple_group_get_name(group);

	if (get_effective_setting(primStatus, setting, BUDDY_TYPE_BUDDY, group_name, protocol_id, account_name, buddy->name))
	{
		purple_debug_misc("win_toast_notifications", "Notification should be shown\n");
		senderName = purple_buddy_get_alias(buddy);
		if (senderName == NULL)
		{
			senderName = purple_buddy_get_name(buddy);
		}
		icon = purple_buddy_get_icon(buddy);
		if (icon != NULL)
		{
			iconPath = purple_buddy_icon_get_full_path(icon);
		}

		protocolName = purple_account_get_protocol_name(account);
		attrText = get_attr_text(protocolName, account_name, NULL);

		// todo: handover a reference to the buddy to be able to create a conversation on click on the notification
		callResult = (showToastProcAdd)(senderName, message, iconPath, attrText, NULL);
		if (callResult)
		{
			output_toast_error(callResult, "Failed to show Toast Notification");
		}
		else
		{
			purple_debug_misc("win_toast_notifications", "Showed Toast Notification\n");
		}
		free(attrText);
	}
}

static void button_clicked_cb(GtkButton *button, const char *path)
{
    gboolean value = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
	purple_prefs_set_bool(path, value);
}

static void settings_dialog_destroy_cb(GtkWidget *w, struct localSettingsData *data)
{
	struct charListNode *node = 0;
	struct charListNode *lastNode = 0;
	node = data->paths;
	while (node != 0) {
		free(node->str);
		lastNode = node;
		node = node->next;
		free(lastNode);
	}
	free(data);
}

static void local_settings_dialog_response_cb(GtkWidget *dialog, gint resp, struct localSettingsData *data)
{
    gtk_widget_destroy(dialog);
}

static void add_setting_button(
	PurpleStatusPrimitive status,
	Buddy_type buddy_type,
	const char *group_name,
	const char *protocol_id,
	const char *account_name,
	const char *buddy_name,
	GtkWidget *vbox,
	struct localSettingsData *data,
	Setting setting,
	const char *labelText
) {
	GtkWidget * button;
	char *path;
	struct charListNode *node;
	gboolean exists;
	gboolean value;

	button = gtk_check_button_new_with_label(labelText);
	gtk_container_add(GTK_CONTAINER(vbox), button);
	path = get_prefs_path(status, setting, buddy_type, group_name, protocol_id, account_name, buddy_name);
	node = malloc(sizeof(struct charListNode));
	node->next = data->paths;
	data->paths = node;
	node->str = path;
	ensure_prefs_path(path);
	exists = purple_prefs_exists(path);
	if (!exists) {
		purple_prefs_add_bool(path, FALSE);
	}
	value = purple_prefs_get_bool(path);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_clicked_cb), path);
}

static void add_setting_buttons(
	PurpleStatusPrimitive status,
	Buddy_type buddy_type,
	const char *group_name,
	const char *protocol_id,
	const char *account_name,
	const char *buddy_name,
	GtkWidget *vbox,
	struct localSettingsData *data,
	const char *labelText
) {
	GtkWidget *label;
	char *markup;
	gchar *label_markup = "<span weight=\"bold\">%s</span>";

	label = gtk_label_new(NULL);
	markup = g_markup_printf_escaped(label_markup, labelText);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	add_setting_button(
		status,
		buddy_type,
		group_name,
		protocol_id,
		account_name,
		buddy_name,
		vbox,
		data,
		SETTING_ENABLED,
		"Enabled"
	);
	if (buddy_type != BUDDY_TYPE_CHAT) {
		add_setting_button(
			status,
			buddy_type,
			group_name,
			protocol_id,
			account_name,
			buddy_name,
			vbox,
			data,
			SETTING_BUDDY_SIGNED_ON,
			"Buddy signed on"
		);
		add_setting_button(
			status,
			buddy_type,
			group_name,
			protocol_id,
			account_name,
			buddy_name,
			vbox,
			data,
			SETTING_BUDDY_SIGNED_OFF,
			"Buddy signed off"
		);
		add_setting_button(
			status,
			buddy_type,
			group_name,
			protocol_id,
			account_name,
			buddy_name,
			vbox,
			data,
			SETTING_FOR_IM,
			"Direct messages"
		);
	}
	if (buddy_type != BUDDY_TYPE_BUDDY) {
		add_setting_button(
			status,
			buddy_type,
			group_name,
			protocol_id,
			account_name,
			buddy_name,
			vbox,
			data,
			SETTING_FOR_CHAT,
			"Every message in chats"
		);
		add_setting_button(
			status,
			buddy_type,
			group_name,
			protocol_id,
			account_name,
			buddy_name,
			vbox,
			data,
			SETTING_FOR_CHAT_MENTIONED,
			"Messages in chats when mentioned"
		);
	}
	add_setting_button(
		status,
		buddy_type,
		group_name,
		protocol_id,
		account_name,
		buddy_name,
		vbox,
		data,
		SETTING_FOR_FOCUS,
		"Even if the window is focused"
	);
}

static void add_setting_groups(
	Buddy_type buddy_type,
	const char *group_name,
	const char *protocol_id,
	const char *account_name,
	const char *buddy_name,
	GtkWidget *vbox,
	struct localSettingsData *data
) {
	add_setting_buttons(
		PURPLE_STATUS_UNSET,
		buddy_type,
		group_name,
		protocol_id,
		account_name,
		buddy_name,
		vbox,
		data,
		"In every status notify for"
	);
	add_setting_buttons(
		PURPLE_STATUS_AVAILABLE,
		buddy_type,
		group_name,
		protocol_id,
		account_name,
		buddy_name,
		vbox,
		data,
		"If in status 'Available' instead notify for"
	);
	add_setting_buttons(
		PURPLE_STATUS_AWAY,
		buddy_type,
		group_name,
		protocol_id,
		account_name,
		buddy_name,
		vbox,
		data,
		"If in status 'Away' instead notify for"
	);
	add_setting_buttons(
		PURPLE_STATUS_UNAVAILABLE,
		buddy_type,
		group_name,
		protocol_id,
		account_name,
		buddy_name,
		vbox,
		data,
		"If in status 'Do not disturb' instead notify for"
	);
	add_setting_buttons(
		PURPLE_STATUS_INVISIBLE,
		buddy_type,
		group_name,
		protocol_id,
		account_name,
		buddy_name,
		vbox,
		data,
		"If in status 'Invisible' instead notify for"
	);
	add_setting_buttons(
		PURPLE_STATUS_EXTENDED_AWAY,
		buddy_type,
		group_name,
		protocol_id,
		account_name,
		buddy_name,
		vbox,
		data,
		"If in status 'Extended away' instead notify for"
	);
}

static void show_local_settings_dialog(PurpleBlistNode *node, gpointer plugin)
{
	PurplePlugin *prpl = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;
	struct localSettingsData *data = NULL;
	GtkWidget *dialog = NULL;
	GtkWidget *scrolled_window = NULL;
	GtkWidget *vbox = NULL;
	GtkWidget *label = NULL;
	Buddy_type buddy_type;
	PurpleGroup * groupNode = NULL;
	PurpleBuddy * buddyNode = NULL;
	PurpleChat * chatNode = NULL;
	const char *chatName = NULL;
	const char *account_name = NULL;
	const char *group_name = NULL;
	const char *buddy_name = NULL;
	const char *protocol_id = NULL;

	if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		groupNode = (PurpleGroup*) node;
		buddy_type = BUDDY_TYPE_GROUP;
		group_name = purple_group_get_name(groupNode);
		label = gtk_label_new("Overwrite global settings for this group");
		purple_debug_misc("win_toast_notifications", "Open local settings dialog for group: %s\n", group_name);
	} else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		buddyNode = (PurpleBuddy*) node;
		buddy_type = BUDDY_TYPE_BUDDY;
		account_name = buddyNode->account->username;
		buddy_name = buddyNode->name;
		protocol_id = buddyNode->account->protocol_id;
		label = gtk_label_new("Overwrite global and group settings for this buddy");
		purple_debug_misc("win_toast_notifications", "Open local settings dialog for buddy: %s, protocol: %s, username: %s\n", buddy_name, protocol_id, account_name);
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		chatNode = (PurpleChat*) node;
		prpl = purple_find_prpl(purple_account_get_protocol_id(chatNode->account));
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
		if (prpl_info->chat_info) {
			struct proto_chat_entry *pce;
			GList *parts = prpl_info->chat_info(purple_account_get_connection(chatNode->account));
			pce = parts->data;
			chatName = g_hash_table_lookup(chatNode->components, pce->identifier);
			g_list_foreach(parts, (GFunc)g_free, NULL);
			g_list_free(parts);
			buddy_type = BUDDY_TYPE_CHAT;
			account_name = chatNode->account->username;
			buddy_name = chatName;
			protocol_id = chatNode->account->protocol_id;
			purple_debug_misc("win_toast_notifications", "Open local settings dialog for chat: %s, protocol: %s, username: %s\n", chatName, protocol_id, account_name);
		} else {
			purple_debug_error("win_toast_notifications", "Cannot open local settings dialog for chat because it has no chat_info");
			return;
		}
		label = gtk_label_new("Overwrite global and group settings for this chat");
	} else {
		return;
	}

	data = malloc(sizeof(struct localSettingsData));
	data->paths = 0;

	dialog = gtk_dialog_new_with_buttons(
		"Local Windows Toast Notifications Settings",
	    NULL,
		0,
	    GTK_STOCK_CLOSE,
		GTK_RESPONSE_CLOSE,
	    NULL);
	gtk_widget_set_size_request(dialog, 400, 400);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
    gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), 0);
    gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 0);

	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(scrolled_window),
        GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC
	);				
    gtk_box_pack_start(
		GTK_BOX(GTK_DIALOG(dialog)->vbox),
		scrolled_window, 
		TRUE,
		TRUE,
		0
	);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), vbox);

	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	add_setting_groups(
		buddy_type,
		group_name,
		protocol_id,
		account_name,
		buddy_name,
		vbox,
		data
	);
	
	g_signal_connect(
		G_OBJECT(dialog),
		"destroy",
	    G_CALLBACK(settings_dialog_destroy_cb),
		data
	);
    g_signal_connect(
		G_OBJECT(dialog),
		"response",
	    G_CALLBACK(local_settings_dialog_response_cb),
		data
	);

	gtk_widget_show_all(dialog);
}

static void
context_menu(PurpleBlistNode *node, GList **menu, gpointer plugin)
{
	PurpleMenuAction *action;

	if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		action = purple_menu_action_new("Buddy Windows Toast Notifications Settings",
						PURPLE_CALLBACK(show_local_settings_dialog), plugin, NULL);
		(*menu) = g_list_prepend(*menu, action);
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		action = purple_menu_action_new("Chat Windows Toast Notifications Settings",
						PURPLE_CALLBACK(show_local_settings_dialog), plugin, NULL);
		(*menu) = g_list_prepend(*menu, action);
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		action = purple_menu_action_new("Group Windows Toast Notifications Settings",
						PURPLE_CALLBACK(show_local_settings_dialog), plugin, NULL);
		(*menu) = g_list_prepend(*menu, action);
	}
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_debug_misc("win_toast_notifications", "loading...\n");

	set_default_prefs();

	hinstLib = LoadLibrary(TEXT("PidginWinToastLib.dll"));

	if (hinstLib != NULL)
	{
		purple_debug_misc("win_toast_notifications",
						  "dll loaded\n");

		initAdd = (initProc)GetProcAddress(hinstLib, "pidginWinToastLibInit");
		showToastProcAdd = (showToastProc)GetProcAddress(hinstLib, "pidginWinToastLibShowMessage");

		if (initAdd == NULL)
		{
			purple_debug_misc("win_toast_notifications", "pidginWinToastLibInit not found!\n");
		}
		else if (showToastProcAdd == NULL)
		{
			purple_debug_misc("win_toast_notifications", "pidginWinToastLibShowMessage not found!\n");
		}
		else
		{
			int callResult;
			void *conv_handle;
			void *blist_handle;
			void *account_handle;
			purple_debug_misc("win_toast_notifications",
							  "pidginWinToastLibInit called\n");
			callResult = (initAdd)((void *)toast_clicked_cb);
			if (callResult)
			{
				output_toast_error(callResult, "Initialization failed");
			}
			else
			{
				GList* accounts = NULL;
				purple_debug_misc("win_toast_notifications",
								  "pidginWinToastLibInit initialized\n");

				num_connection_nodes = 0;
				connection_root = NULL;
				accounts = purple_accounts_get_all();
				while (accounts != NULL) {
					ConnectionNode **tree_node = NULL;
					ConnectionNode *node = malloc(sizeof(ConnectionNode));
					if (node == NULL) {
						plugin_unload(plugin);
						return FALSE;
					}
					node->account = accounts->data;
					node->connect_time = time(NULL);
					if (node->connect_time == (time_t) -1) {
						free(node);
						plugin_unload(plugin);
						return FALSE;
					}
					tree_node = (ConnectionNode **)tsearch((void*)node, (void*)&connection_root, compare_connection_nodes);
					if (tree_node == NULL) {
						free(node);
						plugin_unload(plugin);
						return FALSE;
					}
					num_connection_nodes++;
					
					accounts = accounts->next;
				}

				conv_handle = pidgin_conversations_get_handle();
				blist_handle = purple_blist_get_handle();
				account_handle = purple_accounts_get_handle();
				purple_signal_connect(conv_handle, "displayed-im-msg",
									  plugin, PURPLE_CALLBACK(displayed_msg_cb), NULL);
				purple_signal_connect(conv_handle, "displayed-chat-msg",
									  plugin, PURPLE_CALLBACK(displayed_msg_cb), NULL);
				purple_signal_connect(blist_handle, "buddy-signed-on",
									  plugin, PURPLE_CALLBACK(buddy_sign_cb), (void*)TRUE);
				purple_signal_connect(blist_handle, "buddy-signed-off",
									  plugin, PURPLE_CALLBACK(buddy_sign_cb), (void*)FALSE);
				purple_signal_connect(blist_handle, "blist-node-extended-menu",
									  plugin, PURPLE_CALLBACK(context_menu), plugin);
				purple_signal_connect(account_handle, "account-signed-on",
									  plugin, PURPLE_CALLBACK(account_signed_on), NULL);

				return TRUE;
			}
		}
	}
	else
	{
		purple_debug_misc("win_toast_notifications",
						  "failed to load dll\n");
	}

	return FALSE;
}

void add_connection_node_to_delete_array (const void *nodep, VISIT value, int level) {
	ConnectionNode * node = *((ConnectionNode **)nodep);
	connection_nodes_to_delete[current_connection_node_id] = node;
	current_connection_node_id++;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	int i;
	connection_nodes_to_delete = malloc(sizeof(ConnectionNode *) * num_connection_nodes);
	twalk((void*)connection_root, add_connection_node_to_delete_array);

	for (i = 0; i < num_connection_nodes; i++) {
		ConnectionNode * node = connection_nodes_to_delete[i];
		tdelete((void*)node, (void*)&connection_root, compare_connection_nodes);
		free(node);
	}

	purple_signals_disconnect_by_handle(plugin);

	if (hinstLib != NULL)
	{
		FreeLibrary(hinstLib);
	}

	return TRUE;
}

static void ensure_pref_default(PurpleStatusPrimitive status, Setting setting, gboolean value) {
	char *path;
	char *oldPath;
	gboolean exists;

	path = get_prefs_path(status, setting, BUDDY_TYPE_GLOBAL, NULL, NULL, NULL, NULL);
	ensure_prefs_path(path);	
	exists = purple_prefs_exists(path);
	if (!exists) {
		purple_prefs_add_bool(path, value);
	}

	oldPath = get_old_prefs_path(status, setting);
	exists = purple_prefs_exists(oldPath);
	if (exists) {
		value = purple_prefs_get_bool(oldPath);
		purple_prefs_set_bool(path, value);
		purple_prefs_remove(oldPath);
	}

	free(path);
	free(oldPath);
}

static void set_default_prefs() {
	char *oldPath;
	gboolean exists;

	ensure_pref_default(PURPLE_STATUS_UNSET, SETTING_ENABLED, TRUE);
	ensure_pref_default(PURPLE_STATUS_UNSET, SETTING_FOR_IM, TRUE);
	ensure_pref_default(PURPLE_STATUS_UNSET, SETTING_FOR_CHAT, TRUE);
	ensure_pref_default(PURPLE_STATUS_UNSET, SETTING_FOR_CHAT_MENTIONED, TRUE);
	ensure_pref_default(PURPLE_STATUS_UNSET, SETTING_FOR_FOCUS, FALSE);
	
	ensure_pref_default(PURPLE_STATUS_AVAILABLE, SETTING_ENABLED, FALSE);
	ensure_pref_default(PURPLE_STATUS_AVAILABLE, SETTING_FOR_IM, TRUE);
	ensure_pref_default(PURPLE_STATUS_AVAILABLE, SETTING_FOR_CHAT, TRUE);
	ensure_pref_default(PURPLE_STATUS_AVAILABLE, SETTING_FOR_CHAT_MENTIONED, TRUE);
	ensure_pref_default(PURPLE_STATUS_AVAILABLE, SETTING_FOR_FOCUS, FALSE);
	oldPath = get_old_prefs_path(PURPLE_STATUS_AVAILABLE, SETTING_NONE);
	exists = purple_prefs_exists(oldPath);
	if (exists) {
		purple_prefs_remove(oldPath);
	}
	free(oldPath);
	
	ensure_pref_default(PURPLE_STATUS_AWAY, SETTING_ENABLED, FALSE);
	ensure_pref_default(PURPLE_STATUS_AWAY, SETTING_FOR_IM, TRUE);
	ensure_pref_default(PURPLE_STATUS_AWAY, SETTING_FOR_CHAT, FALSE);
	ensure_pref_default(PURPLE_STATUS_AWAY, SETTING_FOR_CHAT_MENTIONED, TRUE);
	ensure_pref_default(PURPLE_STATUS_AWAY, SETTING_FOR_FOCUS, FALSE);
	oldPath = get_old_prefs_path(PURPLE_STATUS_AWAY, SETTING_NONE);
	exists = purple_prefs_exists(oldPath);
	if (exists) {
		purple_prefs_remove(oldPath);
	}
	free(oldPath);
	
	ensure_pref_default(PURPLE_STATUS_UNAVAILABLE, SETTING_ENABLED, TRUE);
	ensure_pref_default(PURPLE_STATUS_UNAVAILABLE, SETTING_FOR_IM, FALSE);
	ensure_pref_default(PURPLE_STATUS_UNAVAILABLE, SETTING_FOR_CHAT, FALSE);
	ensure_pref_default(PURPLE_STATUS_UNAVAILABLE, SETTING_FOR_CHAT_MENTIONED, FALSE);
	ensure_pref_default(PURPLE_STATUS_UNAVAILABLE, SETTING_FOR_FOCUS, FALSE);
	oldPath = get_old_prefs_path(PURPLE_STATUS_UNAVAILABLE, SETTING_NONE);
	exists = purple_prefs_exists(oldPath);
	if (exists) {
		purple_prefs_remove(oldPath);
	}
	free(oldPath);
	
	ensure_pref_default(PURPLE_STATUS_INVISIBLE, SETTING_ENABLED, FALSE);
	ensure_pref_default(PURPLE_STATUS_INVISIBLE, SETTING_FOR_IM, TRUE);
	ensure_pref_default(PURPLE_STATUS_INVISIBLE, SETTING_FOR_CHAT, FALSE);
	ensure_pref_default(PURPLE_STATUS_INVISIBLE, SETTING_FOR_CHAT_MENTIONED, TRUE);
	ensure_pref_default(PURPLE_STATUS_INVISIBLE, SETTING_FOR_FOCUS, FALSE);
	oldPath = get_old_prefs_path(PURPLE_STATUS_INVISIBLE, SETTING_NONE);
	exists = purple_prefs_exists(oldPath);
	if (exists) {
		purple_prefs_remove(oldPath);
	}
	free(oldPath);
	
	ensure_pref_default(PURPLE_STATUS_EXTENDED_AWAY, SETTING_ENABLED, FALSE);
	ensure_pref_default(PURPLE_STATUS_EXTENDED_AWAY, SETTING_FOR_IM, TRUE);
	ensure_pref_default(PURPLE_STATUS_EXTENDED_AWAY, SETTING_FOR_CHAT, FALSE);
	ensure_pref_default(PURPLE_STATUS_EXTENDED_AWAY, SETTING_FOR_CHAT_MENTIONED, TRUE);
	ensure_pref_default(PURPLE_STATUS_EXTENDED_AWAY, SETTING_FOR_FOCUS, FALSE);
	oldPath = get_old_prefs_path(PURPLE_STATUS_EXTENDED_AWAY, SETTING_NONE);
	exists = purple_prefs_exists(oldPath);
	if (exists) {
		purple_prefs_remove(oldPath);
	}
	free(oldPath);
}

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(win_toast_notifications, init_plugin, info);
