bin_PROGRAMS = maemo-xinput-sounds

maemo_xinput_sounds_CFLAGS = $(LIBDBUS_CFLAGS) $(LIBDBUSGLIB_CFLAGS) \
			     $(X11_CFLAGS) $(XEXT_CFLAGS) $(XTEST_CFLAGS) \
			     $(PULSE_CFLAGS) $(CANBERRA_CFLAGS) \
			     $(PLAYBACK_CFLAGS) $(PROFILE_CFLAGS)

maemo_xinput_sounds_LDADD = $(LIBDBUS_LIBS) $(LIBDBUSGLIB_LIBS) $(X11_LIBS) \
			    $(XEXT_LIBS) $(XTEST_LIBS) $(PULSE_LIBS) \
			    $(CANBERRA_LIBS) $(PLAYBACK_LIBS) $(PROFILE_LIBS)

maemo_xinput_sounds_LDFLAGS = -Wl,--as-needed

maemo_xinput_sounds_SOURCES = maemo-input-sounds.c \
			      mis-profile.c \
			      mis-pulse.c \
			      mis-mce.c \
			      mis-vibra.c \
			      mis-sound.c \
			      mis-policy.c \
			      mis-xrecord.c

soundsdir = $(prefix)/share/sounds/maemo/stereo
sounds_DATA = x-maemo-key-pressed.wav x-maemo-touchscreen-pressed.wav
