#include "maemo-input-sounds.h"

void mis_policy_init(struct private_data *priv) {
	int bus_name;
	DBusError error;

	dbus_error_init(&error);
	priv->bus = dbus_bus_get(0, &error);
	if (!priv->bus) {
		if (dbus_error_is_set(&error)) {
			LOG_ERROR1("Failed to get DBUS connection (%s)",
				   error.message);
			exit(1);
		}
		LOG_ERROR("Failed to get DBUS connection");
		exit(1);
	}

	dbus_connection_setup_with_g_main(priv->bus, NULL);
	bus_name =
	    dbus_bus_request_name(priv->bus, "org.maemo.XInputSounds",
				  DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);

	if (bus_name == DBUS_REQUEST_NAME_REPLY_EXISTS) {
		LOG_ERROR
		    ("DBUS Name already taken, another instance already running");
		exit(0);
	}

	if (bus_name != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		if (dbus_error_is_set(&error)) {
			LOG_ERROR1("Failed to acquire bus name (%s)",
				   error.message);
			dbus_error_free(&error);
		} else {
			LOG_ERROR("Failed to acquire bus name");
		}
	}
	LOG_VERBOSE("Connected to DBUS");
	priv->playback =
	    pb_playback_new_2(priv->bus, PB_CLASS_INPUT, PB_FLAG_AUDIO,
			      PB_STATE_STOP, policy_cmd_cb, priv);
	if (!priv->playback) {
		LOG_ERROR("Failed to initialise policy/libplayback interface");
		exit(1);
	}

	pb_playback_set_state_hint(priv->playback, policy_hint_cb, priv);
	priv->device_state &= MODE_PLAYBACK_PLAY_MASK;
}

void mis_policy_exit(struct private_data *priv) {
	if (priv->playback) {
		pb_playback_destroy(priv->playback);
		priv->playback = NULL;
	}
}

void policy_cmd_cb(pb_playback_t * playback, enum pb_state_e req_state,
		   pb_req_t * ext_req, void *data) {
	(void)playback;

	struct private_data *priv = data;

	switch (req_state) {
	case PB_STATE_STOP:
		LOG_VERBOSE("Received policy STOP command");
		break;
	case PB_STATE_PLAY:
		LOG_VERBOSE("Received policy PLAY command");
		break;
	default:
		LOG_VERBOSE1("Received unknown policy command 0x%x", req_state);
	}
	pb_playback_req_completed(priv->playback, ext_req);
	return;
}

void policy_hint_cb(pb_playback_t * pb, const int *allowed_states, void *data) {
	(void)pb;

	struct private_data *priv = data;

	if (allowed_states[PB_STATE_PLAY]) {
		priv->device_state &= MODE_PLAYBACK_PLAY_MASK;
		LOG_VERBOSE("sounds enabled by policy");
	} else {
		priv->device_state |= MODE_PLAYBACK_PLAY;
		LOG_VERBOSE("sounds disabled by policy");
	}
	return;
}
