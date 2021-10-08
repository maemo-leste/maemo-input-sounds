#include "maemo-input-sounds.h"

void mis_profile_init(struct private_data *priv) {
	char *current_profile;
	char *keypad_sound_level;
	char *touchscreen_sound_level;

	if (!priv) {
		LOG_ERROR("priv == NULL");
		return;
	}

	profile_track_add_profile_cb(track_profile_cb, priv,
				     profile_null_callback);
	profile_track_add_active_cb(track_value_cb, priv,
				    profile_null_callback);

	if (profile_tracker_init() < 0) {
		LOG_ERROR("profile_tracker_init < 0");
		return;
	}

	current_profile = profile_get_profile();
	if (!current_profile) {
		LOG_ERROR("cannot get current profile");
		return;
	}

	track_profile_cb(current_profile, priv);
	keypad_sound_level =
	    profile_get_value(current_profile, "keypad.sound.level");

	if (keypad_sound_level
	    && get_volume_for_level(*keypad_sound_level,
				    &priv->volume_key_press) < 0) {
		LOG_VERBOSE("Invalid keypad value.")
	}
	LOG_VERBOSE1("got kp vol %s", keypad_sound_level);

	if (keypad_sound_level)
		g_free(keypad_sound_level);

	touchscreen_sound_level =
	    profile_get_value(current_profile, "touchscreen.sound.level");

	if (touchscreen_sound_level
	    && get_volume_for_level(*touchscreen_sound_level,
				    &priv->volume_pen_down) < 0) {
		LOG_VERBOSE("Invalid touchscreen value");
	}

	LOG_VERBOSE1("got ts vol: %s", touchscreen_sound_level);

	if (touchscreen_sound_level)
		g_free(touchscreen_sound_level);

	if (profile_has_value("touchscreen.vibration.enabled") != 0) {
		priv->touch_vibration_enabled = profile_get_value_as_bool(current_profile,
									"touchscreen.vibration.enabled");
		LOG_VERBOSE1("touchscreen.vibration.enabled is %s",
					 priv->touch_vibration_enabled ? "active" : "disabled");
	} else {
		LOG_VERBOSE1("no touchscreen.vibration.enabled in profile %s", current_profile);
		priv->touch_vibration_enabled = 0;
	}

	g_hook_list_invoke(&priv->g_hook_list, TRUE);
	g_free(current_profile);
}

void mis_profile_exit(struct private_data *priv) {
	if (priv) {
		profile_track_remove_profile_cb(track_profile_cb, priv);
		profile_track_remove_active_cb(track_value_cb, priv);
		profile_tracker_quit();
	} else {
		LOG_ERROR("priv == NULL");
	}
}

int get_volume_for_level(int value, char **dest) {
	int ret;

	switch (value) {
	case '0':
		ret = 0;
		*dest = "-60";
		break;
	case '1':
		ret = 0;
		*dest = "-25";
		break;
	case '2':
		ret = 0;
		*dest = "-5";
		break;
	default:
		ret = -1;
	}

	return ret;
}

void track_profile_cb(const char *profile, void *data) {
	struct private_data *priv = data;

	LOG_VERBOSE("track_profile_cb");

	if (priv) {
		if (profile) {
			LOG_VERBOSE1("profile = %s", profile);
			if (g_str_equal("silent", profile)) {
				priv->device_state |= MODE_PROFILE_SILENT;
			} else {
				priv->device_state &= MODE_PROFILE_SILENT_MASK;
			}

		} else {
			LOG_ERROR("profile == NULL");
		}
	} else {
		LOG_ERROR("priv == NULL");
	}
}

void track_value_cb(const char *profile, const char *key, const char *val,
		    const char *type, void *data) {

	struct private_data *priv = data;
	int found;

	if (!priv) {
		LOG_ERROR("priv == NULL");
		return;
	}
	if (!profile) {
		LOG_ERROR("profile == NULL");
		return;
	}
	if (!key) {
		LOG_ERROR("key == NULL");
		return;
	}
	if (!val) {
		LOG_ERROR("val == NULL");
		return;
	}
	if (!type) {
		LOG_ERROR("type == NULL");
		return;
	}

	LOG_VERBOSE1("profile_track_cb %s %s", key, val);

	found = g_str_equal("system.sound.level", key);

	if (!found) {
		if (g_str_equal("keypad.sound.level", key)) {
			if (get_volume_for_level(*val, &priv->volume_key_press)
			    < 0) {
				LOG_VERBOSE1("Invalid %s value", key);
			} else {
				found = 1;
			}
		} else if (g_str_equal("touchscreen.sound.level", key)) {
			if (get_volume_for_level(*val, &priv->volume_pen_down) <
			    0) {
				LOG_VERBOSE1("Invalid %s value", key);
			} else {
				found = 1;
			}
		} else if (g_str_equal("touchscreen.vibration.enabled", key)) {
			gchar* current_profile = profile_get_profile();
			priv->touch_vibration_enabled = profile_get_value_as_bool(current_profile,
									"touchscreen.vibration.enabled");
			g_free(current_profile);
		}

		if (found) {
			g_hook_list_invoke(&priv->g_hook_list, TRUE);
			return;
		} else {
			return;
		}

	}
}

void profile_null_callback(void *data) {
	(void)data;
	return;
}
