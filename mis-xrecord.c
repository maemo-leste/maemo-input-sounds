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

	/* Via uinput-mapper een muis er aan hangen om XRecord te testen */
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
