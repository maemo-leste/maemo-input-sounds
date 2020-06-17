#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>


#include <X11/Xlib.h>
#include <X11/extensions/record.h>


int verbose = 1; // XXX

#define LOG_ERROR(msg) fprintf(stderr, "%s:%u, %s(): " msg "\n", __FILE__, __LINE__, __FUNCTION__);
#define LOG_VERBOSE(msg) \
{ \
    if (verbose) { \
        fprintf(stderr, "%s:%u, %s(): " msg "\n", __FILE__, __LINE__, __FUNCTION__); \
    } \
}
#define LOG_VERBOSE1(fmt, ...) \
{ \
    if (verbose) { \
        fprintf(stderr, "%s:%u, %s(): " fmt "\n", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); \
    } \
}

struct private_data {
    GMainLoop* loop;
    Display* display;
    Display* display_thread;
    XRecordContext recordcontext;
    GThread* thread;

    DBusConnection* dbus_system;

    struct timespec last_event_ts;

    int touch_vibration_enabled;

    GHookList g_hook_list;
};

static struct private_data* static_priv = NULL;

void mis_vibra_init(struct private_data* priv) {
    (void)priv;
    priv->touch_vibration_enabled = 1; // XXX
}

DBusHandlerResult mis_dbus_mce_filter(DBusConnection *conn, DBusMessage *msg, void* data) {
    (void)conn;
    (void)msg;
    (void)data;
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void mis_mce_init(struct private_data* priv) {
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
    dbus_connection_add_filter(priv->dbus_system, mis_dbus_mce_filter, priv, NULL);
    dbus_bus_add_match(priv->dbus_system, "type='signal',path='/com/nokia/mce/signal',interface='com.nokia.mce.signal',member='tklock_mode_ind'", &error);
    if (dbus_error_is_set(&error)) {
        dbus_error_free(&error);
        LOG_ERROR("failed to add match for tklock");
        return;
    }

    return;

}

void mis_request_state(void* data, int state) {
    struct private_data* priv = (void*)data;
    DBusMessage *msg;

    const char* pattern = "PatternTouchscreen";

    if (!priv) {
        LOG_ERROR("priv == NULL");
        return;
    }
    if (!priv->dbus_system) {
        LOG_ERROR("priv->dbus_system == NULL");
        return;
    }

    const char* method_name = "req_vibrator_pattern_activate";
    if (!state) {
        method_name = "req_vibrator_pattern_deactivate";
    }

    msg = dbus_message_new_method_call("com.nokia.mce", "/com/nokia/mce/request", "com.nokia.mce.request", method_name);
    if (!msg) {
        LOG_ERROR("unable to create dbus message");
        return;
    }

    dbus_message_append_args(msg, DBUS_TYPE_STRING, &pattern, DBUS_TYPE_INVALID);
    dbus_connection_send(priv->dbus_system, msg, NULL);
    dbus_connection_flush(priv->dbus_system);
    dbus_message_unref(msg);

    LOG_VERBOSE1("request_state: %d", state);
}

void mis_vibra_set_state(void* data, int state) {
    struct private_data* priv = (void*)data;
    if (!priv) {
        LOG_ERROR("priv == NULL)");
    }
    if (priv->touch_vibration_enabled) {
        mis_request_state(priv, state);
    }
}


int xerror_handler(Display* display, XErrorEvent* ev) {
    (void)display;
    (void)ev;
    /* TODO */
    fprintf(stderr, "xerror_handler fired\n");

    return 0;
}

void xrec_data_cb(XPointer data, XRecordInterceptData* recdat) {
    struct private_data* priv = (void*)data;
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

    diff_ms = (1000 * (ts.tv_sec - priv->last_event_ts.tv_sec)) + (ts.tv_nsec - priv->last_event_ts.tv_nsec) / 1000000;
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

void* xrec_thread(void* data) {
    struct private_data* priv = data;
    int major, minor;
    XRecordRange* ranges[2];
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

    priv->recordcontext = XRecordCreateContext(priv->display_thread, 0, &spec, 1, ranges, 2);
    if (!priv->recordcontext) {
        LOG_ERROR("failed to create X Record Context");
        exit(1);
    }

    if (!XRecordEnableContext(priv->display_thread, priv->recordcontext, xrec_data_cb, data)) {
        LOG_ERROR("failed to enable async X record data transfers");
    }

    LOG_VERBOSE("event record finished");

    XFree(ranges[0]);
    XFree(ranges[1]);

    return NULL;
}

int main(int argc, char** argv) {
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
