#include "maemo-input-sounds.h"

void mis_mce_init(struct private_data *priv) {
	DBusError error;

	dbus_error_init(&error);
	priv->dbus_system = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	if (!priv->dbus_system) {
		if (dbus_error_is_set(&error)) {
			dbus_error_free(&error);
			LOG_ERROR("Failed to get DBUS connection\n");
		}
		return;
	}

	dbus_connection_setup_with_g_main(priv->dbus_system, NULL);
	dbus_connection_add_filter(priv->dbus_system, mis_dbus_mce_filter, priv,
				   NULL);
	dbus_bus_add_match(priv->dbus_system,
			   "type='signal',path='/com/nokia/mce/signal',interface='com.nokia.mce.signal',member='tklock_mode_ind'",
			   &error);
	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		LOG_ERROR("failed to add match for tklock");
		return;
	}

	return;

}

void mis_mce_exit(struct private_data *priv) {
	(void)priv;
}

DBusHandlerResult mis_dbus_mce_filter(DBusConnection * conn,
				      DBusMessage * msg, void *data) {
	(void)conn;
	(void)msg;
	(void)data;
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
