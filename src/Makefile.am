AM_CPPFLAGS =                                             \
    -DPACKAGE_LOCALE_DIR=\""$(localedir)"\"               \
    -DPACKAGE_SRC_DIR=\""$(srcdir)"\"                     \
    -DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\"                \
    -DUI_DATA_DIR=\""$(pkgdatadir)/style"\"               \
    -I$(top_srcdir)/libgd                                 \
    $(MAINTAINER_COMPILER_FLAGS)                          \
    -Wall                                                 \
    -g -O0                                                \
    $(CALENDAR_CFLAGS)

AM_CFLAGS =                                               \
    -Wall                                                 \
    -g -O0

bin_PROGRAMS = gnome-calendar

BUILT_SOURCES =                                           \
    gcal-resources.c                                      \
    gcal-resources.h                                      \
    gcal-enum-types.c                                     \
    gcal-enum-types.h

gnome_calendar_SOURCES =                                  \
    $(BUILT_SOURCES)                                      \
    e-cell-renderer-color.c                               \
    e-cell-renderer-color.h                               \
    main.c                                                \
    gcal-application.h                                    \
    gcal-application.c                                    \
    gcal-arrow-bin.c                                      \
    gcal-arrow-bin.h                                      \
    gcal-floating-container.c                             \
    gcal-floating-container.h                             \
    gcal-window.h                                         \
    gcal-window.c                                         \
    gcal-view.c                                           \
    gcal-view.h                                           \
    gcal-year-view.c                                      \
    gcal-year-view.h                                      \
    gcal-month-view.c                                     \
    gcal-month-view.h                                     \
    gcal-week-view.c                                      \
    gcal-week-view.h                                      \
    gcal-event-widget.c                                   \
    gcal-event-widget.h                                   \
    gcal-event-overlay.c                                  \
    gcal-event-overlay.h                                  \
    gcal-edit-dialog.c                                    \
    gcal-edit-dialog.h                                    \
    gcal-time-entry.c                                     \
    gcal-time-entry.h                                     \
    gcal-date-entry.c                                     \
    gcal-date-entry.h                                     \
    gcal-manager.c                                        \
    gcal-manager.h                                        \
    gcal-nav-bar.h                                        \
    gcal-nav-bar.c                                        \
    gcal-viewport.c                                       \
    gcal-viewport.h                                       \
    gcal-all-day-grid.c                                   \
    gcal-all-day-grid.h                                   \
    gcal-utils.c                                          \
    gcal-utils.h

gnome_calendar_LDFLAGS =                                  \
  -Wl,--export-dynamic

gnome_calendar_LDADD =                                    \
  -lm                                                     \
  $(top_builddir)/libgd/libgd.la                          \
  $(CALENDAR_LIBS)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/calendar.gresource.xml)
gcal-resources.c: calendar.gresource.xml $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name calendar $(srcdir)/calendar.gresource.xml
gcal-resources.h: calendar.gresource.xml $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name calendar $(srcdir)/calendar.gresource.xml

ENUM_TYPES =                                              \
    gcal-utils.h

gcal-enum-types.h: gcal-enum-types.h.template $(ENUM_TYPES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template gcal-enum-types.h.template $(ENUM_TYPES)) > $@

gcal-enum-types.c: gcal-enum-types.c.template $(ENUM_TYPES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template gcal-enum-types.c.template $(ENUM_TYPES)) > $@

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

EXTRA_DIST =                                              \
    $(resource_files)                                     \
    gcal-enum-types.h.template                            \
    gcal-enum-types.c.template

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
