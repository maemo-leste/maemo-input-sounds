#include "config.h"

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
	pa_proplist_sets(pa_proplist, "application.name", PACKAGE);
	pa_proplist_sets(pa_proplist, "application.id",
			 "org.maemo.XInputSounds");
	pa_proplist_sets(pa_proplist, "application.version", PACKAGE_VERSION);
	priv->pa_ctx = pa_context_new_with_proplist(api, 0, pa_proplist);
	pa_proplist_free(pa_proplist);

	if (!priv->pa_ctx) {
		LOG_ERROR("Cannot create pa context");
		pa_glib_mainloop_free(pa_glib_main);
		return;
	}

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
		return;
	}

	LOG_VERBOSE("Initialised pulseaudio");
}

void mis_pulse_exit(struct private_data *priv) {
	if (!priv) {
		LOG_ERROR("priv == NULL");
		return;
	}

	if (priv->pa_ctx) {
		pa_context_unref(priv->pa_ctx);
		priv->pa_ctx = NULL;
	}
}

void context_state_callback(pa_context * pactx, struct private_data *priv) {
	if (!pactx) {
		LOG_ERROR("pactx is NULL");
	}

	priv->pa_ctx_state = pa_context_get_state(pactx);

	/* basically, anything < PA_CONTEXT_READY means we need to wait for pulse to
	 * fully init */
	if (priv->pa_ctx_state > PA_CONTEXT_SETTING_NAME) {
		if (priv->pa_ctx_state == PA_CONTEXT_READY) {
			LOG_VERBOSE("pulseaudio is ready");
		} else {
			mis_pulse_exit(priv);
			priv->sound_not_ready = 1;
			g_timeout_add_seconds(2, call_mis_pulse_init, priv);
		}
	}
}
