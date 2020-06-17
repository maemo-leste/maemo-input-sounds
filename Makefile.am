# XXX/FIXME/TODO: honour debian cflags too
CFLAGS = -Wall -Werror -Wextra -std=gnu99 -O2 -ggdb -g
# CFLAGS += $(CFLAGS) -ggdb -g

bin_PROGRAMS = maemo-xinput-sounds

maemo_xinput_sounds_CFLAGS = $(CFLAGS) $(LIBDBUS_FLAGS) $(GCONF_CFLAGS)
maemo_xinput_sounds_SOURCES = maemo-input-sounds.c
