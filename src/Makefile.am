AM_CPPFLAGS =                                             \
    -I$(srcdir)/views                                     \
    -DPACKAGE_LOCALE_DIR=\""$(localedir)"\"               \
    -DPACKAGE_SRC_DIR=\""$(srcdir)"\"                     \
    -DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\"                \
    -DUI_DATA_DIR=\""$(pkgdatadir)/style"\"               \
    $(WARN_CFLAGS)                                        \
    -Wno-sign-compare                                     \
    -DEDS_DISABLE_DEPRECATED                              \
    -DGOA_API_IS_SUBJECT_TO_CHANGE                        \
    $(CALENDAR_CFLAGS)

AM_LDFLAGS = \
	$(WARN_LDFLAGS)

bin_PROGRAMS = gnome-calendar

BUILT_SOURCES =                                           \
    $(dbus_shell_search_provider_built_sources)           \
    gcal-resources.c                                      \
    gcal-resources.h                                      \
    gcal-enum-types.c                                     \
    gcal-enum-types.h

gnome_calendar_SOURCES =                                  \
    $(BUILT_SOURCES)                                      \
    e-cal-data-model-subscriber.h                         \
    e-cal-data-model-subscriber.c                         \
    e-cal-data-model.h                                    \
    e-cal-data-model.c                                    \
    main.c                                                \
    css-code.h                                            \
    views/gcal-month-view.c                               \
    views/gcal-month-view.h                               \
    views/gcal-range-tree.c                               \
    views/gcal-range-tree.h                               \
    views/gcal-week-grid.c                                \
    views/gcal-week-grid.h                                \
    views/gcal-week-header.c                              \
    views/gcal-week-header.h                              \
    views/gcal-week-view.c                                \
    views/gcal-week-view.h                                \
    views/gcal-year-view.c                                \
    views/gcal-year-view.h                                \
    gcal-application.c                                    \
    gcal-application.h                                    \
    gcal-clock.c                                          \
    gcal-clock.h                                          \
    gcal-date-chooser-day.c                               \
    gcal-date-chooser-day.h                               \
    gcal-date-chooser.c                                   \
    gcal-date-chooser.h                                   \
    gcal-date-selector.c                                  \
    gcal-date-selector.h                                  \
    gcal-debug.h                                          \
    gcal-edit-dialog.c                                    \
    gcal-edit-dialog.h                                    \
    gcal-event.c                                          \
    gcal-event.h                                          \
    gcal-event-widget.c                                   \
    gcal-event-widget.h                                   \
    gcal-log.c                                            \
    gcal-log.h                                            \
    gcal-manager.c                                        \
    gcal-manager.h                                        \
    gcal-multi-choice.c                                   \
    gcal-multi-choice.h                                   \
    gcal-quick-add-popover.c                              \
    gcal-quick-add-popover.h                              \
    gcal-search-view.h                                    \
    gcal-search-view.c                                    \
    gcal-shell-search-provider.h                          \
    gcal-shell-search-provider.c                          \
    gcal-sidebar.c                                        \
    gcal-sidebar.h                                        \
    gcal-source-dialog.c                                  \
    gcal-source-dialog.h                                  \
    gcal-subscriber-view.c                                \
    gcal-subscriber-view.h                                \
    gcal-subscriber-view-private.h                        \
    gcal-time-selector.c                                  \
    gcal-time-selector.h                                  \
    gcal-utils.c                                          \
    gcal-utils.h                                          \
    gcal-view.c                                           \
    gcal-view.h                                           \
    gcal-window.h                                         \
    gcal-window.c

gnome_calendar_LDFLAGS =                                  \
  -Wl,--export-dynamic

gnome_calendar_LDADD =                                    \
  -lm                                                     \
  $(CALENDAR_LIBS)

resource_files = $(shell glib-compile-resources --sourcedir=$(top_srcdir)/data --generate-dependencies $(top_srcdir)/data/calendar.gresource.xml)
gcal-resources.c: $(top_srcdir)/data/calendar.gresource.xml $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/data --generate-source --c-name calendar $(top_srcdir)/data/calendar.gresource.xml
gcal-resources.h: $(top_srcdir)/data/calendar.gresource.xml $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/data --generate-header --c-name calendar $(top_srcdir)/data/calendar.gresource.xml

ENUM_TYPES =                                              \
    gcal-utils.h

gcal-enum-types.h: gcal-enum-types.h.template $(ENUM_TYPES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template gcal-enum-types.h.template $(ENUM_TYPES)) > $@

gcal-enum-types.c: gcal-enum-types.c.template $(ENUM_TYPES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template gcal-enum-types.c.template $(ENUM_TYPES)) > $@

dbus_shell_search_provider_built_sources =		\
	gcal-shell-search-provider-generated.c	\
	gcal-shell-search-provider-generated.h

$(dbus_shell_search_provider_built_sources) : Makefile.am $(top_srcdir)/data/shell-search-provider-dbus-interfaces.xml
	gdbus-codegen									\
		--interface-prefix org.gnome.						\
		--c-namespace Gcal							\
		--generate-c-code gcal-shell-search-provider-generated		\
		$(top_srcdir)/data/shell-search-provider-dbus-interfaces.xml

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

EXTRA_DIST =                                              \
    $(resource_files)                                     \
    gcal-enum-types.h.template                            \
    gcal-enum-types.c.template


#---------------------------------------------
# Library for GTK-Doc
#---------------------------------------------
noinst_LTLIBRARIES = libgcal.la

libgcal_la_SOURCES = $(gnome_calendar_SOURCES)

libgcal_la_CFLAGS = \
	$(CALENDAR_CFLAGS) \
	$(CALENDAR_WARN_CFLAGS)

libgcal_la_LDFLAGS = \
	-Wl,--export-dynamic \
	-lm


dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
