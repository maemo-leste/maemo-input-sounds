#include "maemo-input-sounds.h"

void mis_vibra_init(struct private_data *priv) {
	(void)priv;

	if (!priv) {
		LOG_ERROR("no priv");
		return;
	}

	priv->touch_vibration_enabled = 1;

	if (priv->gconf_client) {
		g_object_ref(priv->gconf_client);
	} else {
		priv->gconf_client = gconf_client_get_default();
		if (!priv->gconf_client) {
			LOG_ERROR("could not allocate gconf client");
			return;
		}
	}

	gconf_client_add_dir(priv->gconf_client, GCONF_SYSTEM_OSSO_DSM_VIBRA,
			     GCONF_CLIENT_PRELOAD_NONE, NULL);
	gconf_client_notify_add(priv->gconf_client,
				GCONF_SYSTEM_OSSO_DSM_VIBRA_TS_ENABLED,
				vibration_changed_notifier, (gpointer) priv,
				NULL, NULL);
	priv->touch_vibration_enabled =
	    gconf_client_get_bool(priv->gconf_client,
				  GCONF_SYSTEM_OSSO_DSM_VIBRA_TS_ENABLED, NULL);
	if (verbose) {
		LOG_VERBOSE1("vibra initialised and %s",
			     priv->touch_vibration_enabled ? "enabled" :
			     "disabled");
	}

}

void mis_vibra_exit(struct private_data *priv) {
	if (priv) {
		if (priv->gconf_client)
			g_object_unref(priv->gconf_client);
		priv->gconf_client = NULL;
	}
}

void vibration_changed_notifier(GConfClient * client, guint cnxn_id,
				GConfEntry * entry, gpointer user_data) {
	(void)client;
	(void)cnxn_id;

	struct private_data *priv = (void *)user_data;
	GConfValue *gconf_val;
	const char *gconf_key;

	if (!priv) {
		LOG_ERROR("no priv");
		return;
	}

	gconf_val = gconf_entry_get_value(entry);
	if (gconf_val) {
		gconf_key = gconf_entry_get_key(entry);
		if (g_str_equal
		    (gconf_key, GCONF_SYSTEM_OSSO_DSM_VIBRA_TS_ENABLED)) {
			if (gconf_val->type == GCONF_VALUE_BOOL) {
				priv->touch_vibration_enabled =
				    gconf_value_get_bool(gconf_val);
				if (!priv->touch_vibration_enabled)
					mis_request_state(priv, 0);
			} else {
				LOG_ERROR
				    ("gconf_val->type != GCONF_VALUE_BOOL");
			}
		}
	} else {
		LOG_ERROR("gconf_val is NULL");
	}
}

void mis_request_state(void *data, int state) {
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

	const char *method_name = "req_vibrator_pattern_activate";
	if (!state) {
		method_name = "req_vibrator_pattern_deactivate";
	}

	msg =
	    dbus_message_new_method_call("com.nokia.mce",
					 "/com/nokia/mce/request",
					 "com.nokia.mce.request", method_name);
	if (!msg) {
		LOG_ERROR("unable to create dbus message");
		return;
	}

	dbus_message_append_args(msg, DBUS_TYPE_STRING, &pattern,
				 DBUS_TYPE_INVALID);
	dbus_connection_send(priv->dbus_system, msg, NULL);
	dbus_connection_flush(priv->dbus_system);
	dbus_message_unref(msg);

	LOG_VERBOSE1("request_state: %d", state);
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
