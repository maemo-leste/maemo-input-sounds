#include "maemo-input-sounds.h"

void mis_mce_init(struct private_data *priv) {
	DBusError error;

	dbus_error_init(&error);
	priv->dbus_system = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	if (!priv->dbus_system) {
		if (dbus_error_is_set(&error)) {
			LOG_ERROR1("Failed to get DBUS connection: %s",
				   error.message);
			dbus_error_free(&error);
		} else
			LOG_ERROR("Failed to get DBUS connection");
		goto done;
	}

	dbus_connection_setup_with_g_main(priv->dbus_system, NULL);
	dbus_connection_add_filter(priv->dbus_system, mis_dbus_mce_filter, priv,
				   NULL);
	dbus_bus_add_match(priv->dbus_system,
			   "type='signal',path='" MCE_SIGNAL_PATH
			   "',interface='" MCE_SIGNAL_IF "',member='"
			   MCE_TKLOCK_MODE_SIG "'", &error);
	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		LOG_ERROR("failed to add match for tklock");
		return;
	}

 done:
	priv->device_state &= MODE_TKLOCK_LOCKED_MASK;
	return;

}

void mis_mce_exit(struct private_data *priv) {
	(void)priv;
}

DBusHandlerResult mis_dbus_mce_filter(DBusConnection * conn,
				      DBusMessage * msg, void *data) {
	struct private_data *priv = data;
	DBusMessageIter iter;
	const char *value = NULL;

	if (!conn) {
		LOG_ERROR("conn == NULL");
		goto done;
	}
	if (!msg) {
		LOG_ERROR("msg == NULL");
		goto done;
	}
	if (!priv) {
		LOG_ERROR("priv == NULL");
		goto done;
	}

	if (dbus_message_is_signal(msg, MCE_SIGNAL_IF, MCE_TKLOCK_MODE_SIG)) {
		value = NULL;
		dbus_message_iter_init(msg, &iter);
		if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING)
		{
			dbus_message_iter_get_basic(&iter, &value);
			LOG_VERBOSE1("mce says tklock %s", value);
			dbus_message_iter_get_basic(&iter, &value);

			if (g_str_equal(value, MCE_TK_UNLOCKED))
				priv->device_state &= MODE_TKLOCK_LOCKED_MASK;
			else
				priv->device_state |= MODE_TKLOCK_UNLOCKED;

		} else {
			LOG_VERBOSE
			    ("mce send a non-string tklock_mode_ind signal");
		}
	}

 done:
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
