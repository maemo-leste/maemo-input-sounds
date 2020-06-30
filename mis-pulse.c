#include "maemo-input-sounds.h"

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
		} else {
			LOG_ERROR("Unable to set volume_changed_hook");
		}
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
	pa_operation *pa_operation;

	if (!pactx) {
		LOG_ERROR("pactx is NULL");
	}

	priv->pa_ctx_state = pa_context_get_state(pactx);

	/* basically, anything < PA_CONTEXT_READY means we need to wait for pulse to
	 * fully init */
	if (priv->pa_ctx_state > PA_CONTEXT_SETTING_NAME) {
		if (priv->pa_ctx_state == PA_CONTEXT_READY) {
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

void volume_changed_cb(void *data) {
	struct private_data *priv = (void *)data;
	pa_operation *pa_op;
	pa_ext_stream_restore_info key_info;
	pa_ext_stream_restore_info ts_info;
	pa_ext_stream_restore_info *pa_restore_info[2];
	//pa_ext_stream_restore2_info key_info;
	//pa_ext_stream_restore2_info ts_info;
	//pa_ext_stream_restore2_info *pa_restore_info[2];

	if (!priv) {
		LOG_ERROR("priv == NULL");
		return;
	}

	LOG_VERBOSE("Volume changed, updating rules");

	pa_restore_info[0] = &key_info;
	pa_restore_info[1] = &ts_info;

	if ((mis_fill_info
	     (&key_info, "x-maemo-key-pressed", priv->volume_key_press) < 0)
	    ||
	    (mis_fill_info
	     (&ts_info, "x-maemo-touchscreen-pressed",
	      priv->volume_pen_down) < 0)) {
		LOG_VERBOSE("Cannot fill volume from stream-restore");
		return;
	}
	//pa_op = pa_ext_stream_restore2_write
	pa_op = pa_ext_stream_restore_write(priv->pa_ctx, PA_UPDATE_REPLACE,
					    *pa_restore_info, 2, 1, NULL, NULL);
	if (pa_op)
		pa_operation_unref(pa_op);
	else
		LOG_VERBOSE("pa_ext_stream_restore_write() failed");

}

int mis_fill_info(pa_ext_stream_restore_info * info, char *rule,
		  const char *volume_str) {
	double volume;
	pa_volume_t pa_vol_sw;
	int mute;
	pa_cvolume pa_volume;

	if (!info) {
		LOG_ERROR("info == NULL");
		return -1;
	}

	if (!rule) {
		LOG_ERROR("rule == NULL");
		return -1;
	}

	if (!volume_str)
		volume_str = "-25";

	errno = 0;
	volume = strtod(volume_str, NULL);
	if (errno != 0) {
		LOG_VERBOSE("Invalid volume");
		return -1;
	} else {
		pa_cvolume_init(&pa_volume);
		pa_vol_sw = pa_sw_volume_from_dB(volume);
		pa_cvolume_set(&pa_volume, 1u, pa_vol_sw);
		info->channel_map.channels = 1;
		info->name = rule;
		info->channel_map.map[0] = 0;
		memcpy(&info->volume, &pa_volume, sizeof(pa_cvolume));
		mute = info->mute & 0xFE;
		info->device = NULL;
		info->mute = mute | 2;
	}

	return 0;
}
