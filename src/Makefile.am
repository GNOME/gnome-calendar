AM_CPPFLAGS =                               \
	-DCLUTTER_VERSION_MIN_REQUIRED=CLUTTER_VERSION_1_10 \
  -DPACKAGE_LOCALE_DIR=\""$(localedir)"\"   \
  -DPACKAGE_SRC_DIR=\""$(srcdir)"\"         \
  -DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\"    \
	$(MAINTAINER_COMPILER_FLAGS)              \
  $(CALENDAR_CFLAGS)

AM_CFLAGS =                                 \
	-Wall -Wextra                             \
  -g -O0

bin_PROGRAMS = gnome-calendar

gnome_calendar_SOURCES =                    \
  main.c                                    \
  gcal-application.h                        \
  gcal-application.c                        \
	gcal-main-toolbar.h                       \
	gcal-main-toolbar.c                       \
	gcal-window.h                             \
	gcal-window.c


gnome_calendar_LDFLAGS =                    \
  -Wl,--export-dynamic

gnome_calendar_LDADD =                      \
  $(CALENDAR_LIBS)


