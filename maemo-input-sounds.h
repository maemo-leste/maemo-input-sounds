#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>

#include <gconf/gconf-client.h>
#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <X11/Xlib.h>
#include <X11/extensions/record.h>


#define GCONF_SYSTEM_OSSO_DSM_VIBRA "/system/osso/dsm/vibra"
#define GCONF_SYSTEM_OSSO_DSM_VIBRA_TS_ENABLED "/system/osso/dsm/vibra/touchscreen_vibra_enabled"


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

    GConfClient *gconf_client;
    int touch_vibration_enabled;

    GHookList g_hook_list;
};



void vibration_changed_notifier(GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data);
void mis_vibra_init(struct private_data* priv);
void mis_vibra_exit(struct private_data* priv);
void mis_mce_init(struct private_data* priv);
DBusHandlerResult mis_dbus_mce_filter(DBusConnection *conn, DBusMessage *msg, void* data);
static void mis_request_state(void* data, int state);
void mis_vibra_set_state(void* data, int state);
int xerror_handler(Display* display, XErrorEvent* ev);
void xrec_data_cb(XPointer data, XRecordInterceptData* recdat);
void* xrec_thread(void* data);

