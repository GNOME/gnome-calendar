SUBDIRS = icons

desktopdir = $(datadir)/applications
desktop_in_files = org.gnome.Calendar.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

searchproviderdir = $(datadir)/gnome-shell/search-providers
searchprovider_DATA = org.gnome.Calendar.search-provider.ini

%.ini: %.ini.in
	LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

@INTLTOOL_XML_RULE@
appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = $(srcdir)/appdata/org.gnome.Calendar.appdata.xml.in

@INTLTOOL_DESKTOP_RULE@

servicedir = $(datadir)/dbus-1/services
service_DATA = org.gnome.Calendar.service

%.service: %.service.in Makefile
	$(AM_V_GEN) [ -d $(@D) ] || $(mkdir_p) $(@D) ; \
	            sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@.tmp && mv $@.tmp $@

org.gnome.Calendar.service:
	$(AM_V_GEN) (echo '[D-BUS Service]'; \
	            echo 'Name=org.gnome.Calendar'; \
	            echo 'Exec=${bindir}/gnome-calendar --gapplication-service') > $@.tmp && \
	            mv $@.tmp $@

@GSETTINGS_RULES@

%.gschema.xml: %.gschema.xml.in
	$(AM_V_GEN) $(INTLTOOL_MERGE) -x -u --no-translations $< $@

gsettings_ENUM_NAMESPACE = org.gnome.calendar
gsettings_ENUM_FILES = \
	$(top_srcdir)/src/gcal-utils.h

gsettingsschema_in_files = org.gnome.calendar.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

EXTRA_DIST=                     \
  appdata/org.gnome.Calendar.appdata.xml \
  org.gnome.Calendar.search-provider.ini.in.in \
  org.gnome.Calendar.desktop    \
  calendar.gresource.xml        \
  shell-search-provider-dbus-interfaces.xml \
  ui/alarm-row.ui               \
  ui/calendar-row.ui            \
  ui/date-chooser.ui            \
  ui/date-selector.ui           \
  ui/edit-dialog.ui             \
  ui/help-overlay.ui            \
  ui/menus.ui                   \
  ui/multi-choice.ui            \
  ui/quick-add-popover.ui       \
  ui/search-view.ui             \
  ui/source-dialog.ui           \
  ui/time-selector.ui           \
  ui/week-header.ui             \
  ui/week-view.ui               \
  ui/window.ui                  \
  ui/year-view.ui               \
  theme/gtk-styles.css          \
  $(appdata_in_files)           \
  $(desktop_in_files)           \
  $(gsettingsschema_in_files)

CLEANFILES =                    \
  $(service_DATA)               \
  $(desktop_DATA)               \
  $(searchprovider_DATA)        \
  $(gsettings_SCHEMAS)          \
  $(appdata_DATA)
