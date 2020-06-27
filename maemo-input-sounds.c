#include "maemo-input-sounds.h"

static struct private_data *static_priv = NULL;

static int verbose = 0;

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

int call_mis_pulse_init(gpointer data) {
	mis_pulse_init((struct private_data *)data);
	return 0;
}

void mis_pulse_init(struct private_data *priv) {
	GMainContext *gmainctx;
	pa_glib_mainloop *pa_glib_main;

	pa_mainloop_api *api;
	pa_proplist *pa_proplist;

	if (!priv) {
		LOG_ERROR("priv is null");
		return;
	}

	if (priv->pa_ctx) {
		LOG_ERROR("priv->pa_ctx already set");
		return;
	}

	gmainctx = g_main_context_default();
	pa_glib_main = pa_glib_mainloop_new(gmainctx);
	if (!pa_glib_main) {
		LOG_ERROR("Unable to create pa_glib_mainloop");
		return;
	}

	api = pa_glib_mainloop_get_api(pa_glib_main);
	if (!api) {
		LOG_ERROR("Cannot get pa glib mainloop api");
		return;
	}

	pa_proplist = pa_proplist_new();
	pa_proplist_sets(pa_proplist, "application.name", "maemo-input-sounds");
	pa_proplist_sets(pa_proplist, "application.id",
			 "org.maemo.XInputSounds");
	pa_proplist_sets(pa_proplist, "application.version", "0.7");
	priv->pa_ctx = pa_context_new_with_proplist(api, 0, pa_proplist);
	if (priv->pa_ctx) {
		pa_proplist_free(pa_proplist);
		pa_context_set_state_callback(priv->pa_ctx,
					      (pa_context_notify_cb_t) &
					      context_state_callback,
					      (void *)priv);

		if (pa_context_connect
		    (priv->pa_ctx, NULL, PA_CONTEXT_NOAUTOSPAWN, 0) < 0) {
			if (verbose)
				LOG_ERROR("Connection failed");
			pa_context_unref(priv->pa_ctx);
			priv->pa_ctx = NULL;
			pa_glib_mainloop_free(pa_glib_main);
		}

		priv->volume_changed_hook = g_hook_alloc(&priv->g_hook_list);

		if (priv->volume_changed_hook) {
			priv->volume_changed_hook->data = (gpointer) priv;
			priv->volume_changed_hook->func = volume_changed_cb;
			g_hook_insert_before(&priv->g_hook_list,
					     0, priv->volume_changed_hook);
			LOG_VERBOSE("Initialised pulseaudio");
			return;
		}

		LOG_ERROR("Unable to set volume_changed_hook");
	}
}

void mis_pulse_exit(struct private_data *priv) {
	if (!priv) {
		LOG_ERROR("priv == NULL");
	}

	if (priv->pa_ctx) {
		pa_context_unref(priv->pa_ctx);
		priv->pa_ctx = NULL;
	}

	if (priv->volume_changed_hook) {
		g_hook_destroy_link(&priv->g_hook_list,
				    priv->volume_changed_hook);
		priv->volume_changed_hook = NULL;
	}
}

void mis_profile_init(struct private_data *priv) {
	(void)priv;
}

void mis_profile_exit(struct private_data *priv) {
	(void)priv;
}

void mis_policy_init(struct private_data *priv) {
	(void)priv;
}

void mis_policy_exit(struct private_data *priv) {
	(void)priv;
}

int sound_init(struct private_data *priv) {
	char *display_name;
	int ret;

	if (ca_context_create(&priv->canberra_ctx)) {
		LOG_ERROR("failed to create canberra context");
		exit(1);
	}

	display_name = XDisplayName(0);
	ret = ca_context_change_props(priv->canberra_ctx,
				      "application.name",
				      "maemo-input-sounds",
				      "application.id",
				      "org.maemo.XInputSounds",
				      "window.x11.screen",
				      display_name,
				      "media.language",
				      "en_EN",
				      "canberra.cache-control",
				      "permanent", NULL);

	if (ret)
		LOG_VERBOSE1("failed to change canberra properties: %s",
			     ca_strerror(ret));

	ret = ca_context_set_driver(priv->canberra_ctx, "pulse");
	if (ret) {
		LOG_ERROR1("failed to select canberra PulseAudio driver: %s",
			   ca_strerror(ret));
		exit(1);
	}

	ret = ca_context_open(priv->canberra_ctx);
	if (ret) {
		LOG_ERROR1("failed to open canberra context: %s",
			   ca_strerror(ret));
	}

	if (priv->canberra_device_name) {
		ret =
		    ca_context_change_device(priv->canberra_ctx,
					     priv->canberra_device_name);
		if (ret) {
			LOG_VERBOSE1("failed to reroute context to %s",
				     ca_strerror(ret));
		} else {
			LOG_VERBOSE1("sound rerouted to %s",
				     priv->canberra_device_name);
		}
	}
	return ret;
}

int sound_exit(struct private_data *priv) {
	int ret = 0;

	if (priv->canberra_ctx) {
		ret = ca_context_destroy(priv->canberra_ctx);
		priv->canberra_ctx = NULL;
	}

	return ret;
}

int sound_play(struct private_data *priv, int event_code, signed int interval) {
	int result;
	char *volume;
	const char *media_path;
	const char *media_name;
	int play_failed;
	int vol;
	char *s = alloca(sizeof(char) * 16);

	if (priv->pa_ctx_state != PA_CONTEXT_READY)
		return 0;

	if (priv->sound_not_ready) {
		sound_exit(priv);
		sound_init(priv);
		priv->sound_not_ready = 0;
	}
	if (priv->canberra_ctx) {
		// TODO: policy_state / profile state
#if 0
		if (*priv->policy_state_maybe)
			return 0;
#endif

		if (event_code == ButtonPress) {
			volume = "0";
			//volume = priv->volume_pen_down;
			media_path = "/usr/share/sounds/ui-pen_down.wav";
			media_name = "x-maemo-touchscreen-pressed";
		} else {
			if (event_code != KeyPress)
				return 1;
			//volume = priv->volume_key_press;
			volume = "0";
			media_path = "/usr/share/sounds/ui-key_press.wav";
			media_name = "x-maemo-key-pressed";
		}

		if (!volume)
			volume = "-25";

		if (event_code == KeyPress && interval <= 100) {
			vol = strtol(volume, NULL, 10);
			volume = s;
			snprintf(s, 10, "%d", vol - 30);

#if 0
			if (!flag_record_maybe)
				return 0;
#endif
		}

		LOG_VERBOSE1("vol %s, interval %d", volume, interval);

		if (g_str_equal(volume, "-60"))
			return 0;

		play_failed = ca_context_play(priv->canberra_ctx,
					      0,
					      "media.filename",
					      media_path,
					      "media.name",
					      media_name,
					      "canberra.volume",
					      volume,
					      "module-stream-restore.id",
					      media_name, NULL);

		if (play_failed)
			LOG_VERBOSE1("failed to play sound %s (%s)", media_path,
				     ca_strerror(play_failed));

		result = play_failed == 0;
	} else {
		LOG_ERROR("priv->canberra_ctx == NULL");
		result = 0;
	}
	return result;
}

void signal_handler(int signal) {
	fprintf(stderr, "Signal (%d) received, exiting\n", signal);
	if (static_priv) {
		g_main_loop_quit(static_priv->loop);
	} else {
		exit(0);
	}
}

void volume_changed_cb(void *data) {
	struct private_data *priv = (void *)data;
	(void)priv;
}

void ext_stream_restore_read_cb(struct pa_context *pa_ctx,
				const struct pa_ext_stream_restore_info *info,
				int eol, void *userdata) {
	(void)info;
	(void)userdata;
	if (eol < 0)
		LOG_VERBOSE1
		    ("Failed to initialize stream_restore extension: %s",
		     pa_strerror(pa_context_errno(pa_ctx)));
}

void ext_stream_restore_subscribe_cb(pa_context * pa_ctx, void *userdata) {
	struct private_data *priv = userdata;
	(void)pa_ctx;
	pa_operation *operation;

	//operation = pa_ext_stream_restore2_read(*priv->pa_ctx,
	//                               ext_stream_restore_read_cb, priv);
	// XXX: pa_ext_stream_restore2_read is maemo specific - why?
	operation = pa_ext_stream_restore_read(priv->pa_ctx,
					       ext_stream_restore_read_cb,
					       priv);
	if (operation) {
		pa_operation_unref(operation);
	} else
		LOG_VERBOSE("pa_ext_stream_restore2_read failed");
}

void ext_stream_restore_test_cb(pa_context * pa_ctx, unsigned int version,
				void *userdata) {
	struct private_data *priv = userdata;
	pa_operation *pa_operation;

	LOG_VERBOSE1("ext-stream-restore-version: %u", version);
	//pa_operation = pa_ext_stream_restore2_read(pa_ctx, ext_stream_restore_read_cb, priv);
	// XXX: pa_ext_stream_restore2_read is maemo specific - why?
	pa_operation =
	    pa_ext_stream_restore_read(pa_ctx, ext_stream_restore_read_cb,
				       priv);
	if (pa_operation) {
		pa_operation_unref(pa_operation);
		pa_ext_stream_restore_set_subscribe_cb(pa_ctx,
						       ext_stream_restore_subscribe_cb,
						       priv);

		pa_operation =
		    pa_ext_stream_restore_subscribe(pa_ctx, 1, 0, NULL);
		if (pa_operation)
			pa_operation_unref(pa_operation);
	}
	volume_changed_cb(priv);
}

void context_state_callback(pa_context * pactx, struct private_data *priv) {
	pa_context_state_t pa_ctx_state;
	pa_operation *pa_operation;

	if (!pactx) {
		LOG_ERROR("pactx is NULL");
	}

	priv->pa_ctx_state = pa_context_get_state(pactx);
	pa_ctx_state = pa_context_get_state(pactx);
	if (pa_ctx_state > 3) {
		if (pa_ctx_state == PA_CONTEXT_READY) {
			pa_operation =
			    pa_ext_stream_restore_test(pactx,
						       ext_stream_restore_test_cb,
						       priv);

			if (pa_operation) {
				pa_operation_unref(pa_operation);
			} else
				LOG_VERBOSE1
				    ("Failed to initialized stream_restore extension: %s",
				     pa_strerror(pa_context_errno(pactx)));

		} else {
			mis_pulse_exit(priv);
			priv->sound_not_ready = 1;
			g_timeout_add_seconds(2, call_mis_pulse_init, priv);
		}
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

DBusHandlerResult mis_dbus_mce_filter(DBusConnection * conn,
				      DBusMessage * msg, void *data) {
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
	    (1000 * (ts.tv_sec - priv->last_event_ts.tv_sec)) +
	    (ts.tv_nsec - priv->last_event_ts.tv_nsec)
	    / 1000000;
	if (diff_ms < 33) {
		goto done;
	}

	priv->last_event_ts = ts;

	if (xrd[0] == ButtonPress && verbose) {
		LOG_VERBOSE1("X ButtonPress %d\n", xrd[1]);
		sound_play(priv, ButtonPress, diff_ms);
	}
	if (xrd[0] == MotionNotify && verbose) {
		LOG_VERBOSE1("X MotionNotify %d\n", xrd[1]);
	}
	if (xrd[0] == KeyPress && verbose) {
		LOG_VERBOSE1("X KeyPress %d\n", xrd[1]);
		sound_play(priv, KeyPress, diff_ms);
	}
	// if sounds enabled, or this is not a button, play sounds
	//if (priv->policy_state & 0xFFFFFF8) || !is_button)

	if (priv->touch_vibration_enabled && xrd[0] == ButtonPress) {
		mis_vibra_set_state(data, 1);
	} else if (priv->touch_vibration_enabled && xrd[0] == MotionNotify) {
		mis_vibra_set_state(data, 1);
	}

 done:
	XRecordFreeData(recdat);
	return;
}

void *xrec_thread(void *data) {
	struct private_data *priv = data;
	int major, minor;
	XRecordRange *ranges[3];
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
	ranges[2] = XRecordAllocRange();

	if (!ranges[0] || !ranges[1] || !ranges[2]) {
		LOG_ERROR("failed to allocate X Record Range");
	}

	ranges[0]->device_events.first = KeyPress;
	ranges[0]->device_events.last = KeyPress;
	ranges[1]->device_events.first = ButtonPress;
	ranges[1]->device_events.last = ButtonPress;
	ranges[2]->device_events.first = MotionNotify;
	ranges[2]->device_events.last = MotionNotify;
	spec = XRecordAllClients;

	priv->recordcontext =
	    XRecordCreateContext(priv->display_thread, 0, &spec, 1, ranges, 3);
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
	XFree(ranges[2]);

	return NULL;
}

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	struct private_data priv;

	struct option options;
	int opt;

	memset(&priv, 0, sizeof(struct private_data));

	while (1) {
		//opt = getopt_long(argc, argv, "d:f:vhr", &options, NULL);
		opt = getopt_long(argc, argv, "d:v", &options, NULL);
		if (opt == -1)
			break;
		switch (opt) {
		case 'v':
			verbose = 1;
			break;
		case 'd':
			priv.canberra_device_name = optarg;
			break;
		default:
			/* TODO: print help */
			LOG_ERROR("Invalid option");
			return 1;
		}
	}

	g_hook_list_init(&priv.g_hook_list, sizeof(GHook));
	priv.loop = g_main_loop_new(NULL, 0);
	if (priv.loop == NULL) {
		// TODO: fprintf error with macro
		fprintf(stderr, "Cannot create main loop\n");
		exit(EXIT_FAILURE);
	}

	mis_policy_init(&priv);
	mis_profile_init(&priv);
	mis_pulse_init(&priv);
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
	sound_init(&priv);

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	static_priv = &priv;
	g_main_loop_run(priv.loop);
	sound_exit(&priv);

	if (priv.display && priv.display_thread) {
		XRecordDisableContext(priv.display, priv.recordcontext);
		XRecordFreeContext(priv.display, priv.recordcontext);
		XCloseDisplay(priv.display);
		XCloseDisplay(priv.display_thread);
		g_thread_join(priv.thread);
	}

	priv.display = NULL;
	priv.display_thread = NULL;
	priv.recordcontext = 0;
	priv.thread = NULL;

	mis_vibra_exit(&priv);
	mis_mce_exit(&priv);
	mis_pulse_exit(&priv);
	mis_profile_exit(&priv);
	mis_policy_exit(&priv);

	g_main_loop_unref(priv.loop);
	g_hook_list_clear(&priv.g_hook_list);

	return EXIT_SUCCESS;
}
