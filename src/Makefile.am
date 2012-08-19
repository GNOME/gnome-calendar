AM_CPPFLAGS =                                             \
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
    gcal-marshalers.c                                     \
    gcal-marshalers.h                                     \
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

if MAINTAINER_MODE

CLEANFILES = \
	$(BUILT_SOURCES) \
	stamp-gcal-marshalers.h \
	xgen-ac \
	xgen-ah \
	xgen-etbc \
	xgen-etbh \
	xgen-tmc \
	xgen-tmh \
	$(NULL)

gcal-enum-types.h: gcal-enum-types.h.template $(ENUM_TYPES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template gcal-enum-types.h.template $(ENUM_TYPES)) > $@

gcal-enum-types.c: gcal-enum-types.c.template $(ENUM_TYPES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template gcal-enum-types.c.template $(ENUM_TYPES)) > $@

gcal-marshalers.h: stamp-gcal-marshalers.h
	@true

stamp-gcal-marshalers.h: gcal-marshalers.list Makefile
	$(AM_V_GEN) ( \
		cd $(srcdir) \
		&& ( glib-genmarshal \
			--prefix=_gcal_marshal \
			--header gcal-marshalers.list ) >> xgen-tmh \
		&& ( cmp -s xgen-tmh gcal-marshalers.h || cp xgen-tmh gcal-marshalers.h ) \
		&& rm -f xgen-tmh \
		&& echo timestamp > $(@F) \
	)

gcal-marshalers.c: gcal-marshalers.list gcal-marshalers.h Makefile
	$(AM_V_GEN) ( \
		cd $(srcdir) \
		&& echo "#include \"gcal-marshalers.h\"" > xgen-tmc \
		&& ( glib-genmarshal \
			--prefix=_gcal_marshal \
			--body gcal-marshalers.list ) >> xgen-tmc \
		&& cp xgen-tmc gcal-marshalers.c \
		&& rm -f xgen-tmc \
	)

endif

EXTRA_DIST =                                              \
    gcal-enum-types.h.template                            \
    gcal-enum-types.c.template                            \
    gcal-marshalers.list

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
