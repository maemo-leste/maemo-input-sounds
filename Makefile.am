# XXX/FIXME/TODO: honour debian cflags too
CFLAGS = -Wall -Werror -Wextra -std=gnu99 -O2 -ggdb -g
# CFLAGS += $(CFLAGS) -ggdb -g

bin_PROGRAMS = maemo-xinput-sounds

maemo_xinput_sounds_CFLAGS = $(CFLAGS) $(LIBDBUS_CFLAGS) $(LIBDBUSGLIB_CFLAGS) $(GCONF_CFLAGS) $(X11_CFLAGS) $(XEXT_CFLAGS) $(XTEST_CFLAGS) $(PULSE_CFLAGS)
maemo_xinput_sounds_LDADD = $(LIBDBUS_LIBS) $(LIBDBUSGLIB_LIBS) $(GCONF_LIBS) $(X11_LIBS) $(XEXT_LIBS) $(XTEST_LIBS) $(PULSE_LIBS)
maemo_xinput_sounds_SOURCES = maemo-input-sounds.c

soundsdir = $(prefix)/share/sounds/maemo/stereo
sounds_DATA = x-maemo-key-pressed.wav x-maemo-touchscreen-pressed.wav
