AM_CPPFLAGS =                                             \
    -DPACKAGE_LOCALE_DIR=\""$(localedir)"\"               \
    -DPACKAGE_SRC_DIR=\""$(srcdir)"\"                     \
    -DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\"                \
    -DUI_DATA_DIR=\""$(pkgdatadir)/style"\"               \
    $(MAINTAINER_COMPILER_FLAGS)                          \
    $(WARN_CFLAGS)                                        \
    -DEDS_DISABLE_DEPRECATED                              \
    -DGOA_API_IS_SUBJECT_TO_CHANGE                        \
    $(CALENDAR_CFLAGS)

bin_PROGRAMS = gnome-calendar

BUILT_SOURCES =                                           \
    $(dbus_shell_search_provider_built_sources)           \
    gcal-resources.c                                      \
    gcal-resources.h                                      \
    gcal-enum-types.c                                     \
    gcal-enum-types.h

gnome_calendar_SOURCES =                                  \
    $(BUILT_SOURCES)                                      \
    gb-scrolled-window.h                                  \
    gb-scrolled-window.c                                  \
    e-cal-data-model-subscriber.h                         \
    e-cal-data-model-subscriber.c                         \
    e-cal-data-model.h                                    \
    e-cal-data-model.c                                    \
    main.c                                                \
    css-code.h                                            \
    gcal-application.h                                    \
    gcal-application.c                                    \
    gcal-shell-search-provider.h                          \
    gcal-shell-search-provider.c                          \
    gcal-window.h                                         \
    gcal-window.c                                         \
    gcal-view.c                                           \
    gcal-view.h                                           \
    gcal-subscriber-view.c                                \
    gcal-subscriber-view.h                                \
    gcal-subscriber-view-private.h                        \
    gcal-source-dialog.c                                  \
    gcal-source-dialog.h                                  \
    gcal-month-view.c                                     \
    gcal-month-view.h                                     \
    gcal-week-view.c                                      \
    gcal-week-view.h                                      \
    gcal-year-view.c                                      \
    gcal-year-view.h                                      \
    gcal-search-view.h                                    \
    gcal-search-view.c                                    \
    gcal-event-widget.c                                   \
    gcal-event-widget.h                                   \
    gcal-edit-dialog.c                                    \
    gcal-edit-dialog.h                                    \
    gcal-time-selector.c                                  \
    gcal-time-selector.h                                  \
    gcal-date-selector.c                                  \
    gcal-date-selector.h                                  \
    gcal-manager.c                                        \
    gcal-manager.h                                        \
    gcal-utils.c                                          \
    gcal-utils.h

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

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
