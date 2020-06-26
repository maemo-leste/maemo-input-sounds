#include "maemo-input-sounds.h"

static struct private_data *static_priv = NULL;
int verbose = 1;		// XXX

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

DBusHandlerResult mis_dbus_mce_filter(DBusConnection * conn, DBusMessage * msg,
				      void *data) {
	(void)conn;
	(void)msg;
	(void)data;
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void mis_request_state(void *data, int state) {
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

int xerror_handler(Display * display, XErrorEvent * ev) {
	(void)display;
	(void)ev;

	LOG_ERROR("X11 error_handler fired");

	return 0;
}

void xrec_data_cb(XPointer data, XRecordInterceptData * recdat) {
	struct private_data *priv = (void *)data;
	int diff_ms;
	struct timespec ts;

	unsigned char *xrd = recdat->data;

	if (!priv) {
		LOG_ERROR("priv == NULL");
		return;
	}

	if (!xrd) {
		LOG_ERROR("xrd == NULL");
		return;
	}

	if (recdat->category || !priv->recordcontext) {
		goto done;
	}

	clock_gettime(CLOCK_MONOTONIC, &ts);

	diff_ms =
	    (1000 * (ts.tv_sec - priv->last_event_ts.tv_sec)) + (ts.tv_nsec -
								 priv->last_event_ts.tv_nsec)
	    / 1000000;
	if (diff_ms < 33) {
		goto done;
	}

	priv->last_event_ts = ts;

	if (xrd[0] == ButtonPress && verbose) {
		LOG_VERBOSE1("X ButtonPress %d\n", xrd[1]);
	}
	if (xrd[0] == KeyPress && verbose) {
		LOG_VERBOSE1("X KeyPress %d\n", xrd[1]);
	}
	// if sounds enabled, or this is not a button, play sounds
	//if (priv->policy_state & 0xFFFFFF8) || !is_button)

	if (priv->touch_vibration_enabled && xrd[0] == ButtonPress) {
		mis_vibra_set_state(data, 1);
	}

 done:
	XRecordFreeData(recdat);
	return;
}

void *xrec_thread(void *data) {
	struct private_data *priv = data;
	int major, minor;
	XRecordRange *ranges[2];
	XRecordClientSpec spec;

	priv->display_thread = XOpenDisplay(NULL);
	if (!priv->display_thread) {
		// TODO fprintf error with macro
		fprintf(stderr, "xrec_thread failed to open display\n");
		exit(EXIT_FAILURE);
	}

	XSetErrorHandler(xerror_handler);
	if (!XRecordQueryVersion(priv->display_thread, &major, &minor)) {
		LOG_ERROR("X Record Extension now available.");
		exit(1);
	}

	LOG_VERBOSE1("X Record %d.%d is available\n", major, minor);

	ranges[0] = XRecordAllocRange();
	ranges[1] = XRecordAllocRange();

	if (!ranges[0] || !ranges[1]) {
		LOG_ERROR("failed to allocate X Record Range");
	}

	ranges[0]->device_events.first = KeyPress;
	ranges[0]->device_events.last = KeyPress;
	ranges[1]->device_events.first = ButtonPress;
	ranges[1]->device_events.last = ButtonPress;
	spec = XRecordAllClients;

	priv->recordcontext =
	    XRecordCreateContext(priv->display_thread, 0, &spec, 1, ranges, 2);
	if (!priv->recordcontext) {
		LOG_ERROR("failed to create X Record Context");
		exit(1);
	}

	if (!XRecordEnableContext
	    (priv->display_thread, priv->recordcontext, xrec_data_cb, data)) {
		LOG_ERROR("failed to enable async X record data transfers");
	}

	LOG_VERBOSE("event record finished");

	XFree(ranges[0]);
	XFree(ranges[1]);

	return NULL;
}

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	struct private_data priv;

	memset(&priv, 0, sizeof(struct private_data));

	g_hook_list_init(&priv.g_hook_list, sizeof(GHook));
	priv.loop = g_main_loop_new(NULL, 0);
	if (priv.loop == NULL) {
		// TODO: fprintf error with macro
		fprintf(stderr, "Cannot create main loop\n");
		exit(EXIT_FAILURE);
	}

	mis_mce_init(&priv);
	mis_vibra_init(&priv);
	priv.display = XOpenDisplay(NULL);
	if (!priv.display) {
		// TODO: fprintf error with macro
		fprintf(stderr, "Cannot open display\n");
		exit(EXIT_FAILURE);
	}

	priv.thread = g_thread_new(NULL, xrec_thread, &priv);
	if (!priv.thread) {
		// TODO: fprintf error with macro
		fprintf(stderr, "Cannot create thread\n");
		exit(EXIT_FAILURE);

	}

	static_priv = &priv;
	g_main_loop_run(priv.loop);

	return EXIT_SUCCESS;
}
