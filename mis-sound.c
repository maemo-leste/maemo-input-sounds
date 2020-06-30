#include "maemo-input-sounds.h"

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

	if (!priv) {
		LOG_ERROR("priv == NULL");
		return 0;
	}

	if (priv->pa_ctx_state != PA_CONTEXT_READY)
		return 0;

	if (priv->sound_not_ready) {
		sound_exit(priv);
		sound_init(priv);
		priv->sound_not_ready = 0;
	}

	if (priv->canberra_ctx) {
		if (priv->device_state)
			return 0;

		if (event_code == ButtonPress) {
			volume = priv->volume_pen_down;
			media_path = "/usr/share/sounds/ui-pen_down.wav";
			media_name = "x-maemo-touchscreen-pressed";
		} else {
			if (event_code != KeyPress)
				return 1;
			volume = priv->volume_key_press;
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
			// XXX
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
