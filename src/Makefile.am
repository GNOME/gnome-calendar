AM_CPPFLAGS =                                             \
    -DCLUTTER_VERSION_MIN_REQUIRED=CLUTTER_VERSION_1_10   \
    -DPACKAGE_LOCALE_DIR=\""$(localedir)"\"               \
    -DPACKAGE_SRC_DIR=\""$(srcdir)"\"                     \
    -DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\"                \
    -DUI_DATA_DIR=\""$(pkgdatadir)/style"\"               \
    $(MAINTAINER_COMPILER_FLAGS)                          \
    -Wall                                                 \
    -g -O0                                                \
    $(CALENDAR_CFLAGS)

AM_CFLAGS =                                               \
    -Wall                                                 \
    -g -O0

bin_PROGRAMS = gnome-calendar

BUILT_SOURCES =                                           \
    gcal-enum-types.c                                     \
    gcal-enum-types.h

gnome_calendar_SOURCES =                                  \
    $(BUILT_SOURCES)                                      \
    main.c                                                \
    gcal-application.h                                    \
    gcal-application.c                                    \
    gcal-floating-container.c                             \
    gcal-floating-container.h                             \
    gcal-toolbar.h                                        \
    gcal-toolbar.c                                        \
    gcal-window.h                                         \
    gcal-window.c                                         \
    gcal-view.c                                           \
    gcal-view.h                                           \
    gcal-month-view.c                                     \
    gcal-month-view.h                                     \
    gcal-week-view.c                                      \
    gcal-week-view.h                                      \
    gcal-event-widget.c                                   \
    gcal-event-widget.h                                   \
    gcal-event-view.c                                     \
    gcal-event-view.h                                     \
    gcal-editable.c                                       \
    gcal-editable.h                                       \
    gcal-editable-entry.c                                 \
    gcal-editable-entry.h                                 \
    gcal-editable-text.c                                  \
    gcal-editable-text.h                                  \
    gcal-editable-date.c                                  \
    gcal-editable-date.h                                  \
    gcal-editable-combo.c                                 \
    gcal-editable-combo.h                                 \
    gcal-editable-reminder.c                              \
    gcal-editable-reminder.h                              \
    gcal-manager.c                                        \
    gcal-manager.h                                        \
    gcal-utils.c                                          \
    gcal-utils.h                                          \
    gtk-notification.c                                    \
    gtk-notification.h


gnome_calendar_LDFLAGS =                                  \
  -Wl,--export-dynamic

gnome_calendar_LDADD =                                    \
  -lm                                                     \
  $(CALENDAR_LIBS)

ENUM_TYPES =                                              \
    gcal-utils.h

gcal-enum-types.h: gcal-enum-types.h.template $(ENUM_TYPES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template gcal-enum-types.h.template $(ENUM_TYPES)) > $@

gcal-enum-types.c: gcal-enum-types.c.template $(ENUM_TYPES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template gcal-enum-types.c.template $(ENUM_TYPES)) > $@

EXTRA_DIST =                                              \
    gcal-enum-types.h.template                            \
    gcal-enum-types.c.template

CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
