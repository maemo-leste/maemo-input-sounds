#include "maemo-input-sounds.h"

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
	int keyev, val;
	int device_state;

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
	if (diff_ms < delay_filter) {
		goto done;
	}

	priv->last_event_ts = ts;

	keyev = xrd[0];
	val = xrd[1];

	if (keyev == ButtonPress && verbose) {
		LOG_VERBOSE1("X ButtonPress %d\n", val);
	}
	if (keyev == KeyPress && verbose) {
		LOG_VERBOSE1("X KeyPress %d\n", val);
	}

	int is_button = keyev == ButtonPress;
	int is_key = keyev == KeyPress;

	device_state = priv->device_state;

	if (priv->touch_vibration_enabled && is_button && !(priv->device_state & MODE_TKLOCK_UNLOCKED)){
		/* We do this regardless of lock mode */
		mis_vibra_set_state(data, 1);
	}

	LOG_VERBOSE1("device_state = 0x%x", device_state);
	if (!device_state) {
		LOG_VERBOSE("unlocked mask set in device_state");
		if (is_key) {
			// XXX: Not sure if we want to handle these keys separately
			// (q,o,t,r)?
			if (diff_ms > 99 || (val == 111) || (val == 113)
			    || (val == 114) || (val == 116)) {
				sound_play(priv, KeyPress, diff_ms);
			}
		} else if (is_button) {
			sound_play(priv, ButtonPress, diff_ms);
		}
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
		LOG_VERBOSE("failed to open display");
		exit(EXIT_FAILURE);
	}

	XSetErrorHandler(xerror_handler);
	if (!XRecordQueryVersion(priv->display_thread, &major, &minor)) {
		LOG_ERROR("X Record Extension not available.");
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
