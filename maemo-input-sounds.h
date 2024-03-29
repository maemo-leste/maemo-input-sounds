#ifndef __MAEMO_INPUT_SOUNDS_H_
#define __MAEMO_INPUT_SOUNDS_H_
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <libgen.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <pulse/pulseaudio.h>
#include <pulse/ext-stream-restore.h>
#include <pulse/glib-mainloop.h>
#include <libplayback/playback.h>
#include <canberra.h>
#include <X11/Xlib.h>
#include <X11/extensions/record.h>

#include <mce/dbus-names.h>
#include <mce/mode-names.h>

#include <libprofile.h>

#define LOG_ERROR(msg) fprintf(stderr, "%s:%u, %s(): " msg "\n", __FILE__, __LINE__, __FUNCTION__);
#define LOG_ERROR1(fmt, ...) fprintf(stderr, "%s:%u, %s(): " fmt "\n", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__);
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

// TODO: these masks need to just be bitshifted to create the bitmask
#define MODE_PROFILE_SILENT 1
#define MODE_PROFILE_SILENT_MASK 0xFFFFFFE

#define MODE_PLAYBACK_PLAY 4
#define MODE_PLAYBACK_PLAY_MASK 0xFFFFFFFB

#define MODE_TKLOCK_LOCKED 7
#define MODE_TKLOCK_LOCKED_MASK 0xFFFFFFF7

#define MODE_TKLOCK_UNLOCKED 8
#define MODE_TKLOCK_UNLOCKED_MASK 0xFFFFFFF8

struct private_data {
	GMainLoop *loop;
	Display *display;
	Display *display_thread;
	XRecordContext recordcontext;
	GThread *thread;
	ca_context *canberra_ctx;
	char *canberra_device_name;
	struct timespec last_event_ts;
	int device_state;
	DBusConnection *bus;
	pb_playback_t *playback;
	DBusConnection *dbus_system;
	pa_context *pa_ctx;
	GHook *volume_changed_hook;
	pa_context_state_t pa_ctx_state;
	int sound_not_ready;
	int touch_vibration_enabled;
	char *volume_pen_down;
	char *volume_key_press;
	GHookList g_hook_list;
};

void mis_mce_init(struct private_data *priv);
void mis_mce_exit(struct private_data *priv);
int call_mis_pulse_init(gpointer data);
void mis_pulse_init(struct private_data *priv);
void mis_pulse_exit(struct private_data *priv);
void mis_profile_init(struct private_data *priv);
void mis_profile_exit(struct private_data *priv);
void mis_policy_init(struct private_data *priv);
void mis_policy_exit(struct private_data *priv);

int sound_init(struct private_data *priv);
int sound_exit(struct private_data *priv);
int sound_play(struct private_data *priv, int event_code, signed int interval);

void signal_handler(int signal);

void policy_cmd_cb(pb_playback_t * playback, enum pb_state_e req_state,
		   pb_req_t * ext_req, void *data);
void policy_hint_cb(pb_playback_t * pb, const int *allowed_states, void *data);

void ext_stream_restore_read_cb(struct pa_context *pa_ctx,
				const struct pa_ext_stream_restore_info *info,
				int eol, void *userdata);
void ext_stream_restore_subscribe_cb(pa_context * pa_ctx, void *userdata);
void ext_stream_restore_test_cb(pa_context * pa_ctx, unsigned int version,
				void *userdata);
void volume_changed_cb(void *data);
int mis_fill_info(pa_ext_stream_restore_info * info, char *rule,
		  const char *volume_str);

int get_volume_for_level(int value, char **dest);
void track_profile_cb(const char *profile, void *data);
void track_value_cb(const char *profile, const char *key, const char *val,
		    const char *type, void *data);
void profile_null_callback(void *data);

void context_state_callback(pa_context * pactx, struct private_data *priv);
void volume_changed_cb(void *data);
DBusHandlerResult mis_dbus_mce_filter(DBusConnection * conn, DBusMessage * msg,
				      void *data);
void mis_request_state(void *data, int state);
void mis_vibra_set_state(void *data, int state);
int xerror_handler(Display * display, XErrorEvent * ev);
void xrec_data_cb(XPointer data, XRecordInterceptData * recdat);
void *xrec_thread(void *data);

extern int verbose;
extern int delay_filter;

#endif				/* __MAEMO_INPUT_SOUNDS_H_ */
