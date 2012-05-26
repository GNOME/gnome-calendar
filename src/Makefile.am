AM_CPPFLAGS =                                           \
	-DCLUTTER_VERSION_MIN_REQUIRED=CLUTTER_VERSION_1_10   \
  -DPACKAGE_LOCALE_DIR=\""$(localedir)"\"               \
  -DPACKAGE_SRC_DIR=\""$(srcdir)"\"                     \
  -DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\"                \
	-DUI_DATA_DIR=\""$(pkgdatadir)/style"\"               \
	$(MAINTAINER_COMPILER_FLAGS)                          \
	-Wall                                                 \
  -g -O0                                                \
  $(CALENDAR_CFLAGS)

AM_CFLAGS =                                             \
	-Wall                                                 \
  -g -O0

bin_PROGRAMS = gnome-calendar

gnome_calendar_SOURCES =                                \
  main.c                                                \
  gcal-application.h                                    \
  gcal-application.c                                    \
	gcal-floating-container.c                             \
	gcal-floating-container.h                             \
	gcal-toolbar.h  	                                    \
	gcal-toolbar.c       			                            \
	gcal-window.h                                         \
	gcal-window.c                                         \
	gcal-view.c                                           \
	gcal-view.h                                           \
	gcal-month-view.c                                     \
	gcal-month-view.h                                     \
	gcal-event-widget.c                                   \
	gcal-event-widget.h                                   \
	gcal-event-view.c                                     \
	gcal-event-view.h                                     \
	gcal-manager.c                                        \
	gcal-manager.h                                        \
	gcal-utils.c                                          \
	gcal-utils.h


gnome_calendar_LDFLAGS =                                \
  -Wl,--export-dynamic

gnome_calendar_LDADD =                                  \
  $(CALENDAR_LIBS)


