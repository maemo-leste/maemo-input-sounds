#include "maemo-input-sounds.h"

static struct private_data *static_priv = NULL;

int verbose = 0;
int delay_filter = 33;

void signal_handler(int signal) {
	fprintf(stderr, "Signal (%d) received, exiting\n", signal);
	if (static_priv) {
		g_main_loop_quit(static_priv->loop);
	} else {
		exit(0);
	}
}

void print_help(char *program_name, int exit_code) {
	char *progname;

	progname = basename(program_name);
	printf("usage: %s [-r] [-d device] [-f filter] [-h] [-v]\n", progname);
	// TODO [-r]
	exit(exit_code);
}

int main(int argc, char **argv) {
	struct private_data priv;

	int opt;

	char *endptr = NULL;
	char *filter = NULL;

	memset(&priv, 0, sizeof(struct private_data));

	while (1) {
		//opt = getopt_long(argc, argv, "d:f:vhr", NULL, NULL);
		opt = getopt_long(argc, argv, "d:f:vh", NULL, NULL);
		// TODO -r
		if (opt == -1)
			break;
		switch (opt) {
		case 'v':
			verbose = 1;
			break;
		case 'd':
			priv.canberra_device_name = optarg;
			break;
		case 'f':
			filter = optarg;
			break;
		case 'h':
			print_help(argv[0], 0);
			break;
		default:
			/* TODO: print help */
			print_help(argv[0], 1);
		}
	}

	if (filter) {
		strtol(filter, &endptr, 10);
		if (endptr && *endptr) {
			LOG_VERBOSE1
			    ("Invalid filter threshold %s, using the default %d.",
			     filter, delay_filter);
		}
	}

	g_hook_list_init(&priv.g_hook_list, sizeof(GHook));
	priv.loop = g_main_loop_new(NULL, 0);
	if (priv.loop == NULL) {
		// TODO: fprintf error with macro
		fprintf(stderr, "Cannot create main loop\n");
		exit(EXIT_FAILURE);
	}

	mis_policy_init(&priv);
	mis_profile_init(&priv);
	mis_pulse_init(&priv);
	mis_mce_init(&priv);

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
	sound_init(&priv);

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	static_priv = &priv;
	g_main_loop_run(priv.loop);
	sound_exit(&priv);

	if (priv.display && priv.display_thread) {
		XRecordDisableContext(priv.display, priv.recordcontext);
		XRecordFreeContext(priv.display, priv.recordcontext);
		XCloseDisplay(priv.display);
		XCloseDisplay(priv.display_thread);
		g_thread_join(priv.thread);
	}

	priv.display = NULL;
	priv.display_thread = NULL;
	priv.recordcontext = 0;
	priv.thread = NULL;

	mis_mce_exit(&priv);
	mis_pulse_exit(&priv);
	mis_profile_exit(&priv);
	mis_policy_exit(&priv);

	g_main_loop_unref(priv.loop);
	g_hook_list_clear(&priv.g_hook_list);

	return EXIT_SUCCESS;
}
