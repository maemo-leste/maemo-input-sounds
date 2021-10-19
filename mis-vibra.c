#include "maemo-input-sounds.h"

void mis_request_state(void *data, int enabled) {
	struct private_data *priv = (void *)data;
	DBusMessage *msg;

	const char *pattern = "PatternTouchscreen";

	if (!priv) {
		LOG_ERROR("priv == NULL");
		return;
	}
	if (!priv->dbus_system) {
		LOG_ERROR("priv->dbus_system == NULL");
		return;
	}

	const char *method_name = MCE_ACTIVATE_VIBRATOR_PATTERN;
	if (!enabled) {
		method_name = MCE_DEACTIVATE_VIBRATOR_PATTERN;
	}

	msg =
	    dbus_message_new_method_call(MCE_SERVICE, MCE_REQUEST_PATH,
					 MCE_REQUEST_IF, method_name);
	if (!msg) {
		LOG_ERROR("unable to create dbus message");
		return;
	}

	dbus_message_append_args(msg, DBUS_TYPE_STRING, &pattern,
				 DBUS_TYPE_INVALID);
	dbus_connection_send(priv->dbus_system, msg, NULL);
	dbus_connection_flush(priv->dbus_system);
	dbus_message_unref(msg);

	LOG_VERBOSE1("request_enabled: %d", enabled);
}

void mis_vibra_set_state(void *data, int state) {
	struct private_data *priv = (void *)data;
	if (!priv) {
		LOG_ERROR("priv == NULL)");
	}
	if (priv->touch_vibration_enabled) {
		mis_request_state(priv, state);
	}
}
